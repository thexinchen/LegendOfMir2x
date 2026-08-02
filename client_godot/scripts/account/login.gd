extends Control

@onready var account_input: LineEdit = %AccountInput
@onready var password_input: LineEdit = %PasswordInput
@onready var notice: Control = %Notice
@onready var build_version: Label = %BuildVersion

var auto_login_sent := false


func _ready() -> void:
	AudioService.play_map_bgm(0x00040007)
	build_version.text = "编译版本号:%s" % _get_build_signature()
	account_input.grab_focus()
	NetworkClient.message_received.connect(_on_server_message)
	NetworkClient.connection_changed.connect(_on_connection_changed)
	if not NetworkClient.is_connected_to_server():
		NetworkClient.connect_to_server()
	else:
		_try_auto_login()
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var image := get_viewport().get_texture().get_image()
		image.save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()


func _get_build_signature() -> String:
	var signature := FileAccess.get_file_as_string("res://assets/generated/build_signature.txt").strip_edges()
	return signature if not signature.is_empty() else "VENGINEERING-development"


func _on_login_pressed() -> void:
	if account_input.text.is_empty() or password_input.text.is_empty():
		_show_notice("无效的账号或密码")
	else:
		_hide_notice()
		var error := NetworkClient.login(account_input.text, password_input.text)
		if error != OK:
			_show_notice("服务器尚未连接")


func _on_exit_pressed() -> void:
	get_tree().quit()


func _on_create_account_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/create_account.tscn")


func _on_change_password_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/change_password.tscn")


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	if head_code == NetworkClient.SM_LOGINOK:
		get_tree().change_scene_to_file("res://scenes/account/select_character.tscn")
	elif head_code == NetworkClient.SM_LOGINERROR:
		_show_notice("该账号已经登录" if not payload.is_empty() and payload[0] == 2 else "无效的账号或密码")


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected:
		_try_auto_login()


func _try_auto_login() -> void:
	if auto_login_sent:
		return
	var credentials: Array = []
	# Check command line argument --auto-login=user:password
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--auto-login="):
			credentials = arg.substr("--auto-login=".length()).split(":", true, 1)
			break
	# Also check environment variable MIR2X_AUTO_LOGIN
	if credentials.size() != 2 and OS.has_environment("MIR2X_AUTO_LOGIN"):
		credentials = OS.get_environment("MIR2X_AUTO_LOGIN").split(":", true, 1)
	if credentials.size() != 2:
		return
	auto_login_sent = true
	account_input.text = credentials[0]
	password_input.text = credentials[1]
	_on_login_pressed()


func _show_notice(message: String) -> void:
	notice.show_message(message)


func _hide_notice() -> void:
	notice.clear_messages()
