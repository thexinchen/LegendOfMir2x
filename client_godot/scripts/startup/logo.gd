extends Control

const NEXT_SCENE := "res://scenes/startup/sync.tscn"
const SHOW_TIME_SECONDS := 2.0

var elapsed_seconds := 0.0


func _ready() -> void:
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()


func _process(delta: float) -> void:
	elapsed_seconds += delta
	if elapsed_seconds >= SHOW_TIME_SECONDS:
		get_tree().change_scene_to_file(NEXT_SCENE)


func _unhandled_input(event: InputEvent) -> void:
	if event.is_pressed():
		get_tree().change_scene_to_file(NEXT_SCENE)
