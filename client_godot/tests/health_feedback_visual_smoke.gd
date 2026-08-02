extends Control

const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	GameState.player_uid = 999
	GameState.player_x = 0
	GameState.player_y = -1
	GameState.view_x = float(371 * 48 - 304)
	GameState.view_y = float(132 * 32 - 234)
	GameState.creatures.clear()
	var now := Time.get_ticks_msec()
	GameState.ascend_strings = [
		{"x": 370 * 48 + 24 - 20, "y": 131 * 32, "type": 0, "value": 0, "start_time": now},
		{"x": 372 * 48 + 24, "y": 131 * 32, "type": 1, "value": -128, "start_time": now},
		{"x": 374 * 48 + 24, "y": 131 * 32, "type": 2, "value": 35, "start_time": now},
		{"x": 376 * 48 + 24, "y": 131 * 32, "type": 3, "value": 64, "start_time": now},
	]
	var renderer: Control = WorldRendererScript.new()
	renderer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	renderer.game_state = GameState
	if not renderer.load_map(24):
		_fail("unable to load visual map")
		return
	add_child(renderer)
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_PANEL_OUTPUT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-health-feedback.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("HEALTH FEEDBACK VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("HEALTH_FEEDBACK_VISUAL %s" % message)
	get_tree().quit(1)
