extends "res://scripts/game/closable_panel.gd"

signal quantity_requested(npc_uid: int, item_id: int, item_name: String)

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const ItemTooltipFormatterScript = preload("res://scripts/game/item_tooltip_formatter.gd")
const ItemTooltipRendererScript = preload("res://scripts/game/item_tooltip_renderer.gd")
const BUY_NORMAL := preload("res://assets/ui/game/purchase/08000005.png")
const BUY_DOWN := preload("res://assets/ui/game/purchase/08000006.png")
const LEFT_NORMAL := preload("res://assets/ui/game/purchase/08000007.png")
const LEFT_DOWN := preload("res://assets/ui/game/purchase/08000008.png")
const RIGHT_NORMAL := preload("res://assets/ui/game/purchase/08000009.png")
const RIGHT_DOWN := preload("res://assets/ui/game/purchase/0800000a.png")
const COUNT_NORMAL := preload("res://assets/ui/game/purchase/0800000b.png")
const COUNT_DOWN := preload("res://assets/ui/game/purchase/0800000c.png")
const CLOSE_NORMAL := preload("res://assets/ui/game/inventory/0000001c.png")
const CLOSE_DOWN := preload("res://assets/ui/game/inventory/0000001d.png")
const PAGE_SIZE := 12

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _selected_index := 0
var _detail_selected := -1
var _detail_page := 0
var _scroll := 0.0
var _reset_serial := -1
var _tooltip_index := -1


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	ItemTooltipRendererScript.configure($ItemTooltip, false, false)
	_state.state_changed.connect(_refresh)
	$SelectButton.pressed.connect(_query_selected)
	$CloseButton.pressed.connect(_close_panel)
	$GoodsList.gui_input.connect(_on_list_input)
	$SliderHit.gui_input.connect(_on_slider_input)
	_refresh()


func _process(_delta: float) -> void:
	if $ItemTooltip.visible:
		if is_visible_in_tree():
			ItemTooltipRendererScript.update_position($ItemTooltip, false)
		else:
			_hide_item_tooltip()


func _close_panel() -> void:
	_state.npc_sell_detail = {}
	_refresh_detail()


func _refresh() -> void:
	if _reset_serial != _state.npc_sell_reset_serial:
		_reset_serial = _state.npc_sell_reset_serial
		_selected_index = 0
		_detail_selected = -1
		_detail_page = 0
		_scroll = 0.0
	_refresh_goods()
	_refresh_detail()


func _refresh_goods() -> void:
	for child in $GoodsList.get_children():
		child.free()
	var items: Array = _state.npc_sell.get("itemList", [])
	var start := 0 if items.size() <= 4 else roundi((items.size() - 4) * _scroll)
	for index in range(start, mini(start + 4, items.size())):
		var item_id: int = items[index]
		var row := Control.new()
		row.position = Vector2(0, (index - start) * 42)
		row.size = Vector2(233, 38)
		$GoodsList.add_child(row)
		if index == _selected_index:
			var highlight := ColorRect.new()
			highlight.color = Color(1, 1, 1, 64.0 / 255.0)
			highlight.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			highlight.mouse_filter = Control.MOUSE_FILTER_IGNORE
			highlight.show_behind_parent = true
			row.add_child(highlight)
		var icon_button := TextureButton.new()
		icon_button.size = Vector2(233, 38)
		icon_button.ignore_texture_size = true
		icon_button.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		icon_button.pressed.connect(_select_item.bind(index))
		icon_button.gui_input.connect(_on_row_input.bind(index))
		row.add_child(icon_button)
		var icon: Dictionary = _resources.item_icon(item_id)
		_add_item_icon(row, icon, Rect2(0, 0, 38, 38))
		var label := Label.new()
		label.position = Vector2(48, 0)
		label.size = Vector2(185, 38)
		label.text = _resources.item_name(item_id)
		label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		label.add_theme_color_override("font_color", Color.YELLOW)
		label.add_theme_font_size_override("font_size", 12)
		label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		row.add_child(label)
	$Slider.position.y = 18.0 + _scroll * 125.0


func _select_item(index: int) -> void:
	_selected_index = index
	_refresh_goods()


func _on_row_input(event: InputEvent, index: int) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed and event.double_click:
		_selected_index = index
		_query_selected()


func _on_list_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		_scroll = maxf(0.0, _scroll - _scroll_step())
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_scroll = minf(1.0, _scroll + _scroll_step())
	else:
		return
	_refresh_goods()


