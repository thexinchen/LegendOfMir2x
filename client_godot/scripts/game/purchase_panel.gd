extends "res://scripts/game/closable_panel.gd"

signal quantity_requested(npc_uid: int, item_id: int, item_name: String)

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const BUY_NORMAL := preload("res://assets/ui/game/purchase/08000005.png")
const BUY_DOWN := preload("res://assets/ui/game/purchase/08000006.png")
const LEFT_NORMAL := preload("res://assets/ui/game/purchase/08000007.png")
const LEFT_DOWN := preload("res://assets/ui/game/purchase/08000008.png")
const RIGHT_NORMAL := preload("res://assets/ui/game/purchase/08000009.png")
const RIGHT_DOWN := preload("res://assets/ui/game/purchase/0800000a.png")
const COUNT_NORMAL := preload("res://assets/ui/game/purchase/0800000b.png")
const COUNT_DOWN := preload("res://assets/ui/game/purchase/0800000c.png")
const PAGE_SIZE := 12

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _selected_item_id := 0
var _detail_selected := -1
var _detail_page := 0
var _scroll := 0.0


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	$SelectButton.pressed.connect(_query_selected)
	$GoodsList.gui_input.connect(_on_list_input)
	_refresh()


func _refresh() -> void:
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
		row.position = Vector2(0, (index - start) * 38)
		row.size = Vector2(237, 38)
		$GoodsList.add_child(row)
		if item_id == _selected_item_id:
			var highlight := ColorRect.new()
			highlight.color = Color(1, 1, 1, 0.25)
			highlight.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			highlight.mouse_filter = Control.MOUSE_FILTER_IGNORE
			highlight.show_behind_parent = true
			row.add_child(highlight)
		var icon_button := TextureButton.new()
		icon_button.size = Vector2(237, 38)
		icon_button.ignore_texture_size = true
		icon_button.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		icon_button.pressed.connect(_select_item.bind(item_id))
		icon_button.gui_input.connect(_on_row_input.bind(item_id))
		row.add_child(icon_button)
		var icon: Dictionary = _resources.item_icon(item_id)
		if not icon.is_empty():
			var image := TextureRect.new()
			image.position = Vector2(0, 0)
			image.size = Vector2(38, 38)
			image.texture = icon.texture
			image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
			image.mouse_filter = Control.MOUSE_FILTER_IGNORE
			row.add_child(image)
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


func _select_item(item_id: int) -> void:
	_selected_item_id = item_id
	_refresh_goods()


func _on_row_input(event: InputEvent, item_id: int) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed and event.double_click:
		_selected_item_id = item_id
		_query_selected()


func _on_list_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	if event.button_index == MOUSE_BUTTON_WHEEL_UP:
		_scroll = maxf(0.0, _scroll - 0.1)
	elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_scroll = minf(1.0, _scroll + 0.1)
	else:
		return
	_refresh_goods()


func _query_selected() -> void:
	if _selected_item_id == 0:
		return
	_state.npc_sell_detail = {}
	_detail_page = 0
	_detail_selected = -1
	NetworkClient.send_query_sell_item_list(_state.npc_sell.get("npcUID", 0), _selected_item_id)


func _refresh_detail() -> void:
	for child in $Detail.get_children():
		child.free()
	var detail: Dictionary = _state.npc_sell_detail
	var list: Array = detail.get("list", [])
	if detail.get("npcUID", 0) != _state.npc_sell.get("npcUID", 0) or list.is_empty():
		_set_background(0x08000000, Vector2(290, 224))
		return
	if _resources.item_is_packable(_selected_item_id):
		_set_background(0x08000002, Vector2(514, 224))
		_build_packable_detail(list[0])
	else:
		_set_background(0x08000001, Vector2(488, 224))
		_build_unique_detail(list)


