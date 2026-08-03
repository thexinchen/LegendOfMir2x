extends Node


func _ready() -> void:
	var panel := load("res://scenes/game/panels/guild.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	var slider := panel.get_node("Slider") as TextureRect
	var button_positions := {
		"AnnouncementButton": Vector2(40, 385),
		"MembersButton": Vector2(90, 385),
		"ChatButton": Vector2(140, 385),
		"EditButton": Vector2(290, 385),
		"RemoveButton": Vector2(340, 385),
		"DisbandButton": Vector2(390, 385),
		"PositionButton": Vector2(440, 385),
		"CovenantButton": Vector2(490, 385),
	}
	if panel.size != Vector2(594, 444) or (panel.get_node("Background") as TextureRect).texture.resource_path != "res://assets/ui/game/guild/00000500.png":
		_fail("native guild background geometry/resource mismatch: size=%s texture=%s" % [panel.size, (panel.get_node("Background") as TextureRect).texture.resource_path])
		return
	for button_name in button_positions:
		var button := panel.get_node(button_name) as TextureButton
		if button.position != button_positions[button_name] or button.size != Vector2(40, 40):
			_fail("native guild button geometry mismatch: name=%s position=%s size=%s" % [button_name, button.position, button.size])
			return
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
	print("GUILD PANEL PASS: native background/buttons, knob geometry, drag range and active tint")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("GUILD_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
