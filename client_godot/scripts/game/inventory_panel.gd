extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
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
		var button := TextureButton.new()
		button.position = Vector2((index % columns) * 38, floori(float(index) / columns) * 38)
		button.size = Vector2(36, 36)
		button.ignore_texture_size = true
		button.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		var package_gfx_id: int = _resources.item_package_gfx_id(item.get("itemID", 0))
		var icon: Dictionary = _resources.frame("item", package_gfx_id | 0x01000000)
		if not icon.is_empty():
			button.texture_normal = icon.texture
		button.tooltip_text = "物品 %d\n数量 %d\n序号 %d" % [item.get("itemID", 0), item.get("count", 0), item.get("seqID", 0)]
		$ItemGrid.add_child(button)
		if item.get("count", 1) > 1:
			var count_label := Label.new()
			count_label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			count_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
			count_label.text = str(item.get("count", 1))
			count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
			count_label.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
			count_label.add_theme_font_size_override("font_size", 12)
			count_label.add_theme_color_override("font_color", Color.WHITE)
			button.add_child(count_label)


func _sort_items() -> void:
	_state.inventory.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		if left.get("itemID", 0) == right.get("itemID", 0):
			return left.get("seqID", 0) < right.get("seqID", 0)
		return left.get("itemID", 0) < right.get("itemID", 0)
	)
	_state.state_changed.emit()
