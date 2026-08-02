extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var ground_item_id := 0
	for value in resources.item_meta.keys():
		var item_id: int = value
		if resources.item_package_gfx_id(item_id) > 0 and not resources.ground_item(item_id).is_empty():
			ground_item_id = item_id
			break
	if ground_item_id == 0:
		_fail("no original ground item texture found")
		return
	var star_frame: Dictionary = resources.frame("proguse", 0x00000090)
	var star_texture := star_frame.get("texture") as Texture2D
	if star_texture == null:
		_fail("original ground item notification star unavailable")
		return
	GameState.ground_items = {"1,1": [ground_item_id], "2,2": [ground_item_id]}
	GameState.update_ground_item_grids([
		{"x": 2, "y": 2, "items": []},
		{"x": 3, "y": 3, "items": [ground_item_id]},
	])
	if not GameState.ground_items.has("1,1") or GameState.ground_items.has("2,2") or not GameState.ground_items.has("3,3"):
		_fail("incremental grid snapshot semantics mismatch")
		return
	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	GameState.player_uid = (5 << 59) | (1 << 35) | 1
	GameState.player_x = 405
	GameState.player_y = 120
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.view_x = 405 * 48 - 400
	GameState.view_y = 120 * 32 - 300
	GameState.ground_items = {"406,120": [ground_item_id]}
	var dead_monster_id := 0
	for monster_id_value in resources.monster_meta:
		var monster_id: int = monster_id_value
		var look_id: int = resources.monster_look(monster_id)
		var death_key := (look_id << 12) | (4 << 8) | (4 << 5) | 9
		if not resources.frame("monster", death_key).is_empty():
			dead_monster_id = monster_id
			break
	if dead_monster_id == 0:
		_fail("no original monster death frame found")
		return
	GameState.creatures = {
		9001: {
			"uid": 9001, "type": 1, "monster_id": dead_monster_id,
			"x": 406, "y": 120, "direction": 5, "action_type": 13,
			"action_started_ms": Time.get_ticks_msec() - 2000, "action_speed": 100,
		},
	}
	$WorldRenderer.game_state = GameState
	if not $WorldRenderer.load_map(24):
		_fail("map 24 failed to load")
		return
	var item_name: String = resources.item_name(ground_item_id)
	var item_center := Vector2(472, 316)
	var name_layout: Dictionary = $WorldRenderer.call("_ground_item_name_layout", item_name, item_center)
	var name_font := name_layout.get("font") as Font
	var name_size: Vector2i = name_layout.get("raster_size", Vector2i.ZERO)
	var name_top_left: Vector2i = name_layout.get("top_left", Vector2i.ZERO)
	if name_font == null or not name_font.resource_path.ends_with("0B_WenQuanYi_Bitmap_Song_15_px.ttf") or name_layout.get("font_size", 0) != 15:
		_fail("ground item name did not use the packed font-11 resource")
		return
	if name_top_left.x != roundi(item_center.x) - name_size.x / 2 or name_top_left.y != roundi(item_center.y) - name_size.y / 2 - 20:
		_fail("ground item name texture was not centered at the original offset")
		return
	if not is_equal_approx(float(name_layout.baseline.y), float(name_top_left.y) + name_font.get_ascent(15)):
		_fail("ground item name baseline did not preserve measured top-left placement")
		return
	$WorldRenderer.set("_ground_item_star_ratio", 0.5)
	if not $WorldRenderer.call("_is_dead_actor", GameState.creatures[9001]):
		_fail("dead monster did not enter the pre-item draw pass")
		return
	var expected_star_size := roundi(0.5 * star_texture.get_width() / 2.5)
	if $WorldRenderer.call("_ground_item_star_size", star_texture.get_width()) != expected_star_size:
		_fail("ground item star scale mismatch")
		return
	$WorldRenderer.set("_ground_item_star_ratio", 1.05)
	if $WorldRenderer.call("_ground_item_star_size", star_texture.get_width()) != 0:
		_fail("ground item star visible interval mismatch")
		return
	$WorldRenderer.set("_ground_item_star_ratio", 0.75)
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_GROUND_ITEM_SCREENSHOT"):
		$WorldRenderer.set_process(false)
		$WorldRenderer.set("_ground_item_star_ratio", 1.05)
		var screenshot_center := Vector2(520, 316)
		GameState.ground_items = {"407,120": [ground_item_id]}
		Input.warp_mouse(screenshot_center)
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		if not is_equal_approx(float($WorldRenderer.get("_ground_item_star_ratio")), 1.05):
			_fail("screenshot fixture did not freeze the ground-item star")
			return
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GROUND_ITEM_SCREENSHOT"))
	print("GROUND ITEM PASS: id=%d name=%s death pre-pass, original sprite and rotating notification star" % [ground_item_id, resources.item_name(ground_item_id)])
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("GROUND_ITEM_SMOKE %s" % message)
	get_tree().quit(1)
