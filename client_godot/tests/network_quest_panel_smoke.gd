extends Node


const QUEST_PANEL_PATH := "res://scenes/game/panels/quest.tscn"

var _main: Control
var _quest_list_received := false
var _finished := false
var _initial_reset_serial := 0


func _ready() -> void:
	_initial_reset_serial = GameState.quest_reset_serial
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(20.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null or not _quest_list_received:
		return
	if GameState.quest_reset_serial <= _initial_reset_serial:
		return
	var panel := _main.call("_ensure_extra_panel", QUEST_PANEL_PATH) as Control
	if panel == null:
		_fail("quest panel could not be instantiated", 7)
		return
	if not _main.call("_try_panel_hotkey", KEY_Q) or not panel.visible:
		_fail("Q did not open the quest panel after the real quest list", 8)
		return
	var expected_position := Vector2(floorf(_main.size.x * 0.5) - 145.0, floorf(_main.size.y * 0.5) - 223.0)
	if panel.position != expected_position:
		_fail("quest panel did not use the C++ center anchor: actual=%s expected=%s" % [panel.position, expected_position], 9)
		return
	var label_count := 0
	for child in panel.get_node("ContentViewport/Lines").get_children():
		if child is Label:
			label_count += 1
	if label_count != GameState.quests.size():
		_fail("real quest headings do not match GameState: labels=%d quests=%d" % [label_count, GameState.quests.size()], 10)
		return
	if OS.has_environment("MIR2X_NETWORK_QUEST_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_QUEST_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the real quest panel screenshot: %s" % error, 11)
			return
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	Input.parse_input_event(escape)
	await get_tree().process_frame
	if panel.visible:
		_fail("Escape did not close the real quest panel", 12)
		return
	print("NETWORK QUEST PANEL PASS: quests=%d reset_serial=%d position=%s" % [GameState.quests.size(), GameState.quest_reset_serial, expected_position])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 3)
		NetworkClient.SM_ONLINEOK:
			var online := preload("res://scripts/network/protocol.gd").decode_sm_online_ok(payload)
			var action: Dictionary = online.get("action", {})
			GameState.set_player_online({
				"uid": online.get("uid", 0),
				"name": online.get("name", ""),
				"gender": online.get("gender", 0),
				"job": online.get("job", 0),
				"map_uid": online.get("mapUID", 0),
				"x": action.get("x", 0),
				"y": action.get("y", 0),
				"direction": action.get("direction", 0),
			})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_QUESTDESPLIST:
			_quest_list_received = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out online=%s quest_list=%s reset=%d quests=%s" % [_main != null, _quest_list_received, GameState.quest_reset_serial, GameState.quests], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_QUEST_PANEL_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
