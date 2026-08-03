extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const EmojiResourceScript = preload("res://scripts/game/emoji_resource.gd")
const MIN_BOARD_WIDTH := 300.0
const MARGIN := 35.0

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _emoji_resources: RefCounted = EmojiResourceScript.new()
var _emoji_frames: Array[Dictionary] = []
var _hover_meta := ""
var _pressed_meta := ""


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	$Dialog.meta_clicked.connect(_on_meta_clicked)
	$Dialog.meta_hover_started.connect(_on_meta_hover_started)
	$Dialog.meta_hover_ended.connect(_on_meta_hover_ended)
	$Dialog.gui_input.connect(_on_dialog_gui_input)
	_refresh()


func _refresh() -> void:
	var dialog: Dictionary = _state.npc_dialog
	var face_frame: Dictionary = _resources.frame("proguse", 0x50000000 | _npc_id(dialog.get("npcUID", 0)))
	$Face.texture = face_frame.get("texture")
	$Face.visible = $Face.texture != null
	_render_dialog(dialog.get("xmlLayout", ""), _hover_meta)
	_apply_layout.call_deferred()


func _process(_delta: float) -> void:
	var now := Time.get_ticks_msec()
	for emoji in _emoji_frames:
		if emoji.frame_count <= 1 or emoji.fps <= 0:
			continue
		var frame_index := floori(float(now - emoji.start_ms) * emoji.fps / 1000.0) % int(emoji.frame_count)
		if frame_index == emoji.frame:
			continue
		emoji.frame = frame_index
		var columns := maxi(1, floori(float(emoji.atlas.atlas.get_width()) / emoji.width))
		var region: Rect2 = emoji.atlas.region
		region.position = Vector2((frame_index % columns) * emoji.width, floori(float(frame_index) / columns) * emoji.height)
		emoji.atlas.region = region


func _build_bbcode(xml: String, hover_meta: String = "", pressed_meta: String = "", line_width_override: float = -1.0) -> String:
	if xml.is_empty():
		return ""
	var parser := XMLParser.new()
	if parser.open_buffer(xml.to_utf8_buffer()) != OK:
		return _escape_bbcode(xml)
	var result := ""
	var tag_stack: Array[String] = []
	var no_wrap_depth := 0
	var line_width: float = line_width_override if line_width_override > 0.0 else _dialog_line_width()
	var current_line_width := 0.0
	var atomic_result_start := -1
	var atomic_text := ""
	while parser.read() == OK:
		match parser.get_node_type():
			XMLParser.NODE_ELEMENT:
				var name := parser.get_node_name().to_lower()
				if name == "par":
					if not result.is_empty() and not result.ends_with("\n"):
						result += "\n"
					current_line_width = 0.0
					var par_tags: Array[String] = []
					var align := _xml_attribute(parser, "align", "justify").to_lower()
					if align == "center":
						result += "[center]"
						par_tags.append("center")
					elif align == "right":
						result += "[right]"
						par_tags.append("right")
					elif align == "distributed":
						result += "[fill]"
						par_tags.append("fill")
					var font_size := int(_xml_attribute(parser, "size", "15"))
					if font_size != 15:
						result += "[font_size=%d]" % max(1, font_size)
						par_tags.append("font_size")
					var par_color := _xml_attribute(parser, "color", "")
					if not par_color.is_empty():
						result += "[color=%s]" % _bbcode_color(par_color)
						par_tags.append("color")
					var par_bgcolor := _xml_attribute(parser, "bgcolor", "")
					if not par_bgcolor.is_empty():
						result += "[bgcolor=%s]" % _bbcode_color(par_bgcolor)
						par_tags.append("bgcolor")
					tag_stack.append("par:%s" % ",".join(par_tags))
				elif name == "event":
					var wrap_enabled := _parse_bool(_xml_attribute(parser, "wrap", "true"))
					if not wrap_enabled:
						atomic_result_start = result.length()
						atomic_text = ""
					var event := {
						"id": _xml_attribute(parser, "id", ""),
						"path": _xml_attribute(parser, "path", ""),
						"args": _xml_optional_attribute(parser, "args"),
						"close": _parse_bool(_xml_attribute(parser, "close", "0")),
					}
					var meta := JSON.stringify(event)
					var event_color := "#ff00ff" if meta == pressed_meta else ("#00ff00" if meta == hover_meta else "#ffff00")
					result += "[color=%s][url=%s]" % [event_color, meta]
					tag_stack.append("event" if wrap_enabled else "event-nowrap")
					if not wrap_enabled:
						no_wrap_depth += 1
				elif name == "t":
					var text_tags: Array[String] = []
					var text_color := _xml_attribute(parser, "color", "")
					if not text_color.is_empty():
						result += "[color=%s]" % _bbcode_color(text_color)
						text_tags.append("color")
					var text_bgcolor := _xml_attribute(parser, "bgcolor", "")
					if not text_bgcolor.is_empty():
						result += "[bgcolor=%s]" % _bbcode_color(text_bgcolor)
						text_tags.append("bgcolor")
					tag_stack.append("t:%s" % ",".join(text_tags))
				elif name == "emoji":
					var emoji_id := int(_xml_attribute(parser, "id", "0"))
					result += "[[MIR2X_EMOJI:%d]]" % emoji_id
					var emoji_definition := _emoji_definition(emoji_id)
					if not emoji_definition.is_empty():
						current_line_width = _advance_object_width(current_line_width, emoji_definition.width, line_width)
					tag_stack.append("emoji")
				else:
					tag_stack.append(name)
				if parser.is_empty():
					var empty_tag: String = tag_stack.pop_back()
					if empty_tag == "event-nowrap":
						var empty_placement: Array = _place_atomic_text(result, atomic_result_start, atomic_text, current_line_width, line_width)
						current_line_width = empty_placement[0]
						result = empty_placement[1]
						atomic_result_start = -1
						atomic_text = ""
					result = _close_tag(result, empty_tag)
					if empty_tag == "event-nowrap":
						no_wrap_depth -= 1
			XMLParser.NODE_ELEMENT_END:
				if not tag_stack.is_empty():
					var closed_tag: String = tag_stack.pop_back()
					if closed_tag == "event-nowrap":
						var placement: Array = _place_atomic_text(result, atomic_result_start, atomic_text, current_line_width, line_width)
						current_line_width = placement[0]
						result = placement[1]
						atomic_result_start = -1
						atomic_text = ""
					result = _close_tag(result, closed_tag)
					if closed_tag == "event-nowrap":
						no_wrap_depth -= 1
			XMLParser.NODE_TEXT:
				var text := parser.get_node_data()
				result += _escape_bbcode(text)
				if no_wrap_depth > 0:
					atomic_text += text
				else:
					current_line_width = _advance_line_width(current_line_width, text, line_width)
	return result


