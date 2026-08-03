extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const SECURED_PANEL_PATH := "res://scenes/game/panels/secured_items.tscn"
const INPUT_PANEL_PATH := "res://scenes/game/panels/input_string.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_POSITION := Vector2i(396, 116)
const OP_SECURE := 2

enum Stage {
	WAIT_WORLD,
	WAIT_ENTRY,
	WAIT_INV_OP,
	WAIT_QUERY,
	WAIT_STORED,
	WAIT_HOME,
	WAIT_SECURED,
	WAIT_RESTORED,
	WAIT_PASSWORD_HOME,
	WAIT_PASSWORD_FIRST,
	WAIT_PASSWORD_SECOND,
	WAIT_PASSWORD_RESULT,
}

var _main: Control
var _stage := Stage.WAIT_WORLD
var _target_uid := 0
var _inventory_received := false
var _gold_received := false
var _item: Dictionary = {}
var _item_key := ""
var _initial_count := 0
var _initial_gold := 0
var _selection_requested := false
var _secured_selection_requested := false
var _last_clicked_xml := ""
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
			_open_storage_operation()
		Stage.WAIT_INV_OP:
			_select_storage_item()
		Stage.WAIT_QUERY:
			_commit_storage()
		Stage.WAIT_STORED:
			_return_to_home()
		Stage.WAIT_HOME:
			_open_secured_list()
		Stage.WAIT_SECURED:
			_retrieve_stored_item()
		Stage.WAIT_RESTORED:
			_finish_restored_cycle()
		Stage.WAIT_PASSWORD_HOME:
			_open_password_input()
		Stage.WAIT_PASSWORD_FIRST:
			_submit_password("mir2x-storage-smoke", false)
		Stage.WAIT_PASSWORD_SECOND:
			_submit_password("mir2x-storage-smoke", true)
		Stage.WAIT_PASSWORD_RESULT:
			_finish_password_cycle()


func _start_npc_entry() -> void:
	if not _inventory_received or not _gold_received or GameState.inventory.is_empty():
		return
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 3 and Vector2i(creature.get("x", -1), creature.get("y", -1)) == TARGET_NPC_POSITION:
			_target_uid = int(uid_value)
			break
	if _target_uid == 0:
		return
	_initial_gold = GameState.player_gold
	if NetworkClient.send_npc_event(_target_uid, "", SYS_ENTER) != OK:
		_fail("failed to enter the warehouse keeper", 7)
		return
	_stage = Stage.WAIT_ENTRY


func _open_storage_operation() -> void:
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if xml.contains("仓库管理员") and xml.contains("寄存"):
		if _click_event_by_text_once(xml, "寄存"):
			_stage = Stage.WAIT_INV_OP
	elif xml.contains("随便聊聊"):
		_click_event_by_text_once(xml, "随便聊聊")


func _select_storage_item() -> void:
	var operation: Dictionary = GameState.inventory_operation
	if int(operation.get("invOp", 0)) != OP_SECURE or int(operation.get("uid", 0)) != _target_uid:
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	if _selection_requested:
		if inventory_panel.get("_selected_key") != _item_key:
			return
		if (inventory_panel.get_node("Title") as Label).text != "【请选择存储物品】" or not inventory_panel.get_node("OperationButton").visible:
			_fail("storage operation visuals did not match the original inventory mode", 9)
			return
		_stage = Stage.WAIT_QUERY
		return
	var allowed: Array = operation.get("typeList", [])
	var resources: RefCounted = inventory_panel.get("_resources")
	for preferred_seq in [1, 0]:
		for item_value in GameState.inventory:
			var candidate: Dictionary = item_value
			var seq_id := int(candidate.get("seqID", 0))
			if seq_id > 0 and (preferred_seq == 0 or seq_id == preferred_seq) and allowed.has(resources.call("item_type", int(candidate.get("itemID", 0)))):
				_item = candidate.duplicate(true)
				break
		if not _item.is_empty():
			break
	if _item.is_empty():
		_fail("no storable inventory item was available", 8)
		return
	_item_key = "%d:%d" % [_item.get("itemID", 0), _item.get("seqID", 0)]
	_initial_count = _item_count(int(_item.itemID), int(_item.seqID))
	var bin: Dictionary = inventory_panel.get("_bins").get(_item_key, {})
	if bin.is_empty():
		return
	var expected_position := Vector2(int(bin.x) * 38, (int(bin.y) - int(inventory_panel.get("_scroll_row"))) * 38)
	for child in inventory_panel.get_node("ItemGrid").get_children():
		if child is TextureButton and child.position == expected_position:
			_emit_left_click(child)
			_selection_requested = true
			return


func _commit_storage() -> void:
	var xml := _current_dialog_xml()
	if not xml.contains("20金币"):
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	if inventory_panel.get_node("OperationCost").visible:
		_fail("warehouse script unexpectedly exposed a cost packet absent from the C++ flow", 10)
		return
	(inventory_panel.get_node("OperationButton") as TextureButton).pressed.emit()
	_stage = Stage.WAIT_STORED


func _return_to_home() -> void:
	if _item_count(int(_item.itemID), int(_item.seqID)) != 0 or GameState.player_gold != _initial_gold:
		return
	var xml := _current_dialog_xml()
	if not xml.contains("已经放好了"):
		return
	if _click_event_by_text_once(xml, "前一步"):
		_stage = Stage.WAIT_HOME


