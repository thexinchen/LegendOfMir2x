extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	_refresh()


func _refresh() -> void:
	var lines: Array[String] = []
	for quest_name in _state.quests:
		lines.append("[color=#ffd86b]%s[/color]" % quest_name)
		for fsm in _state.quests[quest_name]:
			lines.append("  %s" % _state.quests[quest_name][fsm])
	$QuestList.text = "\n".join(lines) if not lines.is_empty() else "暂无任务"
