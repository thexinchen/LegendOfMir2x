extends Node

const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const Protocol = preload("res://scripts/network/protocol.gd")
const RUNTIME_PANEL_PATH := "res://scenes/game/panels/runtime_config.tscn"

var _role := ""
var _main: Control
var _config_seen := false
var _started := false
var _finished := false


func _ready() -> void:
	_role = OS.get_environment("MIR2X_RUNTIME_CONFIG_ROLE")
	if _role not in ["writer", "reader"]:
		_fail("invalid role: %s" % _role, 2)
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(15.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _started or not _config_seen or _main == null:
		return
	_started = true
	if _role == "writer":
		_write_from_actual_panel()
	else:
		_verify_fresh_login()


func _write_from_actual_panel() -> void:
	var panel := _main.call("_ensure_extra_panel", RUNTIME_PANEL_PATH) as Control
	panel.show()
	await get_tree().process_frame
	var show_fps := panel.get_node("SystemPage/ShowFPS") as Button
	if show_fps.button_pressed:
		_fail("writer fixture expected default ShowFPS=false", 7)
		return
	show_fps.button_pressed = true
	(panel.get_node("Menu/Social") as Button).pressed.emit()
	await get_tree().process_frame
	(panel.get_node("SystemPage/Tab1") as Button).pressed.emit()
	await get_tree().process_frame
	var radios: Array = panel.get("_friend_radios")
	if radios.size() != 3 or not (radios[2] as Button).button_pressed:
		_fail("writer fixture expected default manual friend policy", 8)
		return
	(radios[1] as Button).button_pressed = true
	if not _decode_bool(GameState.runtime_config.get(6, PackedByteArray()), false):
		_fail("actual ShowFPS control did not update local state", 9)
		return
	if _decode_int(GameState.runtime_config.get(48, PackedByteArray()), 2) != 1:
		_fail("actual friend-policy control did not update local state", 10)
		return
	await get_tree().create_timer(1.0).timeout
	_pass("writer")


func _verify_fresh_login() -> void:
	if not _decode_bool(GameState.runtime_config.get(6, PackedByteArray()), false):
		_fail("fresh login did not restore ShowFPS=true", 11)
		return
	if _decode_int(GameState.runtime_config.get(48, PackedByteArray()), 2) != 1:
		_fail("fresh login did not restore reject-all friend policy", 12)
		return
	await get_tree().process_frame
	if not (_main.get_node("FPS") as Label).visible:
		_fail("restored ShowFPS state did not affect the live world", 13)
		return
	var panel := _main.call("_ensure_extra_panel", RUNTIME_PANEL_PATH) as Control
	panel.show()
	await get_tree().process_frame
	if not (panel.get_node("SystemPage/ShowFPS") as Button).button_pressed:
		_fail("restored ShowFPS state did not populate the panel", 14)
		return
	(panel.get_node("Menu/Social") as Button).pressed.emit()
	await get_tree().process_frame
	(panel.get_node("SystemPage/Tab1") as Button).pressed.emit()
	await get_tree().process_frame
	var radios: Array = panel.get("_friend_radios")
	if radios.size() != 3 or not (radios[1] as Button).button_pressed:
		_fail("restored friend policy did not populate the panel", 15)
		return
	_pass("reader")


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("good", "123456") != OK:
		_fail("failed to send login", 3)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 4)
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
		NetworkClient.SM_PLAYERCONFIG:
			var reader := CerealReader.new(payload)
			reader.read_sd_player_config()
			if not reader.valid or not reader.at_end():
				_fail("invalid player-config payload: %s" % reader.error, 5)
				return
			_config_seen = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 6)


func _decode_bool(data: PackedByteArray, fallback: bool) -> bool:
	return data[1] != 0 if data.size() >= 2 and data[0] != 0 else fallback


func _decode_int(data: PackedByteArray, fallback: int) -> int:
	return data.decode_s32(1) if data.size() >= 5 and data[0] != 0 else fallback


func _on_timeout() -> void:
	_fail("timed out role=%s config=%s" % [_role, GameState.runtime_config], 16)


func _pass(label: String) -> void:
	print("NETWORK RUNTIME CONFIG %s PASS" % label.to_upper())
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_RUNTIME_CONFIG_PERSISTENCE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
