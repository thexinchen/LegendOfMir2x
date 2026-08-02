extends Control

@export var close_button_path: NodePath
@export var ui_click_button_paths: Array[NodePath] = []
@export var overlay_button_paths: Array[NodePath] = []
@export var silent_overlay_button_paths: Array[NodePath] = []

var _dragging := false


func _ready() -> void:
	get_viewport().size_changed.connect(_clamp_to_viewport)
	resized.connect(_clamp_to_viewport)
	_clamp_to_viewport()
	var close_button := get_node_or_null(close_button_path) as BaseButton
	if close_button:
		close_button.pressed.connect(hide)
	for button_path in ui_click_button_paths:
		var button := get_node(button_path) as BaseButton
		AudioService.bind_ui_click(button)
	for button_path in overlay_button_paths:
		var button := get_node(button_path) as TextureButton
		AudioService.bind_overlay_button(button)
	for button_path in silent_overlay_button_paths:
		var button := get_node(button_path) as TextureButton
		AudioService.bind_overlay_visual(button)
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
		_clamp_to_viewport()
		accept_event()


func _clamp_to_viewport() -> void:
	var viewport_size := get_viewport_rect().size
	position = Vector2(
		clampf(position.x, 0.0, maxf(0.0, viewport_size.x - size.x)),
		clampf(position.y, 0.0, maxf(0.0, viewport_size.y - size.y)),
	)


func _unhandled_key_input(event: InputEvent) -> void:
	if visible and event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE:
		var main := get_parent()
		if main != null and main.has_method("handle_panel_escape"):
			main.call("handle_panel_escape")
		elif close_for_escape():
			get_viewport().set_input_as_handled()


func close_for_escape() -> bool:
	hide()
	return true