func _close_tag(result: String, name: String) -> String:
	if name.begins_with("par:") or name.begins_with("t:"):
		var is_paragraph := name.begins_with("par:")
		var tags := name.substr(name.find(":") + 1).split(",", false)
		tags.reverse()
		for tag in tags:
			result += "[/%s]" % tag
		if is_paragraph and not result.ends_with("\n"):
			result += "\n"
		return result
	match name:
		"par":
			if not result.ends_with("\n"):
				result += "\n"
		"event", "event-nowrap":
			result += "[/url][/color]"
	return result


func _apply_layout() -> void:
	var face_size := Vector2.ZERO
	if $Face.visible:
		face_size = $Face.texture.get_size()
	var board_width := maxf(get_viewport_rect().size.x / 3.0, MIN_BOARD_WIDTH)
	var line_width := board_width - MARGIN * (3.0 if $Face.visible else 2.0) - face_size.x
	line_width = maxf(1.0, line_width)
	$Dialog.size = Vector2(line_width, 1.0)
	var content_width := clampf(ceilf($Dialog.get_content_width()), 1.0, line_width)
	$Dialog.size = Vector2(content_width, 1.0)
	var content_height := maxf(1.0, ceilf($Dialog.get_content_height()))
	var panel_width := MARGIN * (3.0 if $Face.visible else 2.0) + face_size.x + content_width
	var panel_height := MARGIN * 2.0 + maxf(face_size.y, content_height)
	custom_minimum_size = Vector2.ZERO
	size = Vector2(panel_width, panel_height)
	custom_minimum_size = size
	$Upper.size = Vector2(panel_width, panel_height - 44.0)
	$Lower.position = Vector2(0.0, panel_height - 44.0)
	$Lower.size = Vector2(panel_width, 44.0)
	$Face.position = Vector2(MARGIN, MARGIN)
	$Face.size = face_size
	$Dialog.position = Vector2(MARGIN * 2.0 + face_size.x if $Face.visible else MARGIN, (panel_height - content_height) * 0.5)
	$Dialog.size = Vector2(content_width, content_height)
	$CloseButton.position = Vector2(panel_width - 40.0, panel_height - 43.0)


func _npc_id(uid: int) -> int:
	return (uid >> 35) & 0xFFFFFF


func _xml_attribute(parser: XMLParser, name: String, fallback: String) -> String:
	for index in parser.get_attribute_count():
		if parser.get_attribute_name(index) == name:
			return parser.get_attribute_value(index)
	return fallback