func _scroll_step() -> float:
	var item_count: int = _state.npc_sell.get("itemList", []).size()
	return 1.0 / float(item_count - 4) if item_count > 4 else 0.0


func _on_slider_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		_scroll = clampf((event.position.y - 7.0) / 125.0, 0.0, 1.0)
		_refresh_goods()
		accept_event()
	elif event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		_scroll = clampf((event.position.y - 7.0) / 125.0, 0.0, 1.0)
		_refresh_goods()
		accept_event()


func _query_selected() -> void:
	var items: Array = _state.npc_sell.get("itemList", [])
	if _selected_index < 0 or _selected_index >= items.size():
		return
	var item_id: int = items[_selected_index]
	_state.npc_sell_detail = {}
	_detail_page = 0
	_detail_selected = -1
	NetworkClient.send_query_sell_item_list(_state.npc_sell.get("npcUID", 0), item_id)


func _refresh_detail() -> void:
	var tooltip_index := _tooltip_index
	for child in $Detail.get_children():
		child.free()
	var detail: Dictionary = _state.npc_sell_detail
	var list: Array = detail.get("list", [])
	if detail.get("npcUID", 0) != _state.npc_sell.get("npcUID", 0) or list.is_empty():
		_set_background(0x08000000, Vector2(290, 224))
		_hide_item_tooltip()
		return
	var detail_item: Dictionary = list[0].get("item", {})
	if _resources.item_is_packable(detail_item.get("itemID", 0)):
		_set_background(0x08000002, Vector2(514, 224))
		_build_packable_detail(list[0])
		_hide_item_tooltip()
	else:
		_set_background(0x08000001, Vector2(488, 224))
		_build_unique_detail(list)
		if tooltip_index >= 0 and tooltip_index < list.size() and tooltip_index / PAGE_SIZE == _detail_page:
			_show_item_tooltip(tooltip_index, list[tooltip_index])
		else:
			_hide_item_tooltip()


func _set_background(texture_id: int, panel_size: Vector2) -> void:
	var frame: Dictionary = _resources.frame("proguse", texture_id)
	if not frame.is_empty():
		$Background.texture = frame.texture
	$Background.size = panel_size
	custom_minimum_size = panel_size
	size = panel_size


func _build_unique_detail(list: Array) -> void:
	var page_count := maxi(1, ceili(float(list.size()) / PAGE_SIZE))
	_detail_page = clampi(_detail_page, 0, page_count - 1)
	for grid in PAGE_SIZE:
		var index := _detail_page * PAGE_SIZE + grid
		if index >= list.size():
			break
		var sell_item: Dictionary = list[index]
		var item: Dictionary = sell_item.get("item", {})
		var button := TextureButton.new()
		button.position = Vector2(313 + (grid % 4) * 38, 41 + (grid / 4) * 38)
		button.size = Vector2(38, 38)
		button.ignore_texture_size = true
		button.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		var icon: Dictionary = _resources.item_icon(item.get("itemID", 0))
		if not icon.is_empty():
			_add_item_icon(button, icon, Rect2(Vector2.ZERO, button.size))
		button.mouse_entered.connect(_show_item_tooltip.bind(index, sell_item))
		button.mouse_exited.connect(_hide_item_tooltip)
		button.pressed.connect(func(): _detail_selected = index; _refresh_detail())
		$Detail.add_child(button)
		var price := Label.new()
		price.position = button.position
		price.size = button.size
		price.text = _comma_number(_gold_price(sell_item))
		price.add_theme_font_size_override("font_size", 10)
		price.add_theme_color_override("font_color", Color.YELLOW)
		price.mouse_filter = Control.MOUSE_FILTER_IGNORE
		$Detail.add_child(price)
		var overlay := ColorRect.new()
		overlay.position = button.position
		overlay.size = button.size
		overlay.color = Color(0, 0, 1, 96.0 / 255.0) if _detail_selected == index else Color(1, 1, 1, 96.0 / 255.0)
		overlay.visible = _detail_selected == index
		overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
		$Detail.add_child(overlay)
		button.mouse_entered.connect(func():
			overlay.color = Color(0, 0, 1, 96.0 / 255.0) if _detail_selected == index else Color(1, 1, 1, 96.0 / 255.0)
			overlay.show()
		)
		button.mouse_exited.connect(func(): overlay.visible = _detail_selected == index)
		button.gui_input.connect(_on_detail_grid_input)
	_add_detail_button(Vector2(315, 163), LEFT_NORMAL, LEFT_DOWN, func(): _detail_page -= 1; _refresh_detail())
	_add_detail_button(Vector2(357, 163), BUY_NORMAL, BUY_DOWN, _buy_unique.bind(list))
	_add_detail_button(Vector2(405, 163), RIGHT_NORMAL, RIGHT_DOWN, func(): _detail_page += 1; _refresh_detail())
	_add_detail_close(Vector2(448, 159))
	var page_label := Label.new()
	page_label.position = Vector2(389, 18)
	page_label.text = "第%d/%d页" % [_detail_page + 1, page_count]
	page_label.add_theme_font_size_override("font_size", 12)
	page_label.add_theme_color_override("font_color", Color.YELLOW)
	$Detail.add_child(page_label)


