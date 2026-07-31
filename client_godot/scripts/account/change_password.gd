extends Control

@onready var account_input: LineEdit = %AccountInput
@onready var old_password_input: LineEdit = %OldPasswordInput
@onready var new_password_input: LineEdit = %NewPasswordInput
@onready var confirm_input: LineEdit = %ConfirmInput
@onready var status: Label = %Status


func _ready() -> void:
	account_input.grab_focus()
	NetworkClient.message_received.connect(_on_server_message)
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()


func _on_submit_pressed() -> void:
	if not account_input.text.contains("@"):
		_show_status("无效账号")
	elif old_password_input.text.is_empty():
		_show_status("无效密码")
	elif not _is_valid_password(new_password_input.text):
		_show_status("无效新密码")
	elif new_password_input.text != confirm_input.text:
		_show_status("新密码两次输入不一致")
	elif old_password_input.text == new_password_input.text:
		_show_status("新旧密码相同")
	else:
		_show_status("正在提交")
		var error := NetworkClient.change_password(
			account_input.text,
			old_password_input.text,
			new_password_input.text,
		)
		if error != OK:
			_show_status("服务器尚未连接")


func _on_return_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/login.tscn")


func _is_valid_password(value: String) -> bool:
	var has_digit := false
	var has_lower := false
	var has_upper := false
	var has_special := false
	for index in value.length():
		var code := value.unicode_at(index)
		has_digit = has_digit or (code >= 48 and code <= 57)
		has_upper = has_upper or (code >= 65 and code <= 90)
		has_lower = has_lower or (code >= 97 and code <= 122)
		has_special = has_special or "~!@#$%^&*()".contains(value[index])
	return value.length() >= 8 and has_digit and has_lower and has_upper and has_special


func _show_status(message: String) -> void:
	status.text = message
	status.show()


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	if head_code == NetworkClient.SM_CHANGEPASSWORDOK:
		_show_status("修改密码成功")
	elif head_code == NetworkClient.SM_CHANGEPASSWORDERROR:
		var error_code := payload.decode_u32(0) if payload.size() >= 4 else 0
		var messages := {
			1: "无效的账号",
			2: "无效的密码",
			3: "无效的新密码",
			4: "错误的账号或密码",
		}
		_show_status(messages.get(error_code, "修改密码失败"))
