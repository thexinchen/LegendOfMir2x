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
	$WorldRenderer.game_state = GameState
	if not $WorldRenderer.load_map(24):
		_fail("map 24 failed to load")
		return
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_GROUND_ITEM_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GROUND_ITEM_SCREENSHOT"))
	print("GROUND ITEM PASS: id=%d name=%s original sprite and world draw" % [ground_item_id, resources.item_name(ground_item_id)])
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("GROUND_ITEM_SMOKE %s" % message)
	get_tree().quit(1)
