extends Control


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	GameState.player_uid = (5 << 59) | 1
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.player_direction = 5
	GameState.player_gender = 0
	GameState.player_health_initialized = true
	GameState.player_hp = 0
	GameState.player_action_type = 13
	GameState.player_action_started_ms = Time.get_ticks_msec()
	GameState.player_map_name = "边境城市"
	GameState.view_x = float(371 * 48 - 400)
	GameState.view_y = float(132 * 32 - 234)
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	if not main.get_node("WorldRenderer").call("load_map", 24):
		_fail("unable to load visual map")
		return
	main.call("_update_death_overlay")
	var overlay := main.get_node("DeathOverlay") as ColorRect
	if not overlay.visible or overlay.get_index() <= main.get_node("SkillBuffHUD").get_index() or overlay.get_index() >= main.get_node("ControlPanel").get_index():
		_fail("death veil draw order mismatch")
		return
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_DEATH_OVERLAY_SCREENSHOT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-death-overlay.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("DEATH OVERLAY VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("DEATH_OVERLAY_VISUAL %s" % message)
	get_tree().quit(1)