func _set_background(texture_id: int, panel_size: Vector2) -> void:
	var frame: Dictionary = _resources.frame("proguse", texture_id)
	if not frame.is_empty():
		$Background.texture = frame.texture
	$Background.size = panel_size
	size = panel_size
	custom_minimum_size = panel_size


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
			button.texture_normal = icon.texture
		button.tooltip_text = "%s\n价格 %d" % [_resources.item_name(item.get("itemID", 0)), _gold_price(sell_item)]
		button.pressed.connect(func(): _detail_selected = index; _refresh_detail())
		$Detail.add_child(button)
		var price := Label.new()
		price.position = button.position
		price.size = button.size
		price.text = str(_gold_price(sell_item))
		price.add_theme_font_size_override("font_size", 10)
		price.add_theme_color_override("font_color", Color.YELLOW)
		price.mouse_filter = Control.MOUSE_FILTER_IGNORE
		$Detail.add_child(price)
		if _detail_selected == index:
			button.modulate = Color(0.55, 0.55, 1.0, 1.0)
	_add_detail_button(Vector2(315, 163), LEFT_NORMAL, LEFT_DOWN, func(): _detail_page -= 1; _refresh_detail())
	_add_detail_button(Vector2(357, 163), BUY_NORMAL, BUY_DOWN, _buy_unique.bind(list))
	_add_detail_button(Vector2(405, 163), RIGHT_NORMAL, RIGHT_DOWN, func(): _detail_page += 1; _refresh_detail())
	_add_detail_close(Vector2(448, 159))
	var page_label := Label.new()
	page_label.position = Vector2(382, 16)
	page_label.text = "第 %d/%d 页" % [_detail_page + 1, page_count]
	page_label.add_theme_font_size_override("font_size", 12)
	page_label.add_theme_color_override("font_color", Color.YELLOW)
	$Detail.add_child(page_label)


func _build_packable_detail(sell_item: Dictionary) -> void:
	var item: Dictionary = sell_item.get("item", {})
	var item_id: int = item.get("itemID", 0)
	var icon: Dictionary = _resources.item_icon(item_id)
	if not icon.is_empty():
		var image := TextureRect.new()
		image.position = Vector2(303, 16)
		image.size = Vector2(38, 38)
		image.texture = icon.texture
		image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		$Detail.add_child(image)
	var price := Label.new()
	price.position = Vector2(353, 16)
	price.size = Vector2(145, 38)
	price.text = "%d 金币" % _gold_price(sell_item)
	price.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	price.add_theme_color_override("font_color", Color.YELLOW)
	$Detail.add_child(price)
	_add_detail_button(Vector2(366, 60), COUNT_NORMAL, COUNT_DOWN, func(): quantity_requested.emit(_state.npc_sell.get("npcUID", 0), item_id, _resources.item_name(item_id)))
	_add_detail_close(Vector2(474, 56))


func _buy_unique(list: Array) -> void:
	if _detail_selected < 0 or _detail_selected >= list.size():
		return
	var item: Dictionary = list[_detail_selected].get("item", {})
	NetworkClient.send_buy(_state.npc_sell.get("npcUID", 0), item.get("itemID", 0), item.get("seqID", 0), 1)


func _gold_price(sell_item: Dictionary) -> int:
	var costs: Array = sell_item.get("costList", [])
	return costs[0].get("count", 0) if not costs.is_empty() else 0


func _add_detail_button(position: Vector2, normal: Texture2D, down: Texture2D, callback: Callable) -> void:
	var button := TextureButton.new()
	button.position = position
	button.texture_normal = normal
	button.texture_pressed = down
	button.ignore_texture_size = true
	button.pressed.connect(callback)
	$Detail.add_child(button)


func _add_detail_close(position: Vector2) -> void:
	var button := TextureButton.new()
	button.position = position
	button.texture_normal = preload("res://assets/ui/game/inventory/0000001c.png")
	button.ignore_texture_size = true
	button.pressed.connect(func(): _state.npc_sell_detail = {}; _refresh_detail())
	$Detail.add_child(button)
