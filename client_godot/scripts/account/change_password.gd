extends Control

const Validation = preload("res://scripts/account/account_validation.gd")

@onready var account_input: LineEdit = %AccountInput
@onready var old_password_input: LineEdit = %OldPasswordInput
@onready var new_password_input: LineEdit = %NewPasswordInput
@onready var confirm_input: LineEdit = %ConfirmInput
@onready var status: Label = %Status
@onready var submit_button: TextureButton = $SubmitButton
@onready var account_check: Label = %AccountCheck
@onready var old_password_check: Label = %OldPasswordCheck
@onready var new_password_check: Label = %NewPasswordCheck
@onready var confirm_check: Label = %ConfirmCheck


func _ready() -> void:
	status.active_changed.connect(_on_status_active_changed)
	_update_checks()
	account_input.grab_focus()
	NetworkClient.message_received.connect(_on_server_message)
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()


func _on_submit_pressed() -> void:
	_submit()


func _on_account_text_submitted(_text: String) -> void:
	_submit()


func _on_old_password_text_submitted(_text: String) -> void:
	_submit()


func _on_new_password_text_submitted(_text: String) -> void:
	_submit()


func _on_confirm_text_submitted(_text: String) -> void:
	_submit()


func _submit() -> void:
	if status.is_active():
		return
	if not Validation.is_email(account_input.text):
		_show_status("无效账号", 2000.0)
		_clear_all()
	elif not Validation.is_password(old_password_input.text):
		_show_status("无效密码", 2000.0)
		old_password_input.clear()
		new_password_input.clear()
		confirm_input.clear()
	elif not Validation.is_password(new_password_input.text):
		_show_status("无效新密码", 2000.0)
		new_password_input.clear()
		confirm_input.clear()
	elif new_password_input.text != confirm_input.text:
		_show_status("新密码两次输入不一致", 2000.0)
		new_password_input.clear()
		confirm_input.clear()
	elif old_password_input.text == new_password_input.text:
		_show_status("新旧密码相同", 2000.0)
		new_password_input.clear()
		confirm_input.clear()
	else:
		_show_status("提交中")
		var error := NetworkClient.change_password(
			account_input.text,
			old_password_input.text,
			new_password_input.text,
		)
		if error != OK:
			_show_status("服务器尚未连接", 2000.0)


func _on_return_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/login.tscn")


func _show_status(message: String, duration_ms := 0.0) -> void:
	status.show_status(message, duration_ms)


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	if head_code == NetworkClient.SM_CHANGEPASSWORDOK:
		_show_status("修改密码成功", 2000.0)
	elif head_code == NetworkClient.SM_CHANGEPASSWORDERROR:
		var error_code := payload.decode_u32(0) if payload.size() >= 4 else 0
		var messages := {
			1: "无效的账号",
			2: "无效的密码",
			3: "无效的新密码",
			4: "错误的账号或密码",
		}
		_clear_all()
		_show_status(messages.get(error_code, "修改密码失败"), 2000.0)


func _clear_all() -> void:
	account_input.clear()
	old_password_input.clear()
	new_password_input.clear()
	confirm_input.clear()


func _on_form_text_changed() -> void:
	_update_checks()


func _on_status_active_changed(active: bool) -> void:
	account_input.editable = not active
	old_password_input.editable = not active
	new_password_input.editable = not active
	confirm_input.editable = not active
	submit_button.disabled = active
	_update_checks()


func _update_checks() -> void:
	var active: bool = status.is_active()
	_set_check(account_check, account_input.text, Validation.is_email(account_input.text), active)
	_set_check(old_password_check, old_password_input.text, Validation.is_password(old_password_input.text), active)
	_set_check(new_password_check, new_password_input.text, Validation.is_password(new_password_input.text), active)
	_set_check(confirm_check, confirm_input.text, Validation.is_password(confirm_input.text) and confirm_input.text == new_password_input.text, active)


func _set_check(label: Label, value: String, valid: bool, status_active: bool) -> void:
	label.visible = not status_active and not value.is_empty()
	label.text = "√" if valid else "×"
	label.add_theme_color_override("font_color", Color.GREEN if valid else Color.RED)
