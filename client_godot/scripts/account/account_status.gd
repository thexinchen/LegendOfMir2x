extends Label

signal active_changed(active: bool)

var _remaining_ms := 0.0
var _persistent := false


func _ready() -> void:
	hide()


func _process(delta: float) -> void:
	if _persistent or _remaining_ms <= 0.0:
		return
	_remaining_ms = maxf(0.0, _remaining_ms - delta * 1000.0)
	if _remaining_ms == 0.0:
		clear_status()


func show_status(message: String, duration_ms := 0.0) -> void:
	text = message
	_persistent = duration_ms <= 0.0
	_remaining_ms = maxf(0.0, duration_ms)
	show()
	active_changed.emit(true)


func clear_status() -> void:
	if not visible:
		return
	text = ""
	_persistent = false
	_remaining_ms = 0.0
	hide()
	active_changed.emit(false)


func is_active() -> bool:
	return visible
