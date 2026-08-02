extends Control

@export var draw_background := true

const FONT_SIZE := 15
const DURATION_MS := 5000.0
const LIMIT := 10

var _entries: Array[Dictionary] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	hide()


func _process(delta: float) -> void:
	for entry in _entries:
		entry.remaining_ms = float(entry.remaining_ms) - delta * 1000.0
	while not _entries.is_empty() and float(_entries.front().remaining_ms) <= 0.0:
		_entries.pop_front()
	visible = not _entries.is_empty()
	if visible:
		queue_redraw()


func show_message(message: String, duration_ms := DURATION_MS) -> void:
	_entries.append({"text": message, "remaining_ms": duration_ms})
	while _entries.size() > LIMIT:
		_entries.pop_front()
	show()
	queue_redraw()


func clear_messages() -> void:
	_entries.clear()
	hide()
	queue_redraw()


func notice_box() -> Rect2:
	if _entries.is_empty():
		return Rect2()
	var font := get_theme_default_font()
	var line_height := font.get_height(FONT_SIZE)
	var max_width := 0.0
	for entry in _entries:
		max_width = maxf(max_width, font.get_string_size(String(entry.text), HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x)
	var total_height := line_height * _entries.size()
	return Rect2(
		Vector2((size.x - max_width) * 0.5 - 10.0, (size.y - total_height) * 0.5 - 10.0),
		Vector2(max_width + 20.0, total_height + 20.0),
	)


func _draw() -> void:
	if _entries.is_empty():
		return
	var font := get_theme_default_font()
	var line_height := font.get_height(FONT_SIZE)
	var box := notice_box()
	if draw_background:
		var style := StyleBoxFlat.new()
		style.bg_color = Color(0, 0, 0, 128.0 / 255.0)
		style.border_color = Color(0, 0, 1, 128.0 / 255.0)
		style.set_border_width_all(1)
		style.set_corner_radius_all(8)
		draw_style_box(style, box)
	var draw_y := box.position.y + 10.0 + font.get_ascent(FONT_SIZE)
	for entry in _entries:
		var message := String(entry.text)
		var width := font.get_string_size(message, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x
		draw_string(font, Vector2((size.x - width) * 0.5, draw_y), message, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, Color.YELLOW)
		draw_y += line_height
