extends Control


func _ready() -> void:
	var scene_path := OS.get_environment("MIR2X_PANEL_SCENE")
	var output_path := OS.get_environment("MIR2X_PANEL_OUTPUT")
	if scene_path.is_empty() or output_path.is_empty():
		push_error("MIR2X_PANEL_SCENE and MIR2X_PANEL_OUTPUT are required")
		get_tree().quit(1)
		return
	var packed := load(scene_path) as PackedScene
	if not packed:
		push_error("Unable to load panel: %s" % scene_path)
		get_tree().quit(1)
		return
	var panel := packed.instantiate() as Control
	add_child(panel)
	panel.position = Vector2(20, 20)
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(output_path)
	get_tree().quit()
