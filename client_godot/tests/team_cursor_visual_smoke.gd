extends Control


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	GameState.player_uid = (5 << 59) | 1
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.player_direction = 5
	GameState.player_gender = 0
	GameState.player_action_type = 2
	GameState.player_action_started_ms = Time.get_ticks_msec()
	GameState.player_map_name = "边境城市"
	GameState.view_x = float(371 * 48 - 400)
	GameState.view_y = float(132 * 32 - 234)
	GameState.team_members = []
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	if not main.get_node("WorldRenderer").call("load_map", 24):
		_fail("unable to load visual map")
		return
	Input.warp_mouse(Vector2(400, 260))
	main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/team.tscn")
	main.call("_update_team_flag_cursor")
	var cursor := main.get_node("TeamFlagCursor") as TextureRect
	if not cursor.visible or cursor.texture == null:
		_fail("team-flag cursor texture unavailable")
		return
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_TEAM_CURSOR_SCREENSHOT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-team-cursor.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	main.call("_set_team_flag_cursor", false)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("TEAM CURSOR VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _fail(message: String) -> void:
	Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	push_error("TEAM_CURSOR_VISUAL %s" % message)
	get_tree().quit(1)
