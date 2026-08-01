extends Control

const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	GameState.player_uid = 999
	GameState.player_x = 0
	GameState.player_y = -1
	GameState.view_x = float(371 * 48 - 304)
	GameState.view_y = float(132 * 32 - 234)
	GameState.creatures = {
		1001: _monster(1001, 371, 132, "MOUSE"),
		1002: _monster(1002, 372, 132, "MAGIC"),
		1003: _monster(1003, 373, 132, "FOLLOW"),
		1004: _monster(1004, 374, 132, "ATTACK"),
	}
	var renderer: Control = WorldRendererScript.new()
	renderer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	renderer.game_state = GameState
	if not renderer.load_map(24):
		_fail("unable to load visual map")
		return
	renderer.set_focus_channels(1002, 1003, 1004)
	var mouse_position := get_viewport().get_mouse_position()
	renderer._actor_target_rects = {
		1001: {"rect": Rect2(mouse_position - Vector2(5, 5), Vector2(10, 10)), "map_y": 132},
	}
	add_child(renderer)
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_PANEL_OUTPUT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-world-focus.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("WORLD FOCUS VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _monster(uid: int, x: int, y: int, name: String) -> Dictionary:
	return {
		"uid": uid,
		"type": 1,
		"monster_id": 224,
		"x": x,
		"y": y,
		"direction": 5,
		"action_type": 2,
		"action_speed": 100,
		"action_started_ms": Time.get_ticks_msec(),
		"name": name,
	}


func _fail(message: String) -> void:
	push_error("WORLD_FOCUS_VISUAL %s" % message)
	get_tree().quit(1)
