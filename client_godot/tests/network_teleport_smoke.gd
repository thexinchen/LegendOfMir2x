extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_POSITION := Vector2i(416, 179)
const TARGET_MAP_ID := 1
const TARGET_POSITION := Vector2i(371, 335)
const TELEPORT_COST := 500

enum Stage {
	WAIT_WORLD,
	WAIT_DIALOG,
	WAIT_TELEPORT,
}

var _main: Control
var _stage := Stage.WAIT_WORLD
var _target_uid := 0
var _gold_received := false
var _gold_before := 0
var _clicked := false
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
			_enter_teleport_npc()
		Stage.WAIT_DIALOG:
			_select_destination()
		Stage.WAIT_TELEPORT:
			_finish_teleport()


func _enter_teleport_npc() -> void:
	if not _gold_received:
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
		_fail("failed to enter the six-sided stone", 7)
		return
	_stage = Stage.WAIT_DIALOG


func _select_destination() -> void:
	if _clicked:
		return
	var xml := _current_dialog_xml()
	if xml.is_empty():
		return
	if not xml.contains("比奇城") or not xml.contains("金币500"):
		_fail("teleport dialog is missing the original destination or price: %s" % xml, 8)
		return
	var panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if panel == null or not panel.visible:
		return
	var event_id := _event_id_by_text(xml, "比奇城")
	if event_id.is_empty():
		_fail("teleport dialog did not expose the visible destination event", 9)
		return
	if OS.has_environment("MIR2X_NETWORK_TELEPORT_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_TELEPORT_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the teleport screenshot: %s" % error, 10)
			return
	_clicked = true
	panel.call("_on_meta_clicked", JSON.stringify({"id": event_id, "path": "", "args": null, "close": true}))
	_stage = Stage.WAIT_TELEPORT


func _finish_teleport() -> void:
	if GameState.player_map_id != TARGET_MAP_ID:
		return
	if Vector2i(GameState.player_x, GameState.player_y) != TARGET_POSITION:
		return
	if GameState.player_gold != _gold_before - TELEPORT_COST:
		return
	var panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if panel != null and panel.visible:
		_fail("close=1 teleport event left the NPC dialog visible", 11)
		return
	print("NETWORK TELEPORT PASS: map=24->%d pos=%s gold=%d->%d cost=%d panel=closed" % [TARGET_MAP_ID, TARGET_POSITION, _gold_before, GameState.player_gold, TELEPORT_COST])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _current_dialog_xml() -> String:
	if int(GameState.npc_dialog.get("npcUID", 0)) != _target_uid:
		return ""
	return str(GameState.npc_dialog.get("xmlLayout", ""))


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
		NetworkClient.SM_GOLD:
			_gold_received = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d uid=%d map=%d pos=(%d,%d) gold=%d/%d dialog=%s" % [_stage, _target_uid, GameState.player_map_id, GameState.player_x, GameState.player_y, _gold_before, GameState.player_gold, GameState.npc_dialog], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_TELEPORT_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
