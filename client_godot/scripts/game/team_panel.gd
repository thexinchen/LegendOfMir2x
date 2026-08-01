extends "res://scripts/game/closable_panel.gd"

var _state: Node
var _show_candidates := false
var _selected_uid: int = 0


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$SwitchButton.pressed.connect(func(): _show_candidates = not _show_candidates; _refresh())
	$AddButton.pressed.connect(_join_selected)
	$DeleteButton.pressed.connect(_leave_selected)
	$RefreshButton.pressed.connect(_refresh)
	_refresh()


func _refresh() -> void:
	$Title.text = "申请列表" if _show_candidates else "当前队伍"
	for child in $MemberRows.get_children():
		child.free()
	var rows: Array = _state.team_candidates if _show_candidates else _state.team_members
	for index in range(rows.size()):
		var member: Dictionary = rows[index]
		var button := Button.new()
		button.position = Vector2(4, index * 28)
		button.size = Vector2(238, 26)
		button.text = "%s  Lv.%d%s" % [member.get("name", ""), member.get("level", 0), "  [队长]" if member.get("uid", 0) == _state.team_leader else ""]
		button.pressed.connect(func(): _selected_uid = member.get("uid", 0))
		$MemberRows.add_child(button)


func _join_selected() -> void:
	if _selected_uid:
		NetworkClient.send_request_join_team(_selected_uid)


func _leave_selected() -> void:
	var uid: int = _selected_uid if _selected_uid else _state.player_uid
	NetworkClient.send_request_leave_team(uid)
