extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const PURCHASE_PANEL_PATH := "res://scenes/game/panels/purchase.tscn"
const INPUT_PANEL_PATH := "res://scenes/game/panels/input_string.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_POSITION := Vector2i(402, 122)

enum Stage {
	WAIT_WORLD,
	WAIT_ENTRY,
	WAIT_SELL,
	WAIT_DETAIL,
	WAIT_RESULT,
}

var _main: Control
var _stage := Stage.WAIT_WORLD
var _target_uid := 0
var _quest_wrapper_clicked := false
var _inventory_received := false
var _gold_received := false
var _candidate_ids: Array[int] = []
var _candidate_cursor := 0
var _item_id := 0
var _item_name := ""
var _price := 0
var _gold_before := 0
var _count_before := 0
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(25.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	match _stage:
		Stage.WAIT_WORLD:
			_start_npc_entry()
		Stage.WAIT_ENTRY:
			_open_purchase()
		Stage.WAIT_SELL:
			_select_packable_candidate()
		Stage.WAIT_DETAIL:
			_open_quantity_input()
		Stage.WAIT_RESULT:
			_finish_purchase()


func _start_npc_entry() -> void:
	if not _inventory_received or not _gold_received or GameState.inventory.is_empty() or GameState.player_gold <= 0:
		return
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 3 and Vector2i(creature.get("x", -1), creature.get("y", -1)) == TARGET_NPC_POSITION:
			_target_uid = int(uid_value)
			break
	if _target_uid == 0:
		return
	if NetworkClient.send_npc_event(_target_uid, "", SYS_ENTER) != OK:
		_fail("failed to enter the item-display merchant", 7)
		return
	_stage = Stage.WAIT_ENTRY


func _open_purchase() -> void:
	var dialog: Dictionary = GameState.npc_dialog
	var xml := str(dialog.get("xmlLayout", ""))
	if int(dialog.get("npcUID", 0)) != _target_uid:
		return
	if not xml.contains("所有的物品"):
		if _quest_wrapper_clicked or not xml.contains("随便聊聊"):
			return
		var continue_id := _event_id_by_text(xml, "随便聊聊")
		if continue_id.is_empty():
			_fail("quest wrapper did not expose the original NPC entry", 8)
			return
		if _click_npc_event(continue_id):
			_quest_wrapper_clicked = true
		return
	var event_id := _event_id_by_text(xml, "购买")
	if event_id.is_empty():
		_fail("merchant entry did not contain the purchase event", 8)
		return
	if _click_npc_event(event_id):
		_stage = Stage.WAIT_SELL


func _click_npc_event(event_id: String) -> bool:
	var npc_panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if npc_panel == null or not npc_panel.visible:
		return false
	npc_panel.call("_on_meta_clicked", JSON.stringify({"id": event_id, "path": "", "args": null, "close": false}))
	return true


func _select_packable_candidate() -> void:
	if int(GameState.npc_sell.get("npcUID", 0)) != _target_uid:
		return
	var panel := _main.get("_extra_panel_nodes").get(PURCHASE_PANEL_PATH) as Control
	if panel == null or not panel.visible:
		return
	if _candidate_ids.is_empty():
		var resources: RefCounted = panel.get("_resources")
		for item_value in GameState.npc_sell.get("itemList", []):
			var candidate := int(item_value)
			var count := _item_total(candidate)
			if resources.call("item_is_packable", candidate) and count > 0 and count < 99:
				_candidate_ids.append(candidate)
	if _candidate_ids.is_empty():
		_fail("no existing non-full packable item is sold by the test merchant", 9)
		return
	_query_candidate(panel)


func _query_candidate(panel: Control) -> void:
	if _candidate_cursor >= _candidate_ids.size():
		_fail("no affordable gold-priced packable item was found", 10)
		return
	_item_id = _candidate_ids[_candidate_cursor]
	var items: Array = GameState.npc_sell.get("itemList", [])
	panel.set("_selected_index", items.find(_item_id))
	panel.call("_query_selected")
	_stage = Stage.WAIT_DETAIL


func _open_quantity_input() -> void:
	var detail: Dictionary = GameState.npc_sell_detail
	var list: Array = detail.get("list", [])
	if int(detail.get("npcUID", 0)) != _target_uid or list.is_empty():
		return
	var item: Dictionary = list[0].get("item", {})
	if int(item.get("itemID", 0)) != _item_id:
		return
	var purchase_panel := _main.get("_extra_panel_nodes").get(PURCHASE_PANEL_PATH) as Control
	_price = int(purchase_panel.call("_gold_price", list[0]))
	if _price <= 0 or _price > GameState.player_gold:
		_candidate_cursor += 1
		GameState.npc_sell_detail = {}
		_query_candidate(purchase_panel)
		return
	var resources: RefCounted = purchase_panel.get("_resources")
	_item_name = str(resources.call("item_name", _item_id))
	var quantity_button: TextureButton
	for child in purchase_panel.get_node("Detail").get_children():
		if child is TextureButton and child.position == Vector2(366, 60):
			quantity_button = child
			break
	if quantity_button == null:
		return
	_gold_before = GameState.player_gold
	_count_before = _item_total(_item_id)
	quantity_button.pressed.emit()
	var input_panel := _main.get("_extra_panel_nodes").get(INPUT_PANEL_PATH) as Control
	if input_panel == null or not input_panel.visible or not (input_panel.get_node("Title") as Label).text.contains(_item_name):
		_fail("packable purchase did not open the original quantity dialog", 11)
		return
	(input_panel.get_node("ValueClip/Value") as LineEdit).text = "1"
	(input_panel.get_node("ConfirmButton") as TextureButton).pressed.emit()
	if input_panel.visible or not _main.get("_pending_purchase").is_empty():
		_fail("quantity confirmation did not close and release purchase ownership", 12)
		return
	_stage = Stage.WAIT_RESULT


func _finish_purchase() -> void:
	if GameState.player_gold != _gold_before - _price or _item_total(_item_id) != _count_before + 1:
		return
	if OS.has_environment("MIR2X_NETWORK_PURCHASE_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_PURCHASE_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the real purchase screenshot: %s" % error, 13)
			return
	print("NETWORK PURCHASE PASS: item=%s(%d) count=%d->%d gold=%d->%d price=%d" % [_item_name, _item_id, _count_before, _item_total(_item_id), _gold_before, GameState.player_gold, _price])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _item_total(item_id: int) -> int:
	var total := 0
	for item_value in GameState.inventory:
		var item: Dictionary = item_value
		if int(item.get("itemID", 0)) == item_id:
			total += int(item.get("count", 0))
	return total


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


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("id_3", "123456") != OK:
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
	_fail("timed out stage=%d uid=%d item=%d gold=%d/%d count=%d/%d dialog=%s sell=%s detail=%s" % [_stage, _target_uid, _item_id, _gold_before, GameState.player_gold, _count_before, _item_total(_item_id), GameState.npc_dialog, GameState.npc_sell, GameState.npc_sell_detail], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_PURCHASE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
