extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var weapon_id := _find_visual_weapon(resources)
	if weapon_id == 0:
		_fail("no weapon has both tested stand frames")
		return
	if resources.call("hero_weapon_order", 0, 1, 0) != 1 or resources.call("hero_weapon_order", 0, 5, 0) != 0:
		_fail("tested weapon orders do not cover both layers")
		return

	var now := Time.get_ticks_msec()
	var desp := {"wear": {3: {"itemID": weapon_id}}}
	GameState.player_uid = 1001
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.player_direction = 1
	GameState.player_gender = 0
	GameState.player_action_type = 2
	GameState.player_action_started_ms = now
	GameState.player_desp = desp
	GameState.view_x = float(372 * 48 - 400)
	GameState.view_y = float(132 * 32 - 300)
	GameState.creatures = {
		1002: {
			"uid": 1002, "type": 2, "gender": 0, "desp": desp,
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
	var output_path := OS.get_environment("MIR2X_HERO_WEAPON_ORDER_SCREENSHOT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-hero-weapon-order.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("HERO WEAPON ORDER VISUAL PASS: weapon=%d output=%s" % [weapon_id, output_path])
	get_tree().quit()


func _find_visual_weapon(resources: RefCounted) -> int:
	for item_id_value in resources.item_types:
		var item_id := int(item_id_value)
		if resources.item_type(item_id) != "武器":
			continue
		var shape: int = resources.item_shape(item_id)
		if shape <= 0:
			continue
		var has_all_frames := true
		for direction in [1, 5]:
			var weapon_gfx: int = ((shape - 1) << 9) | (int(direction) - 1)
			var weapon_key: int = (weapon_gfx & 0x1FFFF) << 5
			if resources.frame("weapon", weapon_key).is_empty():
				has_all_frames = false
				break
		if has_all_frames:
			return item_id
	return 0


func _fail(message: String) -> void:
	push_error("HERO_WEAPON_ORDER_VISUAL %s" % message)
	get_tree().quit(1)
