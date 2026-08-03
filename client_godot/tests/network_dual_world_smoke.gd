extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const SAY_MARKER := "双客户端世界交互验证"
const WAIT_SECONDS := 25.0

var _role := "observer"
var _main: Control
var _online := false
var _remote_uid := 0
var _remote_initial_position := Vector2i.ZERO
var _saw_remote := false
var _saw_name := false
var _saw_appearance := false
var _saw_say := false
var _saw_move := false
var _team_request_sent := false
var _saw_team_candidate := false
var _actor_team_created := false
var _actor_candidate_accepted := false
var _actor_saw_full_team := false
var _actor_saw_member_leave := false
var _observer_leave_requested := false
var _observer_left_team := false
var _actor_started := false
var _actor_relocation_requested := false
var _capture_started := false
var _capture_done := false
var _finished := false


func _ready() -> void:
	_role = OS.get_environment("MIR2X_DUAL_ROLE")
	if _role not in ["observer", "actor"]:
		_fail("invalid role: %s" % _role, 2)
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(WAIT_SECONDS).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or not _online or _main == null:
		return
	if _role == "actor":
		_process_actor()
	else:
		_process_observer()


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
			print("NETWORK DUAL WORLD ONLINE role=%s uid=%d map=%d pos=(%d,%d)" % [_role, online.get("uid", 0), online.get("mapUID", 0), action.get("x", 0), action.get("y", 0)])
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
			if _role == "actor":
				_actor_relocation_requested = NetworkClient.send_query_map_base_uid(24, _on_actor_map_uid) == OK
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login/online", 5)


func _on_actor_map_uid(head_code: int, payload: PackedByteArray) -> void:
	if head_code != NetworkClient.SM_UID or payload.size() < 8:
		_fail("failed to query observer map UID", 9)
		return
	if NetworkClient.send_request_space_move(payload.decode_u64(0), 375, 132) != OK:
		_fail("failed to request actor relocation", 10)


func _process_actor() -> void:
	if not _actor_relocation_requested or GameState.player_map_id != 24:
		return
	var remote := _find_remote_player("亚当")
	if remote.is_empty() or not _appearance_ready(remote):
		return
	for candidate_value in GameState.team_candidates:
		var candidate: Dictionary = candidate_value
		if int(candidate.get("uid", 0)) == int(remote.get("uid", 0)) and str(candidate.get("name", "")) == "亚当":
			_saw_team_candidate = true
	if not _saw_team_candidate:
		return
	if not _actor_team_created:
		if int(_main.call("_request_team_flag_target", GameState.player_uid)) != OK:
			_fail("failed to create the actor team through the self team-flag target", 14)
			return
		_actor_team_created = true
		return
	if not _actor_candidate_accepted:
		if not _team_has_uid(GameState.player_uid):
			return
		_main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/team.tscn")
		var team_panel := _main.get("_extra_panel_nodes").get("res://scenes/game/panels/team.tscn") as Control
		if team_panel == null or not team_panel.visible:
			_fail("actor team panel did not open after creating the team", 15)
			return
		team_panel.call("_toggle_mode")
		team_panel.call("_select_uid", 1, int(remote.get("uid", 0)))
		team_panel.call("_join_selected")
		_actor_candidate_accepted = true
		return
	if not _actor_saw_full_team:
		if not _team_has_uid(GameState.player_uid) or not _team_has_uid(int(remote.get("uid", 0))):
			return
		_actor_saw_full_team = true
	if not _actor_saw_member_leave:
		if GameState.team_members.size() != 1 or not _team_has_uid(GameState.player_uid):
			return
		_actor_saw_member_leave = true
	if _actor_started:
		return
	_actor_started = true
	await get_tree().create_timer(1.0).timeout
	if NetworkClient.send_player_say(SAY_MARKER) != OK:
		_fail("failed to send player say", 6)
		return
	var destination := _adjacent_walkable_position()
	if destination == Vector2i(-1, -1):
		_fail("no adjacent walkable position", 7)
		return
	_main.call("_send_move_action", destination.x, destination.y)
	await get_tree().create_timer(1.5).timeout
	print("NETWORK DUAL WORLD ACTOR PASS: remote=%s destination=%s" % [remote.get("name", ""), destination])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _process_observer() -> void:
	var remote := _find_remote_player("夏娃")
	if not remote.is_empty():
		if not _saw_remote:
			_remote_uid = int(remote.get("uid", 0))
			_remote_initial_position = Vector2i(int(remote.get("x", 0)), int(remote.get("y", 0)))
			_saw_remote = true
		_saw_name = str(remote.get("name", "")) == "夏娃"
		_saw_appearance = _appearance_ready(remote)
		if _saw_appearance and not _team_request_sent:
			_main.call("_set_team_flag_cursor", true)
			if not _main.get_node("TeamFlagCursor").visible:
				_fail("team flag cursor did not become visible", 11)
				return
			if int(_main.call("_request_team_flag_target", _remote_uid)) != OK:
				_fail("team flag request was rejected", 12)
				return
			_main.call("_set_team_flag_cursor", false)
			_team_request_sent = true
		var current_position := Vector2i(int(remote.get("x", 0)), int(remote.get("y", 0)))
		_saw_move = _saw_move or current_position != _remote_initial_position
		for message_value in GameState.player_say_messages.get(_remote_uid, []):
			if str(message_value.get("text", "")) == SAY_MARKER:
				_saw_say = true
	if _team_has_uid(GameState.player_uid) and _team_has_uid(_remote_uid) and not _observer_leave_requested:
		_main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/team.tscn")
		var team_panel := _main.get("_extra_panel_nodes").get("res://scenes/game/panels/team.tscn") as Control
		if team_panel == null or not team_panel.visible or team_panel.get_node("MemberRows").get_child_count() != 2:
			_fail("observer team panel did not show both real members", 16)
			return
		team_panel.call("_select_uid", 0, GameState.player_uid)
		team_panel.call("_leave_selected")
		_observer_leave_requested = true
	if _observer_leave_requested and GameState.team_members.is_empty():
		_observer_left_team = true
	if _saw_remote and _saw_name and _saw_appearance and _team_request_sent and _observer_left_team and _saw_say and _saw_move and not _capture_started:
		_capture_started = true
		_capture_dual_world()
	if _saw_remote and _saw_name and _saw_appearance and _team_request_sent and _observer_left_team and _saw_say and _saw_move and _capture_done and GameState.get_creature(_remote_uid).is_empty():
		print("NETWORK DUAL WORLD OBSERVER PASS: uid=%d" % _remote_uid)
		_finished = true
		NetworkClient.disconnect_from_server()
		get_tree().quit()


