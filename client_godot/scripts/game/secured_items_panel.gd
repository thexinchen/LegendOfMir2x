extends "res://scripts/game/closable_panel.gd"

const PAGE_SIZE := 12
var _state: Node
var _page := 0
var _selected: Dictionary = {}


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$LeftButton.pressed.connect(func(): _page = maxi(0, _page - 1); _refresh())
	$RightButton.pressed.connect(func(): _page += 1; _refresh())
	$SelectButton.pressed.connect(_retrieve)
	_refresh()


func _refresh() -> void:
	var page_count: int = maxi(1, ceili(float(_state.secured_items.size()) / PAGE_SIZE))
	_page = clampi(_page, 0, page_count - 1)
	$Page.text = "%d / %d" % [_page + 1, page_count] if not _state.secured_items.is_empty() else "（空）"
	for child in $ItemGrid.get_children():
		child.free()
	for index in range(_page * PAGE_SIZE, mini((_page + 1) * PAGE_SIZE, _state.secured_items.size())):
		var item: Dictionary = _state.secured_items[index]
		var button := Button.new()
		button.text = str(item.get("itemID", 0))
		button.tooltip_text = "物品 %d，数量 %d" % [item.get("itemID", 0), item.get("count", 0)]
		button.pressed.connect(func(): _selected = item)
		$ItemGrid.add_child(button)


func _retrieve() -> void:
	if not _selected.is_empty():
		NetworkClient.send_retrieve_secured_item(_selected.get("itemID", 0), _selected.get("seqID", 0))
