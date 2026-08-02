extends Control

const LOGIN_SCENE := "res://scenes/account/login.tscn"

@onready var progress_bar: Control = %ProgressBar
@onready var progress_clip: Control = $ProgressBar/Clip
@onready var status_label: Label = %Status

var ratio := 0
var _login_requested := false


func _ready() -> void:
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	if NetworkClient.is_connected_to_server():
		_on_connection_changed(true, "")
	else:
		NetworkClient.connect_to_server()


func _process(delta: float) -> void:
	if OS.has_environment("MIR2X_SCREENSHOT"):
		return
	if ratio >= 100:
		get_tree().change_scene_to_file(LOGIN_SCENE)
	elif delta > 0.0:
		ratio += 1
		_update_progress()


func _update_progress() -> void:
	progress_clip.size.x = progress_bar.size.x * clampf(ratio / 100.0, 0.0, 1.0)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.is_pressed() and event.keycode == KEY_ESCAPE and not _login_requested:
		_login_requested = true
		_open_login.call_deferred()


func _open_login() -> void:
	if _login_requested:
		get_tree().change_scene_to_file(LOGIN_SCENE)


func _on_connection_changed(_connected: bool, _message: String) -> void:
	pass
