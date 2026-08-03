extends Node


func _ready() -> void:
	var panel := load("res://scenes/game/panels/input_string.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	var commits: Array[String] = []
	var cancel_count := [0]
	panel.committed.connect(func(value: String): commits.append(value))
	panel.cancelled.connect(func(): cancel_count[0] += 1)
	var title := panel.get_node("Title") as Label
	var value_clip := panel.get_node("ValueClip") as Control
	var value_input := panel.get_node("ValueClip/Value") as LineEdit
	var normal_style := value_input.get_theme_stylebox("normal") as StyleBoxFlat
	var focus_style := value_input.get_theme_stylebox("focus") as StyleBoxFlat
	if title.position.y != 120.0 or title.size.y != 60.0 or title.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or title.get_theme_font_size("font_size") != 12:
		_fail("title did not use original y=120 font-1/12 placement: position=%s control_size=%s font=%s font_size=%d" % [title.position, title.size, title.get_theme_font("font").resource_path, title.get_theme_font_size("font_size")])
		return
	if value_clip.position != Vector2(22, 225) or value_clip.size != Vector2(315, 23) or not value_clip.clip_contents or value_input.position != Vector2.ZERO or value_input.size.x != 315.0 or value_input.get_theme_font("font").resource_path != "res://assets/font/01_Yahei.ttf" or value_input.get_theme_font_size("font_size") != 14:
		_fail("input value did not use original clipped (22,225,315,23) font-1/14 layout: clip=%s/%s/%s input=%s/%s font=%s font_size=%d" % [value_clip.position, value_clip.size, value_clip.clip_contents, value_input.position, value_input.size, value_input.get_theme_font("font").resource_path, value_input.get_theme_font_size("font_size")])
		return
	var style_checks := {
		"normal": normal_style != null and normal_style.bg_color.is_equal_approx(Color(0, 0, 0, 0)),
		"focus": focus_style != null and focus_style.bg_color.is_equal_approx(Color(1, 1, 1, 32.0 / 255.0)),
		"font": value_input.get_theme_color("font_color").is_equal_approx(Color.WHITE),
		"selected": value_input.get_theme_color("font_selected_color").is_equal_approx(Color.WHITE),
		"caret": value_input.get_theme_color("caret_color").is_equal_approx(Color.WHITE),
		"selection": value_input.get_theme_color("selection_color").is_equal_approx(Color(0, 0, 0, 0)),
		"secret": value_input.secret_character == "*",
	}
	for check_name in style_checks:
		if not style_checks[check_name]:
			_fail("input self-draw style check failed: %s; normal=%s focus=%s font=%s selected=%s caret=%s selection=%s secret=%s" % [check_name, normal_style.bg_color if normal_style else Color(-1, -1, -1, -1), focus_style.bg_color if focus_style else Color(-1, -1, -1, -1), value_input.get_theme_color("font_color"), value_input.get_theme_color("font_selected_color"), value_input.get_theme_color("caret_color"), value_input.get_theme_color("selection_color"), value_input.secret_character])
			return
	for style in [normal_style, focus_style]:
		for side in [SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM]:
			if style.get_content_margin(side) != 0.0:
				_fail("input self-draw style added a content margin: style=%s side=%d margin=%s" % [style, side, style.get_content_margin(side)])
				return
	var confirm := panel.get_node("ConfirmButton") as TextureButton
	var cancel := panel.get_node("CancelButton") as TextureButton
	if confirm.position != Vector2(66, 190) or confirm.size != Vector2(80, 34) or cancel.position != Vector2(212, 190) or cancel.size != Vector2(80, 34):
		_fail("YES/NO button geometry mismatch: confirm=%s/%s cancel=%s/%s" % [confirm.position, confirm.size, cancel.position, cancel.size])
		return
	panel.configure("<layout><par>请输入密码<br/>用于验证</par></layout>", true)
	if panel.get_node("Title").text != "请输入密码\n用于验证" or not value_input.secret:
		_fail("XML title/security mode mismatch: title=%s secret=%s" % [panel.get_node("Title").text, value_input.secret])
		return
	value_input.text = "  \t密码  "
	panel.call("_confirm")
	if commits != ["密码  "] or panel.visible or not value_input.text.is_empty():
		_fail("leading trim/confirm cleanup mismatch: %s" % commits)
		return
	panel.configure("购买数量", false)
	if value_input.secret:
		_fail("non-security input was hidden")
		return
	value_input.text = "中".repeat(100)
	panel.call("_on_text_changed", value_input.text)
	if value_input.text.to_utf8_buffer().size() > 255:
		_fail("UTF-8 input exceeded the C++ 255-byte buffer")
		return
	panel.call("_cancel")
	if cancel_count[0] != 1 or panel.visible or not value_input.text.is_empty():
		_fail("Cancel did not clear and signal")
		return
	panel.configure("再次输入", true)
	value_input.text = "secret"
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if cancel_count[0] != 2 or panel.visible or not value_input.text.is_empty():
		_fail("Escape did not use cancellation cleanup")
		return
	if OS.has_environment("MIR2X_INPUT_SCREENSHOT"):
		panel.configure("<layout><par>请输入密码</par></layout>", true)
		value_input.text = "mir2x-password"
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_INPUT_SCREENSHOT"))
	print("INPUT STRING PASS: native geometry/style, XML title, security, UTF-8 limit, trim, Cancel and Escape")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("INPUT_STRING_SMOKE %s" % message)
	get_tree().quit(1)
