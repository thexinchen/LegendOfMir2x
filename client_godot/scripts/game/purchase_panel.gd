extends "res://scripts/game/closable_panel.gd"

var _state: Node
var _selected_item_id := 0


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$SelectButton.pressed.connect(_buy_selected)
	_refresh()


func _refresh() -> void:
	for child in $GoodsList.get_children():
		child.free()
	var items: Array = _state.npc_sell.get("itemList", [])
	for index in range(items.size()):
		var item_id: int = items[index]
		var button := Button.new()
		button.position = Vector2(4, index * 34)
		button.size = Vector2(230, 32)
		button.text = "物品 %d" % item_id
		button.pressed.connect(func(): _selected_item_id = item_id)
		$GoodsList.add_child(button)


func _buy_selected() -> void:
	if _selected_item_id:
		NetworkClient.send_buy(_state.npc_sell.get("npcUID", 0), _selected_item_id, 0, 1)
