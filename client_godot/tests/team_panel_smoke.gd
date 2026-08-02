extends Node


func _ready() -> void:
	GameState.player_uid = (5 << 59) | 1
	GameState.team_leader = GameState.player_uid
	GameState.team_members = []
	for index in 12:
		GameState.team_members.append({"uid": (5 << 59) | (100 + index), "level": 20 + index, "name": "队员%d" % index})
	GameState.team_members[0].name = ""
	GameState.team_candidates = [
		{"uid": (5 << 59) | 201, "level": 30, "name": "申请甲"},
		{"uid": (5 << 59) | 202, "level": 31, "name": "申请乙"},
	]
	var panel := load("res://scenes/game/panels/team.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	if panel.size != Vector2(258, 306) or panel.get_node("MemberRows").get_child_count() != 10:
		_fail("ten-row variable layout mismatch: size=%s rows=%d" % [panel.size, panel.get_node("MemberRows").get_child_count()])
		return
	if (panel.get_node("MemberRows/Row0") as Button).text != "0 PLY_100":
		_fail("empty team-player name did not use original UID fallback: %s" % (panel.get_node("MemberRows/Row0") as Button).text)
		return
	if not panel.get_node("AddButton").disabled or panel.get_node("DeleteButton").disabled:
		_fail("member-mode button gating mismatch")
		return
	panel.call("_select_uid", 0, GameState.team_members[3].uid)
	if panel.get("_selected_uids")[0] != GameState.team_members[3].uid:
		_fail("member selection was not retained")
		return
	panel.call("_toggle_mode")
	if panel.size != Vector2(258, 226) or panel.get_node("MemberRows").get_child_count() != 2:
		_fail("candidate minimum layout mismatch: size=%s rows=%d" % [panel.size, panel.get_node("MemberRows").get_child_count()])
		return
	if not panel.get_node("DeleteButton").disabled or panel.get_node("AddButton").disabled:
		_fail("candidate-mode button gating mismatch")
		return
	panel.call("_select_uid", 1, GameState.team_candidates[1].uid)
	panel.call("_toggle_mode")
	panel.call("_toggle_mode")
	if panel.get("_selected_uids")[0] != GameState.team_members[3].uid or panel.get("_selected_uids")[1] != GameState.team_candidates[1].uid:
		_fail("independent mode selections were not retained")
		return
	var accepted: Dictionary = GameState.team_candidates[1]
	GameState.add_team_candidate({"uid": GameState.team_candidates[0].uid, "level": 40, "name": "更新申请"})
	if GameState.team_candidates.size() != 2 or GameState.team_candidates[0].name != "更新申请":
		_fail("candidate replacement/front insertion mismatch: %s" % GameState.team_candidates)
		return
	GameState.set_team_member_list(GameState.player_uid, [accepted])
	if GameState.team_candidates.any(func(candidate: Dictionary): return candidate.uid == accepted.uid):
		_fail("accepted candidate was not removed")
		return
	var candidate_count := GameState.team_candidates.size()
	GameState.add_team_candidate(accepted)
	if GameState.team_candidates.size() != candidate_count:
		_fail("existing team member was accepted as a candidate")
		return
	panel.call("_select_uid", 1, GameState.team_candidates[0].uid)
	if OS.has_environment("MIR2X_TEAM_SCREENSHOT"):
		GameState.team_members = [{"uid": (5 << 59) | 100, "level": 20, "name": ""}]
		panel.set("_show_candidates", false)
		panel.call("_refresh")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_TEAM_SCREENSHOT"))
	print("TEAM PANEL PASS: original variable board, mode gating, rows and independent selections")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("TEAM_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
