extends Node


func _ready() -> void:
	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	GameState.player_x = 405
	GameState.player_y = 120
	GameState.creatures = {
		1: {"x": 406, "y": 120, "type": 1},
		2: {"x": 405, "y": 121, "type": 3},
	}
	var panel := load("res://scenes/game/panels/minimap.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("MapViewport/MapTexture").texture == null:
		_fail("original minimap texture unavailable")
		return
	if panel.get_node("MapViewport/MapTexture").texture.get_size() != Vector2(900, 600):
		_fail("unexpected minimap texture size")
		return
	if panel.get_node("MapViewport/Markers").get_child_count() != 3:
		_fail("player/NPC/monster markers missing")
		return
	var coordinate := panel.get_node("Coordinate") as Label
	var zoom_text := panel.get_node("ZoomBackground/ZoomText") as Label
	if coordinate.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or coordinate.get_theme_font_size("font_size") != 12:
		_fail("coordinate text did not use original font-1/12")
		return
	if zoom_text.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or zoom_text.get_theme_font_size("font_size") != 12:
		_fail("zoom text did not use original font-1/12")
		return
	if not panel.visible or not panel.call("requested_visible"):
		_fail("minimap is not visible by default")
		return
	get_tree().root.size = Vector2i(1024, 768)
	await get_tree().process_frame
	if panel.size != Vector2(200, 200) or panel.position != Vector2(600, 0):
		_fail("scaled window changed the compact minimap's 800x600 logical anchor: position=%s size=%s" % [panel.position, panel.size])
		return
	get_tree().root.size = Vector2i(800, 600)
	await get_tree().process_frame
	GameState.player_map_id = 0
	GameState.state_changed.emit()
	if panel.visible or not panel.call("requested_visible"):
		_fail("missing texture did not preserve requested visibility")
		return
	GameState.player_map_id = 24
	GameState.state_changed.emit()
	if not panel.visible or not panel.call("has_map_texture"):
		_fail("available map did not restore default minimap")
		return
	panel.call("toggle_requested_visibility")
	if panel.visible or panel.call("requested_visible"):
		_fail("requested visibility toggle failed")
		return
	GameState.player_map_id = 0
	GameState.state_changed.emit()
	GameState.player_map_id = 24
	GameState.state_changed.emit()
	if panel.visible or panel.call("requested_visible"):
		_fail("map reload lost hidden requested state")
		return
	panel.call("toggle_requested_visibility")
	if not panel.visible or not panel.call("has_map_texture"):
		_fail("map reload did not restore the available minimap")
		return
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if not panel.visible or not panel.call("requested_visible"):
		_fail("Escape closed the minimap")
		return
	panel.call("_toggle_alpha")
	if panel.get_node("MapViewport/MapTexture").modulate.a > 0.6:
		_fail("alpha toggle failed")
		return
	panel.call("_toggle_size")
	if panel.size.x <= 200.0 or panel.size.y <= 200.0:
		_fail("extended minimap failed")
		return
	get_tree().root.size = Vector2i(1024, 768)
	await get_tree().process_frame
	var expected_extended_size := Vector2(roundf(800.0 * 0.8), roundf(600.0 * 0.5))
	var expected_extended_position := (Vector2(800, 600) - expected_extended_size) * 0.5
	if panel.size != expected_extended_size or panel.position != expected_extended_position:
		_fail("extended minimap did not follow the original dynamic canvas geometry after resize: position=%s/%s size=%s/%s" % [panel.position, expected_extended_position, panel.size, expected_extended_size])
		return
	get_tree().root.size = Vector2i(800, 600)
	await get_tree().process_frame
	panel.call("_zoom_at", Vector2(100, 100), 1.5)
	if absf(panel.get("_zoom") - 1.5) > 0.01 or panel.get_node("ZoomBackground/ZoomText").text != "150%":
		_fail("cursor zoom failed")
		return
	panel.call("_zoom_at", Vector2(100, 100), 0.1)
	panel.set("_hover_position", Vector2.ZERO)
	panel.call("_update_tooltip")
	if panel.get_node("Coordinate").visible:
		_fail("coordinate tooltip leaked into the blank area outside the minimap image")
		return
	var image_center: Vector2 = panel.get("_image_offset") + panel.call("_image_size") * 0.5
	panel.set("_hover_position", image_center)
	panel.call("_update_tooltip")
	if not panel.get_node("Coordinate").visible:
		_fail("coordinate tooltip disappeared inside the minimap image")
		return
	var coordinate_style := coordinate.get_theme_stylebox("normal") as StyleBoxFlat
	var coordinate_location: Vector2i = panel.call("_canvas_to_map", image_center)
	var coordinate_walkable: bool = panel.get("_world").can_walk(coordinate_location.x, coordinate_location.y)
	var expected_background := Color(0, 0, 0, 200.0 / 255.0) if coordinate_walkable else Color(1, 0, 0, 200.0 / 255.0)
	if coordinate_style == null or not coordinate_style.bg_color.is_equal_approx(expected_background):
		_fail("coordinate tooltip did not use original alpha-200 walkability background")
		return
	if not (coordinate.position + coordinate.size).is_equal_approx(image_center):
		_fail("coordinate tooltip was not tightly anchored above-left of the cursor")
		return
	var expected_coordinate_size := Vector2(
		ceilf(coordinate.get_theme_font("font").get_string_size(coordinate.text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x) + 2.0,
		ceilf(coordinate.get_theme_font("font").get_height(12)) + 1.0,
	)
	if not coordinate.size.is_equal_approx(expected_coordinate_size):
		_fail("coordinate tooltip did not fit replacement-font metrics: actual=%s expected=%s" % [coordinate.size, expected_coordinate_size])
		return
	for walkable in [true, false]:
		var sample_hover := _find_canvas_point(panel, walkable)
		if sample_hover.x < 0.0:
			_fail("map fixture has no %s coordinate tooltip sample" % ("walkable" if walkable else "blocked"))
			return
		panel.set("_hover_position", sample_hover)
		panel.call("_update_tooltip")
		var sample_background := Color(0, 0, 0, 200.0 / 255.0) if walkable else Color(1, 0, 0, 200.0 / 255.0)
		if not coordinate_style.bg_color.is_equal_approx(sample_background):
			_fail("coordinate tooltip walkability background mismatch: walkable=%s actual=%s" % [walkable, coordinate_style.bg_color])
			return
	panel.call("_zoom_at", image_center, 1.5)
	GameState.player_map_id = 0
	GameState.state_changed.emit()
	GameState.player_map_id = 24
	GameState.state_changed.emit()
	if absf(panel.get("_zoom") - 1.5) > 0.01:
		_fail("map reload reset minimap zoom")
		return
	if OS.has_environment("MIR2X_MINIMAP_BOUNDARY_SCREENSHOT"):
		panel.call("_zoom_at", Vector2(100, 100), 0.1)
		panel.set("_hover_position", Vector2.ZERO)
		panel.call("_update_tooltip")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MINIMAP_BOUNDARY_SCREENSHOT"))
	elif OS.has_environment("MIR2X_MINIMAP_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MINIMAP_SCREENSHOT"))
	print("MINIMAP PASS: original texture, markers, alpha, extend, pan and zoom")
	get_tree().quit()


func _find_canvas_point(panel: Control, walkable: bool) -> Vector2:
	var world: RefCounted = panel.get("_world")
	for y in range(0, world.height, 5):
		for x in range(0, world.width, 5):
			if world.can_walk(x, y) == walkable:
				return panel.call("_map_to_canvas", x, y)
	return Vector2(-1, -1)


func _fail(message: String) -> void:
	push_error("MINIMAP_SMOKE %s" % message)
	get_tree().quit(1)
