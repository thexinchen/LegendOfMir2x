extends Control

signal confirmed(password: String)
signal canceled

@onready var password_input: LineEdit = %PasswordInput


func _ready() -> void:
	if OS.has_environment("MIR2X_DELETE_DIALOG_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(
			OS.get_environment("MIR2X_DELETE_DIALOG_SCREENSHOT"),
		)
		get_tree().quit()


func open() -> void:
	password_input.clear()
	show()
	password_input.grab_focus()


func _on_yes_pressed() -> void:
	confirmed.emit(password_input.text)
	hide()


func _on_password_text_submitted(_text: String) -> void:
	_on_yes_pressed()


func _on_no_pressed() -> void:
	password_input.clear()
	canceled.emit()
	hide()
