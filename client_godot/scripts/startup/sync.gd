extends Control

const LOGIN_SCENE := "res://scenes/account/login.tscn"
const LOAD_SECONDS := 1.5

@onready var progress_bar: TextureProgressBar = %ProgressBar

var elapsed_seconds := 0.0
var server_connected := false


func _ready() -> void:
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	if NetworkClient.is_connected_to_server():
		_on_connection_changed(true, "已连接服务器")
	else:
		NetworkClient.connect_to_server()


func _process(delta: float) -> void:
	if OS.has_environment("MIR2X_SCREENSHOT"):
		return
	if not server_connected:
		progress_bar.value = minf(progress_bar.value + delta * 15.0, 85.0)
		return
	elapsed_seconds += delta
	progress_bar.value = minf(elapsed_seconds / LOAD_SECONDS * 100.0, 100.0)
	if progress_bar.value >= 100.0:
		get_tree().change_scene_to_file(LOGIN_SCENE)


func _on_connection_changed(connected: bool, message: String) -> void:
	server_connected = connected
	%Status.text = message
	if connected:
		elapsed_seconds = progress_bar.value / 100.0 * LOAD_SECONDS
