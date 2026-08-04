extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const TEAMERR_INTEAM := 2
const TEAM_PANEL := "res://scenes/game/panels/team.tscn"

enum Stage { WAIT_READY, WAIT_CREATED, WAIT_ERROR, WAIT_GOLD, WAIT_LEFT }

var _main: Control
var _stage := Stage.WAIT_READY
var _online_msec := 0
var _saw_team_error := false
var _saw_post_error_gold := false
var _post_error_gold_requested := false
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(20.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	match _stage:
		Stage.WAIT_READY:
			if Time.get_ticks_msec() - _online_msec < 500:
				return
			if not GameState.team_members.is_empty() or GameState.team_leader != 0:
				_fail("test player unexpectedly entered with a team: leader=%d members=%s" % [GameState.team_leader, GameState.team_members], 6)
				return
			if int(_main.call("_request_team_flag_target", GameState.player_uid)) != OK:
				_fail("real team-flag entry rejected self team creation", 7)
				return
			_stage = Stage.WAIT_CREATED
		Stage.WAIT_CREATED:
			if GameState.team_leader != GameState.player_uid or not _team_has_uid(GameState.player_uid):
				return
			if int(_main.call("_request_team_flag_target", GameState.player_uid)) != OK:
				_fail("real team-flag entry rejected repeated self request", 8)
				return
			_stage = Stage.WAIT_ERROR
		Stage.WAIT_ERROR:
			if not _saw_team_error:
				return
			if GameState.team_leader != GameState.player_uid or GameState.team_members.size() != 1 or not _team_has_uid(GameState.player_uid):
				_fail("TEAMERR_INTEAM changed the existing team: leader=%d members=%s" % [GameState.team_leader, GameState.team_members], 9)
				return
			_post_error_gold_requested = true
			if NetworkClient.send_query_gold() != OK:
				_fail("connection could not send a query after TEAMERR_INTEAM", 10)
				return
			_stage = Stage.WAIT_GOLD
		Stage.WAIT_GOLD:
			if not _saw_post_error_gold:
				return
			_main.call("_on_control_panel_panel_requested", TEAM_PANEL)
			var team_panel := _main.get("_extra_panel_nodes").get(TEAM_PANEL) as Control
			if team_panel == null or not team_panel.visible or team_panel.get_node("MemberRows").get_child_count() != 1:
				_fail("real team panel did not show the surviving self team", 11)
				return
			_stage = Stage.WAIT_LEFT
			if OS.has_environment("MIR2X_TEAM_ERROR_SCREENSHOT"):
				await RenderingServer.frame_post_draw
				var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_TEAM_ERROR_SCREENSHOT"))
				if error != OK:
					_fail("failed to save team-error screenshot: %s" % error, 13)
					return
			team_panel.call("_select_uid", 0, GameState.player_uid)
			team_panel.call("_leave_selected")
		Stage.WAIT_LEFT:
			if not GameState.team_members.is_empty() or GameState.team_leader != 0:
				return
			print("NETWORK TEAM ERROR PASS: TEAMERR_INTEAM=2 kept connection alive and team panel cleanup succeeded")
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()


func _team_has_uid(uid: int) -> bool:
	for value in GameState.team_members:
		var member: Dictionary = value
		if int(member.get("uid", 0)) == uid:
			return true
	return false


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("good", "123456") != OK:
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
			_online_msec = Time.get_ticks_msec()
		NetworkClient.SM_TEAMERROR:
			if _stage != Stage.WAIT_ERROR or payload.size() != 1 or payload[0] != TEAMERR_INTEAM:
				_fail("unexpected team error stage=%d payload=%s" % [_stage, payload], 4)
				return
			_saw_team_error = true
		NetworkClient.SM_GOLD:
			if _post_error_gold_requested:
				_saw_post_error_gold = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 5)


func _on_timeout() -> void:
	_fail("timed out stage=%d error=%s gold=%s leader=%d members=%s" % [_stage, _saw_team_error, _saw_post_error_gold, GameState.team_leader, GameState.team_members], 12)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_TEAM_ERROR_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
