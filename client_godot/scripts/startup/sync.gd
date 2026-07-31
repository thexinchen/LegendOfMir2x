extends Control

const LOGIN_SCENE := "res://scenes/account/login.tscn"

@onready var progress_bar: TextureProgressBar = %ProgressBar
@onready var status_label: Label = %Status

var ratio := 0


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
		progress_bar.value = ratio


func _on_connection_changed(_connected: bool, _message: String) -> void:
	pass
