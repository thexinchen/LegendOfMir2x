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
	if not panel.visible or not panel.call("requested_visible"):
		_fail("minimap is not visible by default")
		return
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
	panel.call("_zoom_at", Vector2(100, 100), 1.5)
	if absf(panel.get("_zoom") - 1.5) > 0.01 or panel.get_node("ZoomBackground/ZoomText").text != "150%":
		_fail("cursor zoom failed")
		return
	GameState.player_map_id = 0
	GameState.state_changed.emit()
	GameState.player_map_id = 24
	GameState.state_changed.emit()
	if absf(panel.get("_zoom") - 1.5) > 0.01:
		_fail("map reload reset minimap zoom")
		return
	if OS.has_environment("MIR2X_MINIMAP_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MINIMAP_SCREENSHOT"))
	print("MINIMAP PASS: original texture, markers, alpha, extend, pan and zoom")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("MINIMAP_SMOKE %s" % message)
	get_tree().quit(1)
