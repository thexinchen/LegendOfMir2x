extends "res://scripts/game/closable_panel.gd"

const TRACK_LENGTH := 293.0
const KNOB_TOP := 41.0

var _scroll_value := 0.0
var _dragging_slider := false


func _ready() -> void:
	super._ready()
	$SliderHitArea.gui_input.connect(_on_slider_input)
	_update_slider()


func _on_slider_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_dragging_slider = event.pressed
		if event.pressed:
			_set_slider_from_hit_position(event.position.y)
		else:
			_update_slider()
		accept_event()
	elif event is InputEventMouseMotion and _dragging_slider:
		_set_slider_from_hit_position(event.position.y)
		accept_event()


func _set_slider_from_hit_position(hit_y: float) -> void:
	_scroll_value = clampf((hit_y - 12.0) / TRACK_LENGTH, 0.0, 1.0)
	_update_slider()


func _update_slider() -> void:
	$Slider.position = Vector2(559, KNOB_TOP + _scroll_value * TRACK_LENGTH)
	$Slider.modulate = Color.WHITE if _dragging_slider else Color(0.5, 0.5, 0.5, 1.0)
