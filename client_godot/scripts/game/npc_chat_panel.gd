extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$Dialog.meta_clicked.connect(_on_meta_clicked)
	_refresh()


func _refresh() -> void:
	$Dialog.text = _build_bbcode(_state.npc_dialog.get("xmlLayout", ""))


func _build_bbcode(xml: String) -> String:
	if xml.is_empty():
		return ""
	var parser := XMLParser.new()
	if parser.open_buffer(xml.to_utf8_buffer()) != OK:
		return _escape_bbcode(xml)
	var result := ""
	var tag_stack: Array[String] = []
	while parser.read() == OK:
		match parser.get_node_type():
			XMLParser.NODE_ELEMENT:
				var name := parser.get_node_name().to_lower()
				if name == "par":
					if not result.is_empty() and not result.ends_with("\n"):
						result += "\n"
					tag_stack.append("par")
				elif name == "event":
					var event := {
						"id": _xml_attribute(parser, "id", ""),
						"path": _xml_attribute(parser, "path", ""),
						"args": _xml_attribute(parser, "args", ""),
						"close": _parse_bool(_xml_attribute(parser, "close", "0")),
					}
					result += "[color=#ffd86b][url=%s]" % JSON.stringify(event)
					tag_stack.append("event")
				elif name == "t":
					result += "[color=%s]" % _bbcode_color(_xml_attribute(parser, "color", "white"))
					tag_stack.append("t")
				elif name == "emoji":
					result += "☺"
					tag_stack.append("emoji")
				else:
					tag_stack.append(name)
				if parser.is_empty():
					result = _close_tag(result, tag_stack.pop_back())
			XMLParser.NODE_ELEMENT_END:
				if not tag_stack.is_empty():
					result = _close_tag(result, tag_stack.pop_back())
			XMLParser.NODE_TEXT:
				result += _escape_bbcode(parser.get_node_data())
	return result.strip_edges(false, true)


func _close_tag(result: String, name: String) -> String:
	match name:
		"par":
			if not result.ends_with("\n"):
				result += "\n"
		"event":
			result += "[/url][/color]"
		"t":
			result += "[/color]"
	return result


func _xml_attribute(parser: XMLParser, name: String, fallback: String) -> String:
	for index in parser.get_attribute_count():
		if parser.get_attribute_name(index) == name:
			return parser.get_attribute_value(index)
	return fallback


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
