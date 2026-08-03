extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_POSITION := Vector2i(426, 117)
const OP_REPAIR := 3
const FIXTURE_ITEM_ID := 826
const FIXTURE_SEQ_ID := 5

enum Stage {
	WAIT_WORLD,
	WAIT_ENTRY,
	WAIT_INV_OP,
	WAIT_QUERY,
	WAIT_RESULT,
}

var _main: Control
var _stage := Stage.WAIT_WORLD
var _target_uid := 0
var _inventory_received := false
var _gold_received := false
var _selection_requested := false
var _last_clicked_xml := ""
var _gold_before := 0
var _duration_before := 0
var _duration_max := 0
var _expect_repair := true
var _repair_cost := 0
var _commit_msec := 0
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
			_open_repair_operation()
		Stage.WAIT_INV_OP:
			_select_repair_item()
		Stage.WAIT_QUERY:
			_commit_repair()
		Stage.WAIT_RESULT:
			_finish_repair()


func _start_npc_entry() -> void:
	if not _inventory_received or not _gold_received:
		return
	var item := _fixture_item()
	if item.is_empty():
		_fail("dedicated account does not contain the wooden-sword fixture", 7)
		return
	var duration: Array = item.get("duration", [])
	if duration.size() < 2 or int(duration[1]) <= 0 or int(duration[0]) > int(duration[1]):
		_fail("fixture must be a valid durable item, got duration=%s" % duration, 8)
		return
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 3 and Vector2i(creature.get("x", -1), creature.get("y", -1)) == TARGET_NPC_POSITION:
			_target_uid = int(uid_value)
			break
	if _target_uid == 0:
		return
	_gold_before = GameState.player_gold
	_duration_before = int(duration[0])
	_duration_max = int(duration[1])
	_expect_repair = _duration_before < _duration_max
	if NetworkClient.send_npc_event(_target_uid, "", SYS_ENTER) != OK:
		_fail("failed to enter the blacksmith", 9)
		return
	_stage = Stage.WAIT_ENTRY


func _open_repair_operation() -> void:
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if xml.contains("购买") and xml.contains("修理"):
		if _click_event_by_text_once(xml, "修理"):
			_stage = Stage.WAIT_INV_OP
	elif xml.contains("随便聊聊"):
		_click_event_by_text_once(xml, "随便聊聊")


func _select_repair_item() -> void:
	var operation: Dictionary = GameState.inventory_operation
	if int(operation.get("invOp", 0)) != OP_REPAIR or int(operation.get("uid", 0)) != _target_uid:
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	var key := "%d:%d" % [FIXTURE_ITEM_ID, FIXTURE_SEQ_ID]
	if _selection_requested:
		if inventory_panel.get("_selected_key") != key:
			return
		if (inventory_panel.get_node("Title") as Label).text != "【请选择修理物品】" or not inventory_panel.get_node("OperationButton").visible:
			_fail("repair operation visuals did not match the original inventory mode", 10)
			return
		_stage = Stage.WAIT_QUERY
		return
	var resources: RefCounted = inventory_panel.get("_resources")
	if not operation.get("typeList", []).has(resources.call("item_type", FIXTURE_ITEM_ID)):
		_fail("wooden-sword fixture is not accepted by the server repair type list", 11)
		return
	var bin: Dictionary = inventory_panel.get("_bins").get(key, {})
	if bin.is_empty():
		return
	var expected_position := Vector2(int(bin.x) * 38, (int(bin.y) - int(inventory_panel.get("_scroll_row"))) * 38)
	for child in inventory_panel.get_node("ItemGrid").get_children():
		if child is TextureButton and child.position == expected_position:
			_emit_left_click(child)
			_selection_requested = true
			return


func _commit_repair() -> void:
	if not _expect_repair:
		if not _current_dialog_xml().contains("不需要修理"):
			return
		if not GameState.inventory_operation_cost.is_empty() or GameState.player_gold != _gold_before:
			_fail("full-durability item produced a quote or changed gold", 17)
			return
		print("NETWORK REPAIR PASS: item=%d:%d already full at %d/%d, no quote and no charge" % [FIXTURE_ITEM_ID, FIXTURE_SEQ_ID, _duration_before, _duration_max])
		_finished = true
		NetworkClient.disconnect_from_server()
		get_tree().quit()
		return
	var cost: Dictionary = GameState.inventory_operation_cost
	if int(cost.get("invOp", 0)) != OP_REPAIR or int(cost.get("itemID", 0)) != FIXTURE_ITEM_ID or int(cost.get("seqID", 0)) != FIXTURE_SEQ_ID:
		return
	_repair_cost = int(cost.get("cost", 0))
	if _repair_cost < 100 or _repair_cost > 200:
		_fail("repair quote is outside the script contract: %d" % _repair_cost, 12)
		return
	var xml := _current_dialog_xml()
	if not xml.contains(str(_repair_cost)):
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	if not inventory_panel.get_node("OperationCost").visible:
		_fail("repair quote did not expose the original cost label", 13)
		return
	if OS.has_environment("MIR2X_NETWORK_REPAIR_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_REPAIR_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the repair screenshot: %s" % error, 14)
			return
	(inventory_panel.get_node("OperationButton") as TextureButton).pressed.emit()
	_commit_msec = Time.get_ticks_msec()
	_stage = Stage.WAIT_RESULT


func _finish_repair() -> void:
	var item := _fixture_item()
	var duration: Array = item.get("duration", [])
	if duration.size() >= 2 and int(duration[0]) == _duration_max and GameState.player_gold == _gold_before - _repair_cost:
		var xml := _current_dialog_xml()
		if not xml.contains("已经修理完毕"):
			return
		print("NETWORK REPAIR PASS: item=%d:%d duration=%d/%d->%d/%d gold=%d->%d cost=%d" % [FIXTURE_ITEM_ID, FIXTURE_SEQ_ID, _duration_before, _duration_max, duration[0], duration[1], _gold_before, GameState.player_gold, _repair_cost])
		_finished = true
		NetworkClient.disconnect_from_server()
		get_tree().quit()
		return
	if _current_dialog_xml().contains("已经修理完毕") and Time.get_ticks_msec() - _commit_msec > 1000:
		_fail("server acknowledged repair without updating durability and gold", 15)


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
	_fail("timed out stage=%d uid=%d item=%s gold=%d/%d cost=%d dialog=%s operation=%s" % [_stage, _target_uid, _fixture_item(), _gold_before, GameState.player_gold, _repair_cost, GameState.npc_dialog, GameState.inventory_operation], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_REPAIR_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