func _open_secured_list() -> void:
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if xml.contains("仓库管理员") and xml.contains("取回"):
		if _click_event_by_text_once(xml, "取回"):
			_stage = Stage.WAIT_SECURED
	elif xml.contains("随便聊聊"):
		_click_event_by_text_once(xml, "随便聊聊")


func _retrieve_stored_item() -> void:
	var secured_index := _secured_item_index()
	if secured_index < 0:
		return
	var panel := _main.get("_extra_panel_nodes").get(SECURED_PANEL_PATH) as Control
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	var npc_panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if panel == null or not panel.visible or not inventory_panel.visible:
		return
	var expected_y := minf(npc_panel.size.y if npc_panel != null and npc_panel.visible else 0.0, _main.size.y - panel.size.y)
	if panel.position != Vector2(0, expected_y) or inventory_panel.position != Vector2(_main.size.x - inventory_panel.size.x, 0):
		_fail("secured/inventory panel choreography diverged from the C++ layout", 11)
		return
	if _secured_selection_requested:
		if int(panel.get("_selected_index")) != secured_index:
			return
		_stage = Stage.WAIT_RESTORED
		if OS.has_environment("MIR2X_NETWORK_STORAGE_SCREENSHOT"):
			await RenderingServer.frame_post_draw
			var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_STORAGE_SCREENSHOT"))
			if error != OK:
				_fail("failed to save the storage screenshot: %s" % error, 13)
				return
		(panel.get_node("SelectButton") as TextureButton).pressed.emit()
		return
	var target_page := floori(float(secured_index) / 12.0)
	while int(panel.get("_page")) < target_page:
		(panel.get_node("RightButton") as TextureButton).pressed.emit()
	var page_slot := secured_index % 12
	var cell := panel.get_node("ItemGrid").get_child(page_slot) as TextureButton
	cell.pressed.emit()
	_secured_selection_requested = true


func _finish_restored_cycle() -> void:
	if _item_count(int(_item.itemID), int(_item.seqID)) != _initial_count or _secured_item_index() >= 0:
		return
	if GameState.player_gold != _initial_gold:
		_fail("warehouse round trip changed gold despite the current server not charging", 14)
		return
	var xml := _current_dialog_xml()
	if not xml.contains("你想找什么"):
		return
	if _click_event_by_text_once(xml, "前一步"):
		_stage = Stage.WAIT_PASSWORD_HOME


func _open_password_input() -> void:
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if xml.contains("仓库管理员") and xml.contains("设置"):
		if _click_event_by_text_once(xml, "设置"):
			_stage = Stage.WAIT_PASSWORD_FIRST
	elif xml.contains("随便聊聊"):
		_click_event_by_text_once(xml, "随便聊聊")


func _submit_password(value: String, confirmation: bool) -> void:
	var pending: Dictionary = GameState.pending_input
	if int(pending.get("uid", 0)) != _target_uid or str(pending.get("commitTag", "")).is_empty():
		return
	var panel := _main.get("_extra_panel_nodes").get(INPUT_PANEL_PATH) as Control
	if panel == null or not panel.visible:
		return
	var title := (panel.get_node("Title") as Label).text
	if (confirmation and not title.contains("确认密码")) or (not confirmation and not title.contains("请输入密码")):
		_fail("password input title did not match the current server step: %s" % title, 16)
		return
	var input := panel.get_node("ValueClip/Value") as LineEdit
	if not input.secret:
		_fail("warehouse password input was not masked", 17)
		return
	input.text = value
	(panel.get_node("ConfirmButton") as TextureButton).pressed.emit()
	_stage = Stage.WAIT_PASSWORD_RESULT if confirmation else Stage.WAIT_PASSWORD_SECOND


func _finish_password_cycle() -> void:
	var xml := _current_dialog_xml()
	if not xml.contains("设置密码成功"):
		return
	if not GameState.pending_input.is_empty():
		_fail("password input state was not cleared after confirmation", 18)
		return
	print("NETWORK STORAGE PASS: item=%d:%d count=%d gold=%d panels=restored password=confirmed" % [_item.itemID, _item.seqID, _initial_count, _initial_gold])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _current_dialog_xml() -> String:
	if int(GameState.npc_dialog.get("npcUID", 0)) != _target_uid:
		return ""
	return str(GameState.npc_dialog.get("xmlLayout", ""))


func _click_event_by_text_once(xml: String, expected_text: String) -> bool:
	if xml == _last_clicked_xml:
		return false
	var event_id := _event_id_by_text(xml, expected_text)
	if event_id.is_empty():
		_fail("NPC dialog did not expose visible event: %s" % expected_text, 15)
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


func _item_count(item_id: int, seq_id: int) -> int:
	for item_value in GameState.inventory:
		var inventory_item: Dictionary = item_value
		if int(inventory_item.get("itemID", 0)) == item_id and int(inventory_item.get("seqID", 0)) == seq_id:
			return int(inventory_item.get("count", 0))
	return 0


func _secured_item_index() -> int:
	for index in GameState.secured_items.size():
		var secured_item: Dictionary = GameState.secured_items[index]
		if int(secured_item.get("itemID", 0)) == int(_item.get("itemID", -1)) and int(secured_item.get("seqID", 0)) == int(_item.get("seqID", -1)):
			return index
	return -1


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
	_fail("timed out stage=%d uid=%d item=%s inv_count=%d secured=%s dialog=%s operation=%s" % [_stage, _target_uid, _item_key, _item_count(int(_item.get("itemID", 0)), int(_item.get("seqID", 0))), GameState.secured_items, GameState.npc_dialog, GameState.inventory_operation], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_STORAGE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
