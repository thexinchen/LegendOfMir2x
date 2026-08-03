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
	var title := panel.get_node("Title") as Label
	var original_font_height := ceilf(title.get_theme_font("font").get_height(12))
	if title.position != Vector2(0, 57) or title.size != Vector2(258, original_font_height) or title.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or title.get_theme_font_size("font_size") != 12:
		_fail("team title did not use original y=57 font-1/12 layout: position=%s size=%s font=%s" % [title.position, title.size, title.get_theme_font("font").resource_path])
		return
	var first_row := panel.get_node("MemberRows/Row0") as Button
	var first_row_text := first_row.get_node_or_null("Text") as Label
	if first_row.text != "" or first_row_text == null or first_row_text.text != "0 PLY_100":
		_fail("empty team-player name did not use original UID fallback label")
		return
	if first_row_text.position != Vector2(5, 2) or first_row_text.size != Vector2(226, original_font_height) or first_row_text.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or first_row_text.get_theme_font_size("font_size") != 12:
		_fail("team row text did not use original (5,2) font-1/12 layout: position=%s size=%s font=%s" % [first_row_text.position, first_row_text.size, first_row_text.get_theme_font("font").resource_path])
		return
	if not panel.get_node("AddButton").disabled or panel.get_node("DeleteButton").disabled:
		_fail("member-mode button gating mismatch")
		return
	panel.call("_select_uid", 0, GameState.team_members[3].uid)
	if panel.get("_selected_uids")[0] != GameState.team_members[3].uid:
		_fail("member selection was not retained")
		return
	var overlay_alpha := 100.0 / 255.0
	var selected_row := panel.get_node("MemberRows/Row3") as Button
	var selected_normal := selected_row.get_theme_stylebox("normal") as StyleBoxFlat
	var selected_hover := selected_row.get_theme_stylebox("hover") as StyleBoxFlat
	var expected_normal := Color(1.0, 0.0, 0.0, overlay_alpha)
	var expected_hover := _source_over(Color(0.0, 0.0, 1.0, overlay_alpha), expected_normal)
	if not selected_normal.bg_color.is_equal_approx(expected_normal):
		_fail("selected-row overlay does not use original alpha: %s" % selected_normal.bg_color)
		return
	if not selected_hover.bg_color.is_equal_approx(expected_hover):
		_fail("selected hover does not preserve original red-then-blue layering: %s" % selected_hover.bg_color)
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
	if panel.get("_selected_uids")[0] != 0 or panel.get("_selected_uids")[1] != 0:
		_fail("removed team rows left stale selections: %s" % [panel.get("_selected_uids")[0], panel.get("_selected_uids")[1]])
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
		panel.call("_select_uid", 0, GameState.team_members[0].uid)
		await get_tree().process_frame
		var screenshot_row := panel.get_node("MemberRows/Row0") as Button
		get_viewport().warp_mouse(screenshot_row.global_position + Vector2(20, 8))
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_TEAM_SCREENSHOT"))
	print("TEAM PANEL PASS: original variable board, mode gating, rows and independent selections")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("TEAM_PANEL_SMOKE %s" % message)
	get_tree().quit(1)


func _source_over(top: Color, bottom: Color) -> Color:
	var output_alpha := top.a + bottom.a * (1.0 - top.a)
	return Color(
		(top.r * top.a + bottom.r * bottom.a * (1.0 - top.a)) / output_alpha,
		(top.g * top.a + bottom.g * bottom.a * (1.0 - top.a)) / output_alpha,
		(top.b * top.a + bottom.b * bottom.a * (1.0 - top.a)) / output_alpha,
		output_alpha,
	)
