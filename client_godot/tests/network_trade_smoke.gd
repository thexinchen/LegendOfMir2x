extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_POSITION := Vector2i(426, 117)
const OP_TRADE := 1
const FIXTURE_ITEM_ID := 826
const FIXTURE_SEQ_ID := 5
const FIXTURE_KEY := "826:5"
const MAX_QUOTE_ATTEMPTS := 8

enum Stage {
	WAIT_WORLD,
	WAIT_ENTRY,
	WAIT_INV_OP,
	WAIT_QUERY,
	WAIT_RESULT,
	WAIT_REPLAY,
}

var _main: Control
var _stage := Stage.WAIT_WORLD
var _target_uid := 0
var _inventory_received := false
var _gold_received := false
var _selection_requested := false
var _last_clicked_xml := ""
var _gold_before := 0
var _quote := 0
var _quote_attempts := 0
var _waiting_requery := false
var _saw_requery_clear := false
var _commit_msec := 0
var _commit_tag := ""
var _settled_gold := 0
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(30.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	match _stage:
		Stage.WAIT_WORLD:
			_start_npc_entry()
		Stage.WAIT_ENTRY:
			_open_trade_operation()
		Stage.WAIT_INV_OP:
			_select_trade_item()
		Stage.WAIT_QUERY:
			_commit_trade()
		Stage.WAIT_RESULT:
			_finish_trade()
		Stage.WAIT_REPLAY:
			_finish_replay()


func _start_npc_entry() -> void:
	if not _inventory_received or not _gold_received:
		return
	if _fixture_item().is_empty():
		_fail("dedicated account does not contain the wooden-sword fixture", 7)
		return
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 3 and Vector2i(creature.get("x", -1), creature.get("y", -1)) == TARGET_NPC_POSITION:
			_target_uid = int(uid_value)
			break
	if _target_uid == 0:
		return
	_gold_before = GameState.player_gold
	if NetworkClient.send_npc_event(_target_uid, "", SYS_ENTER) != OK:
		_fail("failed to enter the blacksmith", 8)
		return
	_stage = Stage.WAIT_ENTRY


func _open_trade_operation() -> void:
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if xml.contains("购买") and xml.contains("出售"):
		if _click_event_by_text_once(xml, "出售"):
			_stage = Stage.WAIT_INV_OP
	elif xml.contains("随便聊聊"):
		_click_event_by_text_once(xml, "随便聊聊")


func _select_trade_item() -> void:
	var operation: Dictionary = GameState.inventory_operation
	if int(operation.get("invOp", 0)) != OP_TRADE or int(operation.get("uid", 0)) != _target_uid:
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	if _selection_requested:
		if inventory_panel.get("_selected_key") != FIXTURE_KEY:
			return
		if (inventory_panel.get_node("Title") as Label).text != "【请选择出售物品】" or not inventory_panel.get_node("OperationButton").visible:
			_fail("trade operation visuals did not match the original inventory mode", 9)
			return
		_stage = Stage.WAIT_QUERY
		return
	var resources: RefCounted = inventory_panel.get("_resources")
	if not operation.get("typeList", []).has(resources.call("item_type", FIXTURE_ITEM_ID)):
		_fail("wooden-sword fixture is not accepted by the server trade type list", 10)
		return
	if _emit_fixture_click(inventory_panel):
		_selection_requested = true


func _commit_trade() -> void:
	var cost: Dictionary = GameState.inventory_operation_cost
	if _waiting_requery:
		if cost.is_empty():
			_saw_requery_clear = true
			return
		if not _saw_requery_clear:
			return
		_waiting_requery = false
		_saw_requery_clear = false
	if int(cost.get("invOp", 0)) != OP_TRADE or int(cost.get("itemID", 0)) != FIXTURE_ITEM_ID or int(cost.get("seqID", 0)) != FIXTURE_SEQ_ID:
		return
	_quote = int(cost.get("cost", 0))
	if _quote < 100 or _quote > 200:
		_fail("trade quote is outside the script contract: %d" % _quote, 11)
		return
	if _quote == 200:
		if _quote_attempts >= MAX_QUOTE_ATTEMPTS:
			_fail("could not obtain a non-legacy quote after %d attempts" % _quote_attempts, 12)
			return
		var inventory_panel := _main.get_node("InventoryPanel") as Control
		if not _emit_fixture_click(inventory_panel):
			return
		_waiting_requery = true
		return
	var xml := _current_dialog_xml()
	if not xml.contains(str(_quote)):
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	if not inventory_panel.get_node("OperationCost").visible:
		_fail("trade quote did not expose the original cost label", 13)
		return
	if OS.has_environment("MIR2X_NETWORK_TRADE_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_TRADE_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the trade screenshot: %s" % error, 14)
			return
	_commit_tag = str(GameState.inventory_operation.get("commitTag", ""))
	if _commit_tag.is_empty():
		_fail("trade operation did not retain a commit tag", 18)
		return
	(inventory_panel.get_node("OperationButton") as TextureButton).pressed.emit()
	_commit_msec = Time.get_ticks_msec()
	_stage = Stage.WAIT_RESULT


func _finish_trade() -> void:
	if not _fixture_item().is_empty():
		return
	var paid := GameState.player_gold - _gold_before
	var xml := _current_dialog_xml()
	if paid == _quote and xml.contains(str(_quote)) and xml.contains("成交"):
		_settled_gold = GameState.player_gold
		if NetworkClient.send_npc_event(_target_uid, "", _commit_tag, "%d:%d" % [FIXTURE_ITEM_ID, FIXTURE_SEQ_ID]) != OK:
			_fail("failed to replay the consumed trade commit", 19)
			return
		_stage = Stage.WAIT_REPLAY
		return
	if Time.get_ticks_msec() - _commit_msec > 500:
		_fail("server settled trade for %d after quoting %d, dialog=%s" % [paid, _quote, xml], 15)


func _finish_replay() -> void:
	var xml := _current_dialog_xml()
	if not xml.contains("出售请求已经失效"):
		return
	if not _fixture_item().is_empty() or GameState.player_gold != _settled_gold:
		_fail("replayed trade changed inventory or gold", 20)
		return
	print("NETWORK TRADE PASS: item=%d:%d removed gold=%d->%d quote=%d attempts=%d replay=rejected" % [FIXTURE_ITEM_ID, FIXTURE_SEQ_ID, _gold_before, _settled_gold, _quote, _quote_attempts])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _emit_fixture_click(inventory_panel: Control) -> bool:
	var bin: Dictionary = inventory_panel.get("_bins").get(FIXTURE_KEY, {})
	if bin.is_empty():
		return false
	var expected_position := Vector2(int(bin.x) * 38, (int(bin.y) - int(inventory_panel.get("_scroll_row"))) * 38)
	for child in inventory_panel.get_node("ItemGrid").get_children():
		if child is TextureButton and child.position == expected_position:
			_emit_left_click(child)
			_quote_attempts += 1
			return true
	return false


func _fixture_item() -> Dictionary:
	for item_value in GameState.inventory:
		var item: Dictionary = item_value
		if int(item.get("itemID", 0)) == FIXTURE_ITEM_ID and int(item.get("seqID", 0)) == FIXTURE_SEQ_ID:
			return item
	return {}


func _current_dialog_xml() -> String:
	if int(GameState.npc_dialog.get("npcUID", 0)) != _target_uid:
		return ""
	return str(GameState.npc_dialog.get("xmlLayout", ""))


func _click_event_by_text_once(xml: String, expected_text: String) -> bool:
	if xml == _last_clicked_xml:
		return false
	var event_id := _event_id_by_text(xml, expected_text)
	if event_id.is_empty():
		_fail("NPC dialog did not expose visible event: %s" % expected_text, 16)
		return false
	var panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if panel == null or not panel.visible:
		return false
	_last_clicked_xml = xml
	panel.call("_on_meta_clicked", JSON.stringify({"id": event_id, "path": "", "args": null, "close": false}))
	return true


func _event_id_by_text(xml: String, expected_text: String) -> String:
	var parser := XMLParser.new()
	if parser.open_buffer(xml.to_utf8_buffer()) != OK:
		return ""
	var event_id := ""
	while parser.read() == OK:
		if parser.get_node_type() == XMLParser.NODE_ELEMENT and parser.get_node_name().to_lower() == "event":
			event_id = ""
			for index in parser.get_attribute_count():
				if parser.get_attribute_name(index).to_lower() == "id":
					event_id = parser.get_attribute_value(index)
		elif parser.get_node_type() == XMLParser.NODE_TEXT and not event_id.is_empty() and parser.get_node_data().contains(expected_text):
			return event_id
		elif parser.get_node_type() == XMLParser.NODE_ELEMENT_END and parser.get_node_name().to_lower() == "event":
			event_id = ""
	return ""


func _emit_left_click(control: Control) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = true
	control.gui_input.emit(event)


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("id_4", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 3)
		NetworkClient.SM_ONLINEOK:
			var online := Protocol.decode_sm_online_ok(payload)
			var action: Dictionary = online.get("action", {})
			GameState.set_player_online({
				"uid": online.get("uid", 0), "name": online.get("name", ""),
				"gender": online.get("gender", 0), "job": online.get("job", 0),
				"map_uid": online.get("mapUID", 0), "x": action.get("x", 0),
				"y": action.get("y", 0), "direction": action.get("direction", 0),
			})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_INVENTORY:
			_inventory_received = true
		NetworkClient.SM_GOLD:
			_gold_received = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d uid=%d item=%s gold=%d/%d quote=%d attempts=%d dialog=%s operation=%s" % [_stage, _target_uid, _fixture_item(), _gold_before, GameState.player_gold, _quote, _quote_attempts, GameState.npc_dialog, GameState.inventory_operation], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_TRADE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
