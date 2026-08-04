extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const MARKER := "旋风腿方向验证"

var _role := "observer"
var _main: Control
var _online := false
var _armed := false
var _actor_started := false
var _finished := false
var _remote_uid := 0


func _ready() -> void:
	_role = OS.get_environment("MIR2X_SPINKICK_ROLE")
	if _role not in ["actor", "observer"]:
		_fail("invalid role: %s" % _role, 2)
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(25.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or not _online or _main == null:
		return
	var remote := _find_remote_player("亚当" if _role == "actor" else "夏娃")
	if remote.is_empty():
		return
	_remote_uid = int(remote.get("uid", 0))
	if _role == "actor":
		if not _actor_started:
			_actor_started = true
			_run_actor()
		return
	for message_value in GameState.player_say_messages.get(_remote_uid, []):
		if str(message_value.get("text", "")) == MARKER:
			_armed = true


func _on_connection_changed(connected: bool, _message: String) -> void:
	if not connected:
		return
	var account := "good" if _role == "actor" else "test"
	if NetworkClient.login(account, "123456") != OK:
		_fail("failed to send login for %s" % account, 3)


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
			_online = true
		NetworkClient.SM_ACTION:
			if _role != "observer" or not _armed:
				return
			var data := Protocol.decode_sm_action(payload)
			if int(data.get("uid", 0)) != _remote_uid:
				return
			var action: Dictionary = data.get("action", {})
			if int(action.get("type", 0)) == 2:
				_fail("actor sent an extra stand action before spin-kick", 8)
				return
			if int(action.get("type", 0)) != 12:
				return
			if int(action.get("direction", -1)) != 0:
				_fail("broadcast spin-kick retained a nonzero wire direction: %s" % action, 9)
				return
			print("NETWORK SPINKICK OBSERVER PASS: uid=%d wire=0 no-extra-stand" % _remote_uid)
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 5)


func _run_actor() -> void:
	if NetworkClient.send_player_say(MARKER) != OK:
		_fail("failed to send observer marker", 6)
		return
	await get_tree().create_timer(0.6).timeout
	var initial_direction := GameState.player_direction
	var aim_grid := Vector2i(GameState.player_x + (-2 if initial_direction == 3 else 2), GameState.player_y)
	var resources: RefCounted = _main.get("_resources")
	var magic_id: int = resources.call("magic_id", "空拳刀法")
	if magic_id <= 0 or not _main.call("_execute_spell_action", 12, magic_id, aim_grid, 0):
		_fail("local spin-kick action was rejected", 7)
		return
	if GameState.player_direction != initial_direction:
		_fail("ground-only local spin-kick changed direction", 10)
		return
	await get_tree().create_timer(1.5).timeout
	print("NETWORK SPINKICK ACTOR PASS: local=%d aim=%s" % [initial_direction, aim_grid])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _find_remote_player(expected_name: String) -> Dictionary:
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 2 and int(uid_value) != GameState.player_uid and str(creature.get("name", "")) == expected_name:
			return creature
	return {}


func _on_timeout() -> void:
	_fail("timed out role=%s online=%s armed=%s remote=%d creatures=%s" % [_role, _online, _armed, _remote_uid, GameState.creatures], 11)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_SPINKICK_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