func _show_item_tooltip(index: int, sell_item: Dictionary) -> void:
	_tooltip_index = index
	var item: Dictionary = sell_item.get("item", {})
	var lines: Array[String] = ItemTooltipFormatterScript.plain_layout_lines(item, _resources, _gold_price(sell_item))
	ItemTooltipRendererScript.show_lines($ItemTooltip, lines, 220.0, 40.0, Vector2(10, 10), 15.0, 12)
	ItemTooltipRendererScript.update_position($ItemTooltip, false)


func _hide_item_tooltip() -> void:
	_tooltip_index = -1
	$ItemTooltip.hide()


func _build_packable_detail(sell_item: Dictionary) -> void:
	var item: Dictionary = sell_item.get("item", {})
	var item_id: int = item.get("itemID", 0)
	var icon: Dictionary = _resources.item_icon(item_id)
	_add_item_icon($Detail, icon, Rect2(303, 16, 38, 38))
	var price := Label.new()
	price.position = Vector2(353, 16)
	price.size = Vector2(145, 38)
	price.text = "%s 金币" % _comma_number(_gold_price(sell_item))
	price.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	price.add_theme_font_size_override("font_size", 13)
	price.add_theme_color_override("font_color", Color.YELLOW)
	$Detail.add_child(price)
	_add_detail_button(Vector2(366, 60), COUNT_NORMAL, COUNT_DOWN, func(): quantity_requested.emit(_state.npc_sell.get("npcUID", 0), item_id, _resources.item_name(item_id)))
	_add_detail_close(Vector2(474, 56))


func _add_item_icon(parent: Node, icon: Dictionary, box: Rect2) -> TextureRect:
	if icon.is_empty():
		return null
	var texture: Texture2D = icon.texture
	var texture_size := texture.get_size()
	var scale := minf(1.0, minf(box.size.x / texture_size.x, box.size.y / texture_size.y))
	var image := TextureRect.new()
	image.name = "Icon"
	image.size = texture_size * scale
	image.position = box.position + (box.size - image.size) / 2.0
	image.texture = texture
	image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	image.stretch_mode = TextureRect.STRETCH_SCALE
	image.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(image)
	return image


func _buy_unique(list: Array) -> void:
	if _detail_selected < 0 or _detail_selected >= list.size():
		return
	var item: Dictionary = list[_detail_selected].get("item", {})
	NetworkClient.send_buy(_state.npc_sell.get("npcUID", 0), item.get("itemID", 0), item.get("seqID", 0), 1)


func _gold_price(sell_item: Dictionary) -> int:
	var costs: Array = sell_item.get("costList", [])
	for cost_value in costs:
		var cost: Dictionary = cost_value
		if _resources.item_type(cost.get("itemID", 0)) == "金币":
			return cost.get("count", 0)
	return 0


func _comma_number(value: int) -> String:
	var digits := str(value)
	var result := ""
	while digits.length() > 3:
		result = "," + digits.right(3) + result
		digits = digits.left(digits.length() - 3)
	return digits + result


func _on_detail_grid_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		_detail_page -= 1
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_detail_page += 1
	else:
		return
	_refresh_detail()
	accept_event()


func _add_detail_button(position: Vector2, normal: Texture2D, down: Texture2D, callback: Callable) -> void:
	var button := TextureButton.new()
	button.position = position
	button.texture_normal = normal
	button.texture_pressed = down
	button.ignore_texture_size = true
	button.pressed.connect(callback)
	AudioService.bind_ui_click(button)
	$Detail.add_child(button)


func _add_detail_close(position: Vector2) -> void:
	var button := TextureButton.new()
	button.position = position
	button.texture_normal = CLOSE_NORMAL
	button.texture_pressed = CLOSE_DOWN
	button.ignore_texture_size = true
	button.pressed.connect(func(): _state.npc_sell_detail = {}; _refresh_detail())
	AudioService.bind_ui_click(button)
	$Detail.add_child(button)
