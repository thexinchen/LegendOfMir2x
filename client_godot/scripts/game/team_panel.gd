extends "res://scripts/game/closable_panel.gd"

const LINE_HEIGHT := 16
const MIN_VISIBLE_ROWS := 5
const MAX_VISIBLE_ROWS := 10
const ROW_WIDTH := 231
const BASE_HEIGHT := 146
const ROW_OVERLAY_ALPHA := 100.0 / 255.0

var _state: Node
var _show_candidates := false
var _selected_uids := [0, 0]
var _start_indices := [0, 0]


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$SwitchButton.pressed.connect(_toggle_mode)
	$AddButton.pressed.connect(_join_selected)
	$DeleteButton.pressed.connect(_leave_selected)
	$RefreshButton.pressed.connect(_reset_scroll)
	$MemberRows.gui_input.connect(_on_rows_input)
	_refresh()


func _refresh() -> void:
	_validate_selections()
	var rows := _current_rows()
	var mode := _mode_index()
	var visible_count := clampi(rows.size(), MIN_VISIBLE_ROWS, MAX_VISIBLE_ROWS)
	var max_start := maxi(0, rows.size() - visible_count)
	_start_indices[mode] = clampi(_start_indices[mode], 0, max_start)
	_apply_layout(visible_count)
	for child in $MemberRows.get_children():
		child.free()
	for visible_index in visible_count:
		var item_index: int = visible_index + _start_indices[mode]
		if item_index >= rows.size():
			break
		_add_row(rows[item_index], item_index, visible_index, mode)
	$Title.text = "申请加入" if _show_candidates else "当前队伍"
	$AddButton.disabled = not _show_candidates
	$DeleteButton.disabled = _show_candidates
	$AddButton.modulate = Color.WHITE if _show_candidates else Color(0.5, 0.5, 0.5, 1.0)
	$DeleteButton.modulate = Color(0.5, 0.5, 0.5, 1.0) if _show_candidates else Color.WHITE


func _current_rows() -> Array:
	return _state.team_candidates if _show_candidates else _state.team_members


func _mode_index() -> int:
	return 1 if _show_candidates else 0


func _apply_layout(visible_count: int) -> void:
	var panel_height := BASE_HEIGHT + visible_count * LINE_HEIGHT
	custom_minimum_size = Vector2(258, panel_height)
	size = custom_minimum_size
	$Background.size = size
	$MemberRows.size = Vector2(ROW_WIDTH, visible_count * LINE_HEIGHT)
	var button_y := 94 + visible_count * LINE_HEIGHT
	$SwitchButton.position.y = button_y
	$AddButton.position.y = button_y
	$DeleteButton.position.y = button_y
	$RefreshButton.position.y = button_y
	$CloseButton.position.y = button_y + 8


func _add_row(member: Dictionary, item_index: int, visible_index: int, mode: int) -> void:
	var uid: int = member.get("uid", 0)
	var button := Button.new()
	button.name = "Row%d" % item_index
	button.position = Vector2(0, visible_index * LINE_HEIGHT)
	button.size = Vector2(ROW_WIDTH, LINE_HEIGHT)
	var player_name := str(member.get("name", ""))
	if player_name.is_empty():
		player_name = "PLY_%d" % (uid & 0xFFFFFFFF)
	button.text = "%d %s" % [item_index, player_name]
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.flat = false
	button.add_theme_font_size_override("font_size", 12)
	button.add_theme_color_override("font_color", Color.WHITE)
	button.add_theme_color_override("font_hover_color", Color.WHITE)
	button.add_theme_color_override("font_pressed_color", Color.WHITE)
	var hover_color := Color(0.0, 0.0, 1.0, ROW_OVERLAY_ALPHA)
	button.add_theme_stylebox_override("normal", _row_style(Color.TRANSPARENT))
	button.add_theme_stylebox_override("hover", _row_style(hover_color))
	button.add_theme_stylebox_override("pressed", _row_style(hover_color))
	button.add_theme_stylebox_override("focus", _row_style(Color.TRANSPARENT))
	if _selected_uids[mode] == uid:
		var selected_color := Color(1.0, 0.0, 0.0, ROW_OVERLAY_ALPHA)
		var selected_hover_color := _source_over(hover_color, selected_color)
		button.add_theme_stylebox_override("normal", _row_style(selected_color))
		button.add_theme_stylebox_override("hover", _row_style(selected_hover_color))
		button.add_theme_stylebox_override("pressed", _row_style(selected_hover_color))
	button.pressed.connect(_select_uid.bind(mode, uid))
	$MemberRows.add_child(button)


func _row_style(color: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.content_margin_left = 5.0
	return style


func _source_over(top: Color, bottom: Color) -> Color:
	var output_alpha := top.a + bottom.a * (1.0 - top.a)
	return Color(
		(top.r * top.a + bottom.r * bottom.a * (1.0 - top.a)) / output_alpha,
		(top.g * top.a + bottom.g * bottom.a * (1.0 - top.a)) / output_alpha,
		(top.b * top.a + bottom.b * bottom.a * (1.0 - top.a)) / output_alpha,
		output_alpha,
	)


func _validate_selections() -> void:
	var row_sets := [_state.team_members, _state.team_candidates]
	for mode in 2:
		var selected_uid: int = _selected_uids[mode]
		if selected_uid != 0 and not _contains_uid(row_sets[mode], selected_uid):
			_selected_uids[mode] = 0


func _contains_uid(rows: Array, uid: int) -> bool:
	for row_value in rows:
		var row: Dictionary = row_value
		if int(row.get("uid", 0)) == uid:
			return true
	return false


func _select_uid(mode: int, uid: int) -> void:
	_selected_uids[mode] = uid
	_refresh()


func _toggle_mode() -> void:
	_show_candidates = not _show_candidates
	_refresh()


func _reset_scroll() -> void:
	_start_indices[_mode_index()] = 0
	_refresh()


func _on_rows_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	var delta := 0
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		delta = -1
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		delta = 1
	if delta == 0:
		return
	var rows := _current_rows()
	var mode := _mode_index()
	var visible_count := clampi(rows.size(), MIN_VISIBLE_ROWS, MAX_VISIBLE_ROWS)
	_start_indices[mode] = clampi(_start_indices[mode] + delta, 0, maxi(0, rows.size() - visible_count))
	_refresh()
	accept_event()


func _join_selected() -> void:
	var uid: int = _selected_uids[1]
	if _show_candidates and uid:
		NetworkClient.send_request_join_team(uid)


func _leave_selected() -> void:
	var uid: int = _selected_uids[0]
	if not _show_candidates and uid:
		NetworkClient.send_request_leave_team(uid)
