extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")

enum Stage { WAIT_TARGET, WAIT_RESULT }

var _main: Control
var _stage := Stage.WAIT_TARGET
var _finished := false
var _target_uid := 0
var _target_click := Vector2.ZERO
var _initial_distance := 0
var _initial_player_grid := Vector2i.ZERO
var _saw_chase_move := false
var _saw_local_attack := false
var _saw_authoritative_hitted := false
var _miss_count := 0
var _result_hp := -1
var _result_max_hp := -1


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(40.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	match _stage:
		Stage.WAIT_TARGET:
			var target := _find_clickable_monster()
			if target.is_empty():
				return
			_target_uid = int(target.uid)
			_target_click = target.point
			_initial_player_grid = Vector2i(GameState.player_x, GameState.player_y)
			var creature: Dictionary = GameState.get_creature(_target_uid)
			_initial_distance = maxi(
				absi(GameState.player_x - int(creature.get("x", GameState.player_x))),
				absi(GameState.player_y - int(creature.get("y", GameState.player_y))),
			)
			var click := InputEventMouseButton.new()
			click.button_index = MOUSE_BUTTON_LEFT
			click.position = _target_click
			click.pressed = true
			_main.call("_unhandled_input", click)
			if int(_main.get("_attack_focus_uid")) != _target_uid:
				_fail("real left click did not select monster uid=%d point=%s focus=%s" % [_target_uid, _target_click, _main.get("_attack_focus_uid")], 7)
				return
			_stage = Stage.WAIT_RESULT
		Stage.WAIT_RESULT:
			_saw_local_attack = _saw_local_attack or GameState.player_action_type == 7
			_saw_chase_move = _saw_chase_move or (_initial_distance > 1 and Vector2i(GameState.player_x, GameState.player_y) != _initial_player_grid)
			if not _saw_local_attack or not _saw_authoritative_hitted or _result_hp < 0:
				return
			if _result_hp >= _result_max_hp:
				_fail("authoritative target health did not show damage: uid=%d hp=%d/%d" % [_target_uid, _result_hp, _result_max_hp], 8)
				return
			if OS.has_environment("MIR2X_NETWORK_ATTACK_SCREENSHOT"):
				await RenderingServer.frame_post_draw
				var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_ATTACK_SCREENSHOT"))
				if error != OK:
					_fail("failed to save attack screenshot: %s" % error, 9)
					return
			print("NETWORK ATTACK PASS: target=%d distance=%d chase=%s local-action=7 authoritative-hitted health=%d/%d misses=%d" % [_target_uid, _initial_distance, _saw_chase_move, _result_hp, _result_max_hp, _miss_count])
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()


func _find_clickable_monster() -> Dictionary:
	var renderer := _main.get_node("WorldRenderer") as Control
	var target_rects: Dictionary = renderer.get("_actor_target_rects")
	for uid_value in target_rects:
		var uid := int(uid_value)
		var creature: Dictionary = GameState.get_creature(uid)
		if int(creature.get("type", 0)) != 1 or int(creature.get("action_type", 0)) == 13:
			continue
		var rect: Rect2 = target_rects[uid].rect
		var point := rect.get_center()
		if point.x < 0.0 or point.x >= 800.0 or point.y < 0.0 or point.y >= 520.0:
			continue
		if int(renderer.call("focus_uid_at_screen", point, true)) == uid:
			return {"uid": uid, "point": point}
	return {}


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
			var online := Protocol.decode_sm_online_ok(payload)
			var action: Dictionary = online.get("action", {})
			GameState.set_player_online({"uid": online.get("uid", 0), "name": online.get("name", ""), "gender": online.get("gender", 0), "job": online.get("job", 0), "map_uid": online.get("mapUID", 0), "x": action.get("x", 0), "y": action.get("y", 0), "direction": action.get("direction", 0)})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_ACTION:
			if _target_uid == 0:
				return
			var data := Protocol.decode_sm_action(payload)
			var action: Dictionary = data.get("action", {})
			if int(data.get("uid", 0)) == _target_uid and int(action.get("type", 0)) == 11 and int(action.get("aimUID", 0)) == GameState.player_uid:
				_saw_authoritative_hitted = true
		NetworkClient.SM_HEALTH:
			if _target_uid == 0:
				return
			var reader = CerealReader.new(payload)
			var health: Dictionary = reader.read_sd_health()
			if reader.valid and int(health.get("uid", 0)) == _target_uid:
				_result_hp = int(health.get("hp", -1))
				_result_max_hp = int(health.get("maxHP", -1))
		NetworkClient.SM_MISS:
			if Protocol.decode_sm_miss(payload) == GameState.player_uid:
				_miss_count += 1
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d target=%d click=%s distance=%d local=%s hitted=%s health=%d/%d misses=%d player=(%d,%d) action=%d creatures=%s" % [_stage, _target_uid, _target_click, _initial_distance, _saw_local_attack, _saw_authoritative_hitted, _result_hp, _result_max_hp, _miss_count, GameState.player_x, GameState.player_y, GameState.player_action_type, GameState.creatures], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_ATTACK_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
