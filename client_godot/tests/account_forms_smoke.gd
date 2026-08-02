extends Node

const Validation = preload("res://scripts/account/account_validation.gd")


func _ready() -> void:
	if not Validation.is_email("test@example.com") or Validation.is_email("bad@") or Validation.is_email("test@domain"):
		_fail("email validation does not match the original account rules")
		return
	if not Validation.is_password("Abcd123!") or Validation.is_password("abcdefgh"):
		_fail("password validation does not match the original account rules")
		return

	var create := load("res://scenes/account/create_account.tscn").instantiate() as Control
	add_child(create)
	await get_tree().process_frame
	var create_account := create.get_node("AccountInput") as LineEdit
	var create_password := create.get_node("PasswordInput") as LineEdit
	var create_confirm := create.get_node("ConfirmInput") as LineEdit
	create_account.text = "bad@"
	create_password.text = "Abcd123!"
	create_confirm.text = "Mismatch1!"
	create.call("_update_checks")
	if not _check_mark(create.get_node("AccountCheck") as Label, "×", Color.RED, Vector2(511, 230)):
		return
	if not _check_mark(create.get_node("PasswordCheck") as Label, "√", Color.GREEN, Vector2(511, 288)):
		return
	if not _check_mark(create.get_node("ConfirmCheck") as Label, "×", Color.RED, Vector2(511, 343)):
		return
	create_confirm.text = create_password.text
	create.call("_update_checks")
	if (create.get_node("ConfirmCheck") as Label).text != "√":
		_fail("matching confirmation did not become valid")
		return
	if OS.has_environment("MIR2X_ACCOUNT_FORMS_SCREENSHOT_DIR"):
		await _capture("create-account.png")

	var create_status := create.get_node("Status") as Label
	create.call("_show_status", "提交中")
	if OS.has_environment("MIR2X_ACCOUNT_FORMS_SCREENSHOT_DIR"):
		await _capture("create-status.png")
	create_status.call("_process", 10.0)
	if not create_status.visible or create_account.editable or not (create.get_node("SubmitButton") as TextureButton).disabled or (create.get_node("PasswordCheck") as Label).visible:
		_fail("persistent create-account status did not lock and hide validation")
		return
	create.call("_show_status", "注册成功", 2000.0)
	create_status.call("_process", 2.1)
	if create_status.visible or not create_account.editable or (create.get_node("SubmitButton") as TextureButton).disabled:
		_fail("timed create-account status did not unlock after 2000 ms")
		return
	create_account.text = "bad@"
	create_password.text = "Abcd123!"
	create_confirm.text = "Abcd123!"
	create.call("_submit")
	if create_status.text != "无效账号" or not create_account.text.is_empty() or not create_password.text.is_empty() or not create_confirm.text.is_empty():
		_fail("invalid account did not clear the complete create form")
		return
	create_status.call("clear_status")
	create_account.text = "test@example.com"
	create_password.text = "Abcd123!"
	create_confirm.text = "Abcd123!"
	create.call("_show_status", "提交中")
	var create_error := PackedByteArray()
	create_error.resize(4)
	create_error.encode_u32(0, 1)
	create.call("_on_server_message", NetworkClient.SM_CREATEACCOUNTERROR, create_error)
	if create_status.text != "账号已存在" or not create_account.text.is_empty() or not create_password.text.is_empty() or not create_confirm.text.is_empty():
		_fail("create-account server error did not clear the form")
		return
	create.queue_free()
	await get_tree().process_frame

	var change := load("res://scenes/account/change_password.tscn").instantiate() as Control
	add_child(change)
	await get_tree().process_frame
	var change_account := change.get_node("AccountInput") as LineEdit
	var old_password := change.get_node("OldPasswordInput") as LineEdit
	var new_password := change.get_node("NewPasswordInput") as LineEdit
	var change_confirm := change.get_node("ConfirmInput") as LineEdit
	change_account.text = "test@example.com"
	old_password.text = "Abcd123!"
	new_password.text = "Efgh456@"
	change_confirm.text = "Efgh456@"
	change.call("_update_checks")
	if not _check_mark(change.get_node("AccountCheck") as Label, "√", Color.GREEN, Vector2(511, 224)):
		return
	if not _check_mark(change.get_node("ConfirmCheck") as Label, "√", Color.GREEN, Vector2(511, 365)):
		return
	if OS.has_environment("MIR2X_ACCOUNT_FORMS_SCREENSHOT_DIR"):
		await _capture("change-password.png")
	var change_status := change.get_node("Status") as Label
	change.call("_show_status", "提交中")
	var change_error := PackedByteArray()
	change_error.resize(4)
	change_error.encode_u32(0, 4)
	change.call("_on_server_message", NetworkClient.SM_CHANGEPASSWORDERROR, change_error)
	if change_status.text != "错误的账号或密码" or not change_account.text.is_empty() or not old_password.text.is_empty() or not new_password.text.is_empty() or not change_confirm.text.is_empty():
		_fail("change-password server error did not clear the form")
		return
	change.queue_free()
	await get_tree().process_frame

	var login := load("res://scenes/account/login.tscn").instantiate() as Control
	add_child(login)
	await get_tree().process_frame
	login.call("_show_notice", "无效的账号或密码")
	var notice := login.get_node("Notice") as Control
	var build_version := login.get_node("BuildVersion") as Label
	if not build_version.text.begins_with("编译版本号:VENGINEERING-") or build_version.text.ends_with("godot"):
		_fail("login still displays a hardcoded build signature: %s" % build_version.text)
		return
	var notice_box: Rect2 = notice.call("notice_box")
	if not notice.visible or not notice.get("draw_background") or notice_box.size.x <= 20.0 or notice_box.size.y <= 20.0 or absf(notice_box.get_center().x - 400.0) > 0.01 or absf(notice_box.get_center().y - 300.0) > 0.01:
		_fail("login notice is not content-sized and screen-centered")
		return
	if OS.has_environment("MIR2X_ACCOUNT_FORMS_SCREENSHOT_DIR"):
		await _capture("login-notice.png")
	notice.call("_process", 5.1)
	if notice.visible:
		_fail("login notice did not expire after 5000 ms")
		return
	login.queue_free()
	await get_tree().process_frame

	var select_character := load("res://scenes/account/select_character.tscn").instantiate() as Control
	var select_notice := select_character.get_node("Notice") as Control
	select_notice.call("show_message", "第一条提示")
	select_notice.call("show_message", "第二条提示")
	var select_entries: Array = select_notice.get("_entries")
	if select_notice.get("entry_limit") != 1 or select_entries.size() != 1 or String(select_entries[0].text) != "第二条提示" or bool(select_notice.get("draw_background")):
		_fail("select-character notice did not preserve original latest-only/background-free configuration: %s" % [select_entries])
		return
	select_character.free()
	AudioService.stop_bgm()
	(AudioService.get_node("BGMPlayer") as AudioStreamPlayer).stream = null
	await get_tree().process_frame
	print("ACCOUNT FORMS PASS: validation marks, modal timed statuses, clearing rules and dynamic notices")
	get_tree().quit()


func _check_mark(label: Label, expected_text: String, expected_color: Color, expected_position: Vector2) -> bool:
	if not label.visible or label.text != expected_text or label.position != expected_position or label.get_theme_color("font_color") != expected_color or label.get_theme_font_size("font_size") != 15:
		_fail("validation mark mismatch: %s %s %s %s" % [label.text, label.position, label.get_theme_color("font_color"), label.get_theme_font_size("font_size")])
		return false
	return true


func _capture(file_name: String) -> void:
	await RenderingServer.frame_post_draw
	var directory := OS.get_environment("MIR2X_ACCOUNT_FORMS_SCREENSHOT_DIR")
	DirAccess.make_dir_recursive_absolute(directory)
	get_viewport().get_texture().get_image().save_png(directory.path_join(file_name))


func _fail(message: String) -> void:
	push_error("ACCOUNT_FORMS_SMOKE %s" % message)
	get_tree().quit(1)
