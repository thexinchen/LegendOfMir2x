extends "res://scripts/game/closable_panel.gd"

var _state: Node


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$SortButton.pressed.connect(_sort_items)
	_refresh()


func _refresh() -> void:
	$Gold.text = "%d" % _state.player_gold
	for child in $ItemGrid.get_children():
		child.free()
	var columns := 10
	for index in range(_state.inventory.size()):
		var item: Dictionary = _state.inventory[index]
		var button := Button.new()
		button.position = Vector2((index % columns) * 38, (index / columns) * 38)
		button.size = Vector2(36, 36)
		button.text = str(item.get("itemID", 0))
		button.tooltip_text = "物品 %d\n数量 %d\n序号 %d" % [item.get("itemID", 0), item.get("count", 0), item.get("seqID", 0)]
		$ItemGrid.add_child(button)


func _sort_items() -> void:
	_state.inventory.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		if left.get("itemID", 0) == right.get("itemID", 0):
			return left.get("seqID", 0) < right.get("seqID", 0)
		return left.get("itemID", 0) < right.get("itemID", 0)
	)
	_state.state_changed.emit()
