extends Control

const AutoLogin = preload("res://scripts/account/auto_login.gd")
const NEXT_SCENE := "res://scenes/startup/sync.tscn"
const SHOW_TIME_SECONDS := 5.0

var elapsed_seconds := 0.0
var auto_login_requested := false


func _ready() -> void:
	auto_login_requested = _has_auto_login()
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()


func _process(delta: float) -> void:
	if auto_login_requested:
		get_tree().change_scene_to_file(NEXT_SCENE)
		return
	elapsed_seconds += delta
	if elapsed_seconds >= SHOW_TIME_SECONDS:
		get_tree().change_scene_to_file(NEXT_SCENE)
	else:
		var ratio: float = clampf(elapsed_seconds / SHOW_TIME_SECONDS, 0.0, 1.0)
		var alpha: float
		if ratio < 0.3:
			alpha = ratio / 0.3
		elif ratio > 0.6:
			alpha = (1.0 - ratio) / 0.4
		else:
			alpha = 1.0
		$Background.modulate.a = alpha


func _has_auto_login() -> bool:
	return AutoLogin.requested()


func _unhandled_input(event: InputEvent) -> void:
	if event.is_pressed() and (event is InputEventKey) and (event.keycode == KEY_SPACE or event.keycode == KEY_ESCAPE):
		get_tree().change_scene_to_file(NEXT_SCENE)
