extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var skeleton_id := _monster_id(resources, "变异骷髅")
	var summon_magic_id: int = resources.magic_id("召唤骷髅")
	if skeleton_id <= 0 or summon_magic_id <= 0:
		_fail("summon metadata unavailable")
		return
	GameState.player_uid = 999
	GameState.player_x = 0
	GameState.player_y = -1
	GameState.view_x = float(371 * 48 - 304)
	GameState.view_y = float(132 * 32 - 234)
	GameState.creatures = {
		1002: {
			"uid": 1002,
			"type": 1,
			"monster_id": skeleton_id,
			"x": 375,
			"y": 132,
			"direction": 6,
			"action_type": 2,
			"action_speed": 100,
			"action_started_ms": Time.get_ticks_msec(),
			"name": "第10帧落地",
		},
	}
	GameState.magic_effects = [{
		"uid": 1001,
		"summon_uid": 1001,
		"source": "summon_spawn",
		"magicID": summon_magic_id,
		"x": 371,
		"y": 132,
		"aimX": 371,
		"aimY": 132,
		"start_time": Time.get_ticks_msec() - 100,
	}]
	var renderer: Control = WorldRendererScript.new()
	renderer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	renderer.game_state = GameState
	renderer.actor_resource = resources
	if not renderer.load_map(24):
		_fail("unable to load visual map")
		return
	add_child(renderer)
	var resolved: Dictionary = renderer.call("_resolve_magic_effect", GameState.magic_effects[0], Time.get_ticks_msec())
	if resolved.is_empty() or resolved.get("stage", 0) != 2:
		_fail("summon run stage was not visually resolvable: %s" % resolved)
		return
	_add_caption("召唤运行段", Vector2(248, 154))
	_add_caption("第10帧落地", Vector2(440, 154))
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_PANEL_OUTPUT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-summon-spawn.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("SUMMON SPAWN VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _add_caption(text: String, position: Vector2) -> void:
	var label := Label.new()
	label.text = text
	label.position = position
	label.add_theme_font_size_override("font_size", 18)
	label.add_theme_color_override("font_color", Color.WHITE)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 4)
	add_child(label)


func _monster_id(resources: RefCounted, monster_name: String) -> int:
	for monster_id_value in resources.monster_names:
		var monster_id: int = monster_id_value
		if resources.monster_name(monster_id) == monster_name:
			return monster_id
	return 0


func _fail(message: String) -> void:
	push_error("SUMMON_SPAWN_VISUAL %s" % message)
	get_tree().quit(1)
