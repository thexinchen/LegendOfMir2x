extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	_refresh()


func _refresh() -> void:
	var xml: String = _state.npc_dialog.get("xmlLayout", "")
	$Dialog.text = _plain_text(xml)


func _plain_text(xml: String) -> String:
	var regex := RegEx.new()
	regex.compile("<[^>]+>")
	return regex.sub(xml.replace("<br>", "\n").replace("<br/>", "\n"), "", true)
