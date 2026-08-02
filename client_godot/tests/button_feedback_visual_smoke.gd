extends Control


func _ready() -> void:
	var background := ColorRect.new()
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.color = Color(0.04, 0.04, 0.04, 1.0)
	add_child(background)
	var horse := _add_panel("res://scenes/game/panels/horse.tscn", Vector2(32, 32))
	var purchase := _add_panel("res://scenes/game/panels/purchase.tscn", Vector2(330, 48))
	var team := _add_panel("res://scenes/game/panels/team.tscn", Vector2(330, 300))
	await get_tree().process_frame
	(horse.get_node("UpButton") as TextureButton).mouse_entered.emit()
	(team.get_node("SwitchButton") as TextureButton).mouse_entered.emit()
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_BUTTON_FEEDBACK_OUTPUT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-button-feedback.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		push_error("BUTTON_FEEDBACK_VISUAL unable to save screenshot: %s" % error)
		get_tree().quit(1)
		return
	if purchase.get_node("SelectButton").texture_normal == null or purchase.get_node("CloseButton").texture_pressed == null:
		push_error("BUTTON_FEEDBACK_VISUAL purchase normal/down state unavailable")
		get_tree().quit(2)
		return
	print("BUTTON FEEDBACK VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _add_panel(scene_path: String, panel_position: Vector2) -> Control:
	var panel := (load(scene_path) as PackedScene).instantiate() as Control
	panel.position = panel_position
	add_child(panel)
	return panel
