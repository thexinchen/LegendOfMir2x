extends "res://scripts/game/closable_panel.gd"

const SYS_QSTFSM := "_RSVD_NAME_QST_FSM_4194347313"
const LINE_HEIGHT := 17.0
const CONTENT_WIDTH := 270.0
const CONTENT_HEIGHT := 300.0
const TRACK_LENGTH := 213.0

var _state: Node
var _folded: Dictionary = {}
var _scroll_value := 0.0
var _content_height := CONTENT_HEIGHT
var _last_reset_serial := -1
var _dragging_slider := false


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$SliderHitArea.gui_input.connect(_on_slider_input)
	$ContentViewport.gui_input.connect(_on_content_input)
	_refresh()


func _refresh() -> void:
	if _last_reset_serial != _state.quest_reset_serial:
		_last_reset_serial = _state.quest_reset_serial
		_folded.clear()
		_scroll_value = 0.0
	for quest_name in _state.quests:
		if not _folded.has(quest_name):
			_folded[quest_name] = true
	for quest_name in _folded.keys():
		if not _state.quests.has(quest_name):
			_folded.erase(quest_name)
	_rebuild_lines()


func _rebuild_lines() -> void:
	for child in $ContentViewport/Lines.get_children():
		child.free()
	var lines: Array[Dictionary] = []
	var quest_names: Array = _state.quests.keys()
	quest_names.sort()
	for quest_name_value in quest_names:
		var quest_name := str(quest_name_value)
		lines.append({"text": quest_name, "quest": quest_name, "heading": true})
		if _folded.get(quest_name, true):
			continue
		var state_map: Dictionary = _state.quests[quest_name]
		var main_desp := str(state_map.get(SYS_QSTFSM, ""))
		if main_desp.is_empty():
			main_desp = "暂无任务描述"
		lines.append_array(_wrapped_lines("    " + main_desp))
		var fsm_names: Array = state_map.keys()
		fsm_names.erase(SYS_QSTFSM)
		fsm_names.sort()
		for fsm_value in fsm_names:
			var fsm := str(fsm_value)
			lines.append_array(_wrapped_lines("    * " + fsm))
			var desp := str(state_map.get(fsm, ""))
			lines.append_array(_wrapped_lines("      " + (desp if not desp.is_empty() else "暂无任务描述")))
	_content_height = maxf(CONTENT_HEIGHT, lines.size() * LINE_HEIGHT + 5.0)
	for index in lines.size():
		_add_line(lines[index], index)
	_update_scroll()


func _wrapped_lines(text: String) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	var line := ""
	var font := get_theme_default_font()
	for index in text.length():
		var glyph := text.substr(index, 1)
		if glyph == "\n":
			result.append({"text": line, "heading": false})
			line = ""
			continue
		var candidate := line + glyph
		if not line.is_empty() and font.get_string_size(candidate, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x > CONTENT_WIDTH:
			result.append({"text": line, "heading": false})
			line = glyph
		else:
			line = candidate
	if not line.is_empty() or result.is_empty():
		result.append({"text": line, "heading": false})
	return result


func _add_line(line: Dictionary, index: int) -> void:
	var label := Label.new()
	label.position = Vector2(0, index * LINE_HEIGHT)
	label.size = Vector2(CONTENT_WIDTH, LINE_HEIGHT)
	label.text = line.get("text", "")
	label.add_theme_font_size_override("font_size", 12)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	$ContentViewport/Lines.add_child(label)
	if line.get("heading", false):
		var button := Button.new()
		button.name = "Heading%d" % index
		button.position = label.position
		button.size = label.size
		button.flat = true
		button.text = ""
		button.pressed.connect(_toggle_quest.bind(line.get("quest", "")))
		$ContentViewport/Lines.add_child(button)


func _toggle_quest(quest_name: String) -> void:
	_folded[quest_name] = not _folded.get(quest_name, true)
	_rebuild_lines()


func _max_offset() -> float:
	return maxf(0.0, _content_height - CONTENT_HEIGHT)


func _update_scroll() -> void:
	$ContentViewport/Lines.position.y = -_scroll_value * _max_offset()
	$Slider.position = Vector2(321, 148 + _scroll_value * TRACK_LENGTH)
	$Slider.modulate = Color.WHITE if _dragging_slider else Color(0.5, 0.5, 0.5, 1.0)


func _on_content_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed or _max_offset() <= 0.0:
		return
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		_scroll_value = clampf(_scroll_value - LINE_HEIGHT / _max_offset(), 0.0, 1.0)
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_scroll_value = clampf(_scroll_value + LINE_HEIGHT / _max_offset(), 0.0, 1.0)
	else:
		return
	_update_scroll()
	accept_event()


func _on_slider_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_dragging_slider = event.pressed
		if event.pressed:
			_set_slider_from_hit_y(event.position.y)
		else:
			_update_scroll()
		accept_event()
	elif event is InputEventMouseMotion and _dragging_slider:
		_set_slider_from_hit_y(event.position.y)
		accept_event()


func _set_slider_from_hit_y(hit_y: float) -> void:
	_scroll_value = clampf((hit_y - 12.0) / TRACK_LENGTH, 0.0, 1.0)
	_update_scroll()
