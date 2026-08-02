extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const ItemTooltipRendererScript = preload("res://scripts/game/item_tooltip_renderer.gd")
const PAGE_SIZE := 12

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _page := 0
var _selected_index := -1
var _last_reset_serial := -1
var _tooltip_index := -1


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	ItemTooltipRendererScript.configure($ItemTooltip, false, false)
	_state.state_changed.connect(_refresh)
	$LeftButton.pressed.connect(func(): _change_page(-1))
	$RightButton.pressed.connect(func(): _change_page(1))
	$SelectButton.pressed.connect(_retrieve)
	_refresh()


func _process(_delta: float) -> void:
	if $ItemTooltip.visible:
		if is_visible_in_tree():
			ItemTooltipRendererScript.update_position($ItemTooltip, false)
		else:
			_hide_item_tooltip()


func _refresh() -> void:
	var tooltip_index := _tooltip_index
	if _last_reset_serial != _state.secured_items_reset_serial:
		_last_reset_serial = _state.secured_items_reset_serial
		_page = 0
		_selected_index = -1
	var page_count := _page_count()
	_page = clampi(_page, 0, maxi(0, page_count - 1))
	if _selected_index >= _state.secured_items.size():
		_selected_index = -1
	$Page.text = "第%d/%d页" % [_page + 1, page_count] if page_count > 0 else "（空）"
	for child in $ItemGrid.get_children():
		child.free()
	for slot in range(PAGE_SIZE):
		var index := _page * PAGE_SIZE + slot
		var cell := TextureButton.new()
		cell.custom_minimum_size = Vector2(38, 38)
		cell.ignore_texture_size = true
		if index < _state.secured_items.size():
			var item: Dictionary = _state.secured_items[index]
			var item_id := int(item.get("itemID", 0))
			var icon: Dictionary = _resources.secured_item_icon(item_id)
			if not icon.is_empty():
				var image := TextureRect.new()
				var image_size: Vector2 = icon.texture.get_size()
				image.name = "Icon"
				image.position = (Vector2(38, 38) - image_size) / 2.0
				image.size = image_size
				image.mouse_filter = Control.MOUSE_FILTER_IGNORE
				image.texture = icon.texture
				image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
				image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
				cell.add_child(image)
			cell.mouse_entered.connect(_show_item_tooltip.bind(index, item))
			cell.mouse_exited.connect(_hide_item_tooltip)
			cell.pressed.connect(_select_index.bind(index))
			cell.gui_input.connect(_on_cell_input)
			if _resources.item_is_packable(item_id) and int(item.get("count", 0)) > 0:
				var count := Label.new()
				count.name = "Count"
				count.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
				count.mouse_filter = Control.MOUSE_FILTER_IGNORE
				count.text = _count_text(item.get("count", 0))
				count.add_theme_font_size_override("font_size", 10)
				count.add_theme_color_override("font_color", Color(1, 1, 0))
				cell.add_child(count)
			if index == _selected_index:
				var selected := ColorRect.new()
				selected.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
				selected.mouse_filter = Control.MOUSE_FILTER_IGNORE
				selected.color = Color(0, 0, 1, 0.38)
				cell.add_child(selected)
			else:
				var hovered := ColorRect.new()
				hovered.name = "Hover"
				hovered.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
				hovered.mouse_filter = Control.MOUSE_FILTER_IGNORE
				hovered.color = Color(1, 1, 1, 0.38)
				hovered.hide()
				cell.add_child(hovered)
				cell.mouse_entered.connect(hovered.show)
				cell.mouse_exited.connect(hovered.hide)
		$ItemGrid.add_child(cell)
	if tooltip_index >= _page * PAGE_SIZE and tooltip_index < mini((_page + 1) * PAGE_SIZE, _state.secured_items.size()):
		_show_item_tooltip(tooltip_index, _state.secured_items[tooltip_index])
	else:
		_hide_item_tooltip()


func _show_item_tooltip(index: int, item: Dictionary) -> void:
	_tooltip_index = index
	var item_id := int(item.get("itemID", 0))
	var description := str(_resources.item_detail(item_id).get("description", ""))
	var lines: Array[String] = [_resources.item_name(item_id), description if not description.is_empty() else "暂无描述"]
	ItemTooltipRendererScript.show_lines($ItemTooltip, lines, 240.0, 60.0, Vector2(20, 12), 19.0, 12, 60.0)
	ItemTooltipRendererScript.update_position($ItemTooltip, false)


func _hide_item_tooltip() -> void:
	_tooltip_index = -1
	$ItemTooltip.hide()


func _select_index(index: int) -> void:
	_selected_index = index
	_refresh()


func _on_cell_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		_change_page(-1)
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_change_page(1)


func _change_page(delta: int) -> void:
	var page_count := _page_count()
	_page = clampi(_page + delta, 0, maxi(0, page_count - 1))
	_refresh()


func _retrieve() -> void:
	if _selected_index < 0 or _selected_index >= _state.secured_items.size():
		return
	var item: Dictionary = _state.secured_items[_selected_index]
	NetworkClient.send_retrieve_secured_item(item.get("itemID", 0), item.get("seqID", 0))


func _page_count() -> int:
	return ceili(float(_state.secured_items.size()) / PAGE_SIZE)


func _count_text(value: int) -> String:
	var digits := str(value)
	var result := ""
	for index in digits.length():
		if index > 0 and (digits.length() - index) % 3 == 0:
			result += ","
		result += digits.substr(index, 1)
	return result
