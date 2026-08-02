extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var dress_id := _find_visual_dress(resources)
	var hair_id := _find_visual_hair(resources)
	if dress_id == 0 or hair_id == 0:
		_fail("tintable dress/hair frames unavailable")
		return

	var white_desp := {
		"wear": {1: {"itemID": dress_id, "extAttrList": {}}},
		"hair": hair_id,
		"hairColor": 0xFFFFFFFF,
	}
	var tinted_desp := {
		"wear": {1: {
			"itemID": dress_id,
			"extAttrList": {41: PackedByteArray([1, 0x60, 0xC0, 0xFF, 0xFF])},
		}},
		"hair": hair_id,
		"hairColor": 0xFF40FF40,
	}
	var now := Time.get_ticks_msec()
	GameState.player_uid = 1101
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.player_direction = 5
	GameState.player_gender = 0
	GameState.player_action_type = 2
	GameState.player_action_started_ms = now
	GameState.player_desp = white_desp
	GameState.view_x = float(372 * 48 - 400)
	GameState.view_y = float(132 * 32 - 300)
	GameState.creatures = {
		1102: {
			"uid": 1102, "type": 2, "gender": 0, "desp": tinted_desp,
			"x": 373, "y": 132, "direction": 5, "action_type": 2,
			"action_speed": 100, "action_started_ms": now,
		},
	}
	var renderer: Control = WorldRendererScript.new()
	renderer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	renderer.game_state = GameState
	if not renderer.load_map(24):
		_fail("unable to load visual map")
		return
	add_child(renderer)
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_HERO_COLOR_SCREENSHOT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-hero-color.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("HERO COLOR VISUAL PASS: dress=%d hair=%d output=%s" % [dress_id, hair_id, output_path])
	get_tree().quit()


func _find_visual_dress(resources: RefCounted) -> int:
	for item_id_value in resources.item_types:
		var item_id := int(item_id_value)
		if resources.item_type(item_id) != "衣服":
			continue
		var shape: int = resources.item_shape(item_id)
		if shape <= 0:
			continue
		var gfx_id := (shape << 9) | 4
		var body_key := (gfx_id & 0x1FFFF) << 5
		if not resources.frame("hero", body_key).is_empty() and not resources.frame("hero", body_key | (1 << 24)).is_empty():
			return item_id
	return 0


func _find_visual_hair(resources: RefCounted) -> int:
	for hair_id in range(1, 257):
		var gfx_id := ((hair_id - 1) << 9) | 4
		var hair_key := (gfx_id & 0x1FFFF) << 5
		if not resources.frame("hair", hair_key).is_empty():
			return hair_id
	return 0


func _fail(message: String) -> void:
	push_error("HERO_COLOR_VISUAL %s" % message)
	get_tree().quit(1)