func _capture_dual_world() -> void:
	var output_path := OS.get_environment("MIR2X_DUAL_SCREENSHOT")
	if not output_path.is_empty():
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(output_path)
		if error != OK:
			_fail("unable to save dual-world screenshot: %s" % error, 13)
			return
	_capture_done = true


func _find_remote_player(expected_name: String) -> Dictionary:
	var unnamed := {}
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) != 2 or int(uid_value) == GameState.player_uid:
			continue
		if str(creature.get("name", "")) == expected_name:
			return creature
		if unnamed.is_empty():
			unnamed = creature
	return unnamed


func _appearance_ready(creature: Dictionary) -> bool:
	var desp: Dictionary = creature.get("desp", {})
	return desp.has("wear")


func _team_has_uid(uid: int) -> bool:
	for member_value in GameState.team_members:
		var member: Dictionary = member_value
		if int(member.get("uid", 0)) == uid:
			return true
	return false


func _adjacent_walkable_position() -> Vector2i:
	var origin := Vector2i(GameState.player_x, GameState.player_y)
	for offset_value in [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1)]:
		var offset: Vector2i = offset_value
		var destination: Vector2i = origin + offset
		if _main.get_node("WorldRenderer").call("can_walk", destination.x, destination.y):
			return destination
	return Vector2i(-1, -1)


func _on_timeout() -> void:
	_fail(
		"timed out role=%s online=%s map=%d pos=(%d,%d) remote=%s name=%s appearance=%s team_request=%s team_candidate=%s team_created=%s accepted=%s full_team=%s actor_leave=%s observer_leave=%s observer_left=%s say=%s move=%s capture=%s members=%s creatures=%s present=%s" % [
			_role, _online, GameState.player_map_uid, GameState.player_x, GameState.player_y,
			_saw_remote, _saw_name, _saw_appearance, _team_request_sent, _saw_team_candidate,
			_actor_team_created, _actor_candidate_accepted, _actor_saw_full_team, _actor_saw_member_leave,
			_observer_leave_requested, _observer_left_team, _saw_say, _saw_move, _capture_done,
			GameState.team_members, GameState.creatures,
			GameState.get_creature(_remote_uid) if _remote_uid != 0 else {},
		],
		8,
	)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_DUAL_WORLD_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
