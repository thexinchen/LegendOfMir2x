extends Control

const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	var now := Time.get_ticks_msec()
	GameState.player_uid = 999
	GameState.player_x = 0
	GameState.player_y = -1
	GameState.view_x = float(371 * 48 - 304)
	GameState.view_y = float(132 * 32 - 234)
	GameState.creatures = {
		1001: _npc(1001, 7, 371, 132, 1, 0, now - 200),
		1002: _npc(1002, 7, 376, 132, 1, 1, now - 200),
		1003: _npc(1003, 56, 371, 137, 1, 0, now - 600),
		1004: _npc(1004, 59, 376, 137, 1, 0, now),
	}
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
		output_path = "/tmp/mir2x-npc-motion.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("NPC MOTION VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _npc(uid: int, npc_id: int, x: int, y: int, direction: int, motion: int, started_ms: int) -> Dictionary:
	return {
		"uid": uid,
		"type": 3,
		"npc_id": npc_id,
		"x": x,
		"y": y,
		"direction": direction,
		"action_type": 2,
		"action_speed": 100,
		"action_started_ms": started_ms,
		"npc_motion": motion,
	}


func _fail(message: String) -> void:
	push_error("NPC_MOTION_VISUAL %s" % message)
	get_tree().quit(1)
