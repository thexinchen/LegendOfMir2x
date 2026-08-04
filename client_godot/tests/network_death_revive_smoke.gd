extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

enum Stage { WAIT_READY, WAIT_DEAD, WAIT_REVIVED }

var _main: Control
var _stage := Stage.WAIT_READY
var _health_received := false
var _revive_health_msec := 0
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(30.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	var renderer := _main.get_node("WorldRenderer") as Control
	var overlay := _main.get_node("DeathOverlay") as ColorRect
	match _stage:
		Stage.WAIT_READY:
			if not _health_received or GameState.player_hp <= 0 or GameState.player_action_type == 13:
				return
			var world: RefCounted = renderer.get("world_resource")
			if int(world.get("map_id")) != GameState.player_map_id:
				return
			_submit_command("@die")
			_stage = Stage.WAIT_DEAD
		Stage.WAIT_DEAD:
			if GameState.player_hp != 0 or GameState.player_action_type != 13:
				return
			_main.call("_update_death_overlay")
			if not overlay.visible or not overlay.color.is_equal_approx(Color(128.0 / 255.0, 0, 0, 64.0 / 255.0)):
				_fail("authoritative death did not show the original red veil", 7)
				return
			_main.set("_follow_focus_uid", 777)
			var blocked_click := InputEventMouseButton.new()
			blocked_click.button_index = MOUSE_BUTTON_RIGHT
			blocked_click.pressed = true
			blocked_click.position = Vector2(500, 300)
			_main.call("_unhandled_input", blocked_click)
			if int(_main.get("_follow_focus_uid")) != 777:
				_fail("dead player accepted a world mouse command", 8)
				return
			if OS.has_environment("MIR2X_NETWORK_DEATH_SCREENSHOT"):
				await RenderingServer.frame_post_draw
				var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_DEATH_SCREENSHOT"))
				if error != OK:
					_fail("failed to save authoritative death screenshot: %s" % error, 9)
					return
			_submit_command("@revive")
			_stage = Stage.WAIT_REVIVED
		Stage.WAIT_REVIVED:
			if GameState.player_hp <= 0:
				return
			if _revive_health_msec == 0:
				_revive_health_msec = Time.get_ticks_msec()
			if GameState.player_action_type == 13:
				if Time.get_ticks_msec() - _revive_health_msec >= 2000:
					_fail("revive health recovered but the authoritative stand action was rejected", 10)
				return
			_main.call("_update_death_overlay")
			if overlay.visible:
				_fail("revived player retained the death veil", 11)
				return
			var live_click := InputEventMouseButton.new()
			live_click.button_index = MOUSE_BUTTON_RIGHT
			live_click.pressed = true
			live_click.position = Vector2(500, 300)
			_main.call("_unhandled_input", live_click)
			if int(_main.get("_follow_focus_uid")) != 0:
				_fail("revived player did not regain world mouse input", 12)
				return
			print("NETWORK DEATH REVIVE PASS: visible @die hp=0/action=13/veil -> @revive hp=%d/action=%d/world-input" % [GameState.player_hp, GameState.player_action_type])
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()


func _submit_command(text: String) -> void:
	var command := _main.get_node("ControlPanel").get_node("%Command") as LineEdit
	command.text = text
	command.text_submitted.emit(command.text)


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
			GameState.set_player_online({"uid": online.get("uid", 0), "name": online.get("name", ""), "gender": online.get("gender", 0), "job": online.get("job", 0), "map_uid": online.get("mapUID", 0), "x": action.get("x", 0), "y": action.get("y", 0), "direction": action.get("direction", 0)})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_HEALTH:
			_health_received = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d hp=%d action=%d health=%s" % [_stage, GameState.player_hp, GameState.player_action_type, _health_received], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_DEATH_REVIVE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