func _xml_optional_attribute(parser: XMLParser, name: String) -> Variant:
	for index in parser.get_attribute_count():
		if parser.get_attribute_name(index) == name:
			return parser.get_attribute_value(index)
	return null


func _bbcode_color(value: String) -> String:
	var color := value.to_lower()
	if color.begins_with("rgb("):
		var regex := RegEx.new()
		regex.compile("0x([0-9a-fA-F]{2})")
		var matches := regex.search_all(color)
		if matches.size() == 3:
			return "#%s%s%s" % [matches[0].get_string(1), matches[1].get_string(1), matches[2].get_string(1)]
	return color


func _escape_bbcode(text: String) -> String:
	return text.replace("[", "[lb]").replace("]", "[rb]")


func _render_dialog(xml: String, hover_meta: String = "", pressed_meta: String = "") -> void:
	var bbcode := _build_bbcode(xml, hover_meta, pressed_meta)
	$Dialog.clear()
	_emoji_frames.clear()
	var cursor := 0
	while cursor < bbcode.length():
		var marker_start := bbcode.find("[[MIR2X_EMOJI:", cursor)
		if marker_start < 0:
			$Dialog.append_text(bbcode.substr(cursor))
			break
		$Dialog.append_text(bbcode.substr(cursor, marker_start - cursor))
		var marker_end := bbcode.find("]]", marker_start)
		if marker_end < 0:
			$Dialog.append_text(bbcode.substr(marker_start))
			break
		var emoji_id := int(bbcode.substr(marker_start + 14, marker_end - marker_start - 14))
		var emoji: Dictionary = _emoji_resources.frame(emoji_id)
		if not emoji.is_empty():
			$Dialog.add_image(emoji.texture, emoji.width, emoji.height)
			emoji["atlas"] = emoji.texture
			emoji["start_ms"] = Time.get_ticks_msec()
			emoji["frame"] = 0
			_emoji_frames.append(emoji)
		cursor = marker_end + 2


func _emoji_definition(emoji_id: int) -> Dictionary:
	return _emoji_resources.definition(emoji_id)


func _dialog_line_width() -> float:
	var face_width: float = float($Face.texture.get_width()) if $Face.visible and $Face.texture != null else 0.0
	var board_width := maxf(get_viewport_rect().size.x / 3.0, MIN_BOARD_WIDTH)
	return maxf(1.0, board_width - MARGIN * (3.0 if $Face.visible else 2.0) - face_width)


func _text_width(text: String) -> float:
	return $Dialog.get_theme_font("normal_font").get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, $Dialog.get_theme_font_size("normal_font_size")).x


func _advance_line_width(current: float, text: String, line_width: float) -> float:
	for index in text.length():
		var character: String = text[index]
		if character == "\n":
			current = 0.0
			continue
		var character_width: float = _text_width(character)
		current = character_width if current > 0.0 and current + character_width > line_width else current + character_width
	return current


func _advance_object_width(current: float, width: float, line_width: float) -> float:
	return width if current > 0.0 and current + width > line_width else current + width


func _place_atomic_text(bbcode: String, start: int, text: String, current: float, line_width: float) -> Array:
	var atomic_width: float = _text_width(text)
	if current > 0.0 and current + atomic_width > line_width and atomic_width <= line_width:
		bbcode = bbcode.insert(start, "\n")
		current = 0.0
	return [_advance_line_width(current, text, line_width), bbcode]


func _parse_bool(value: String) -> bool:
	return value.to_lower() in ["1", "true", "yes", "on"]


func _on_meta_clicked(meta: Variant) -> void:
	var event = JSON.parse_string(str(meta))
	if not event is Dictionary or event.get("id", "").is_empty():
		return
	var path: String = event.get("path", "")
	if path.is_empty():
		path = _state.npc_dialog.get("eventPath", "")
	NetworkClient.send_npc_event(
		_state.npc_dialog.get("npcUID", 0),
		path,
		event.get("id", ""),
		event.get("args", ""),
	)
	if event.get("close", false):
		hide()


func _on_meta_hover_started(meta: Variant) -> void:
	_hover_meta = str(meta)
	_render_dialog(_state.npc_dialog.get("xmlLayout", ""), _hover_meta)


func _on_meta_hover_ended(meta: Variant) -> void:
	if _hover_meta != str(meta):
		return
	_hover_meta = ""
	_render_dialog(_state.npc_dialog.get("xmlLayout", ""))


func _on_dialog_gui_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT:
		return
	_pressed_meta = _hover_meta if event.pressed else ""
	_render_dialog(_state.npc_dialog.get("xmlLayout", ""), _hover_meta, _pressed_meta)
