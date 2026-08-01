extends Control

@export var close_button_path: NodePath

var _dragging := false


func _ready() -> void:
	var close_button := get_node_or_null(close_button_path) as BaseButton
	if close_button:
		close_button.pressed.connect(hide)
	if OS.has_environment("MIR2X_PANEL_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PANEL_SCREENSHOT"))
		get_tree().quit()


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_dragging = event.pressed and event.position.y <= 42.0
		accept_event()
	elif event is InputEventMouseMotion and _dragging:
		position += event.relative
		accept_event()


func _unhandled_key_input(event: InputEvent) -> void:
	if visible and event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE:
		hide()
		get_viewport().set_input_as_handled()
