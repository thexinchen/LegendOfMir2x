extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

var _main: Control
var _online := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(12.0).timeout.connect(func(): _fail("timed out", 7))


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if head_code == NetworkClient.SM_LOGINOK:
		if NetworkClient.enter_game() != OK:
			_fail("failed to enter game", 3)
	elif head_code == NetworkClient.SM_ONLINEOK and not _online:
		_online = true
		var online := Protocol.decode_sm_online_ok(payload)
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
		_main = load("res://scenes/game/main.tscn").instantiate()
		add_child(_main)
		get_tree().create_timer(2.0).timeout.connect(_verify)
	elif head_code in [NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR]:
		_fail("server rejected login/online", 4)


func _verify() -> void:
	if GameState.player_name != "亚当":
		_fail("wrong player name: %s" % GameState.player_name, 5)
		return
	if GameState.player_hp_max <= 0 or GameState.inventory.size() != 6 or GameState.belt.size() != 6:
		_fail("incomplete state hp=%d inventory=%d belt=%d" % [GameState.player_hp_max, GameState.inventory.size(), GameState.belt.size()], 6)
		return
	if OS.has_environment("MIR2X_GAME_RUNTIME_SCREENSHOT"):
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GAME_RUNTIME_SCREENSHOT"))
	print("GAME RUNTIME PASS: name=%s hp=%d/%d inventory=%d creatures=%d" % [GameState.player_name, GameState.player_hp, GameState.player_hp_max, GameState.inventory.size(), GameState.creatures.size()])
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _fail(message: String, code: int) -> void:
	push_error("GAME_RUNTIME_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
