extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$Input.text_submitted.connect(_send)
	_refresh()


func _refresh() -> void:
	var lines: Array[String] = []
	for entry in _state.chat_log:
		lines.append(entry.get("text", ""))
	$MessageList.text = "\n".join(lines)


func _send(text: String) -> void:
	if text.strip_edges().is_empty():
		return
	NetworkClient.send_player_say(text)
	$Input.clear()
