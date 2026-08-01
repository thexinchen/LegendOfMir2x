extends Node


func _ready() -> void:
	var panel := load("res://scenes/game/panels/guild.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	var slider := panel.get_node("Slider") as TextureRect
	if slider.size != Vector2(23, 26) or slider.position != Vector2(559, 41):
		_fail("native slider geometry mismatch: position=%s size=%s" % [slider.position, slider.size])
		return
	if slider.modulate.r > 0.6:
		_fail("idle slider should use the C++ dim tint")
		return
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = Vector2(14, 12 + 293 * 0.5)
	panel.call("_on_slider_input", press)
	if not is_equal_approx(panel.get("_scroll_value"), 0.5) or not is_equal_approx(slider.position.y, 187.5) or slider.modulate.r < 0.9:
		_fail("slider midpoint drag mismatch: value=%s position=%s tint=%s" % [panel.get("_scroll_value"), slider.position, slider.modulate])
		return
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	panel.call("_on_slider_input", release)
	if panel.get("_dragging_slider") or slider.modulate.r > 0.6:
		_fail("slider release did not restore idle state")
		return
	if OS.has_environment("MIR2X_GUILD_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GUILD_SCREENSHOT"))
	print("GUILD PANEL PASS: native knob geometry, drag range and active tint")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("GUILD_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
