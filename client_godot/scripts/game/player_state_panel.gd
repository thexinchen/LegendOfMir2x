extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	_refresh()


func _refresh() -> void:
	$Name.text = _state.player_name
	$StateValues.text = "等级  %d\n经验  %d\n生命  %d / %d\n魔法  %d / %d\n金币  %d" % [_state.player_level, _state.player_exp, _state.player_hp, _state.player_hp_max, _state.player_mp, _state.player_mp_max, _state.player_gold]
	$CombatStats.text = "防御  %d - %d\n攻击  %d - %d\n装备  %d / 12" % [_state.ac_min, _state.ac_max, _state.dc_min, _state.dc_max, _state.wear.size()]
