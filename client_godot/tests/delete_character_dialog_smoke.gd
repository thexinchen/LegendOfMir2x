extends Node


func _ready() -> void:
	var select := load("res://scenes/account/select_character.tscn").instantiate() as Control
	add_child(select)
	await get_tree().process_frame
	select.call("_on_server_message", NetworkClient.SM_QUERYCHAROK, _character_payload("亚当", 1, 2, 10000))
	select.set_process(false)
	select.call("_on_delete_pressed")
	var dialog := select.get_node("DeleteCharacterDialog") as Control
	var password := dialog.get_node("PasswordInput") as LineEdit
	if not dialog.visible or not password.has_focus():
		_fail("popup did not open with password focus")
		return
	for path in ["StartButton", "CreateButton", "DeleteButton", "ExitButton"]:
		if select.get_node(path).visible:
			_fail("underlying %s remained visible while popup was open" % path)
			return
	if dialog.get_node("Background").position != Vector2(221, 166) or dialog.get_node("Background").size != Vector2(358, 268):
		_fail("popup background geometry diverged from C++")
		return
	var warning := dialog.get_node("Warning") as Label
	if warning.position != Vector2(275, 286) or warning.size.x != 250.0 or warning.get_theme_font_size("font_size") != 12 or warning.get_theme_constant("line_spacing") != -3:
		_fail("warning geometry or size diverged from C++")
		return
	if warning.text != "删除的角色将无法还原，请谨慎操作。\n如果确定删除，请输入游戏密码，并点击YES。":
		_fail("warning did not preserve the original two-line wrap")
		return
	if not warning.get_theme_font("font").resource_path.ends_with("0A_WenQuanYi_Bitmap_Song_15_px.ttf"):
		_fail("warning did not use the calibrated replacement font")
		return
	var input_style := password.get_theme_stylebox("normal") as StyleBoxFlat
	if password.position != Vector2(243, 391) or password.size != Vector2(315, 23) or password.get_theme_font_size("font_size") != 14 or not is_equal_approx(input_style.bg_color.a, 32.0 / 255.0):
		_fail("password geometry, font size or alpha-32 fill diverged from C++")
		return
	if dialog.get_node("YesButton").position != Vector2(287, 356) or dialog.get_node("NoButton").position != Vector2(433, 356):
		_fail("YES/NO positions diverged from C++")
		return
	if OS.has_environment("MIR2X_SELECT_DELETE_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SELECT_DELETE_SCREENSHOT"))
	password.text = "cancel-secret"
	dialog.call("_on_no_pressed")
	if dialog.visible or not password.text.is_empty() or not select.get_node("StartButton").visible or not select.get_node("ExitButton").visible:
		_fail("NO did not clear, close and restore selection controls")
		return
	select.call("_on_delete_pressed")
	password.text = "confirm-secret"
	dialog.call("_on_password_text_submitted", password.text)
	if dialog.visible or not password.text.is_empty() or not select.get_node("StartButton").visible or not select.get_node("ExitButton").visible:
		_fail("Enter did not submit, clear, close and restore selection controls")
		return
	AudioService.stop_bgm()
	AudioService.stop_seff()
	for child in AudioService.get_children():
		if child is AudioStreamPlayer:
			child.stream = null
	AudioService.set("_seff_cache", {})
	select.queue_free()
	await get_tree().process_frame
	print("DELETE CHARACTER DIALOG PASS: original geometry/font/fill, modal controls, YES/Enter and NO lifecycle")
	get_tree().quit()


func _character_payload(name: String, gender: int, job: int, experience: int) -> PackedByteArray:
	var encoded := name.to_utf8_buffer()
	var payload := PackedByteArray()
	payload.resize(74)
	payload.encode_u16(0, encoded.size())
	for index in range(mini(encoded.size(), 64)):
		payload[2 + index] = encoded[index]
	payload[68] = gender
	payload[69] = job
	payload.encode_u32(70, experience)
	return payload


func _fail(message: String) -> void:
	push_error("DELETE_CHARACTER_DIALOG_SMOKE %s" % message)
	get_tree().quit(1)
