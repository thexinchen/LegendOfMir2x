class_name ItemTooltipRenderer
extends RefCounted

const BACKGROUND := Color(0, 0, 0, 200.0 / 255.0)
const BORDER := Color(231.0 / 255.0, 231.0 / 255.0, 189.0 / 255.0, 200.0 / 255.0)
const ITEM_TOOLTIP_FONT: Font = preload("res://assets/font/01_Yahei.ttf")


static func configure(panel: Panel, bordered: bool, rounded: bool) -> void:
	var style := StyleBoxFlat.new()
	style.bg_color = BACKGROUND
	if bordered:
		style.border_width_left = 1
		style.border_width_top = 1
		style.border_width_right = 1
		style.border_width_bottom = 1
		style.border_color = BORDER
	if rounded:
		style.corner_radius_top_left = 5
		style.corner_radius_top_right = 5
		style.corner_radius_bottom_right = 5
		style.corner_radius_bottom_left = 5
	panel.add_theme_stylebox_override("panel", style)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.z_index = 4096
	panel.z_as_relative = false
	panel.hide()


static func show_lines(panel: Panel, lines: Array[String], width: float, min_height: float, padding: Vector2, line_height: float, font_size: int, fixed_height := 0.0) -> void:
	for child in panel.get_children():
		child.free()
	var height := fixed_height if fixed_height > 0.0 else maxf(min_height, padding.y * 2.0 + lines.size() * line_height)
	panel.size = Vector2(width, height)
	for index in range(lines.size()):
		var label := Label.new()
		label.position = Vector2(padding.x, padding.y + index * line_height)
		label.size = Vector2(width - padding.x * 2.0, line_height)
		label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		label.add_theme_font_override("font", ITEM_TOOLTIP_FONT)
		label.add_theme_font_size_override("font_size", font_size)
		label.add_theme_color_override("font_color", Color.WHITE)
		label.text = lines[index]
		panel.add_child(label)
	panel.show()


static func update_position(panel: Panel, clamp_to_viewport: bool, mouse_position: Variant = null) -> void:
	var viewport := panel.get_viewport()
	var mouse: Vector2 = viewport.get_mouse_position() if mouse_position == null else mouse_position
	if clamp_to_viewport:
		var viewport_size := panel.get_viewport_rect().size
		mouse = Vector2(
			clampf(mouse.x, 0.0, maxf(0.0, viewport_size.x - panel.size.x)),
			clampf(mouse.y, 0.0, maxf(0.0, viewport_size.y - panel.size.y)),
		)
	panel.global_position = mouse
