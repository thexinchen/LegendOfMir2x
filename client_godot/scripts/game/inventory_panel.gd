extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const GRID_COLUMNS := 10
const GRID_VISIBLE_ROWS := 10
const CELL_SIZE := 38

const OP_NONE := 0
const OP_TRADE := 1
const OP_SECURE := 2
const OP_REPAIR := 3

const REPAIR_NORMAL := preload("res://assets/ui/game/inventory/000000b1.png")
const REPAIR_DOWN := preload("res://assets/ui/game/inventory/000000b2.png")
const TRADE_NORMAL := preload("res://assets/ui/game/inventory/000000b3.png")
const TRADE_DOWN := preload("res://assets/ui/game/inventory/000000b4.png")
const SECURE_NORMAL := preload("res://assets/ui/game/inventory/000000b5.png")
const SECURE_DOWN := preload("res://assets/ui/game/inventory/000000b6.png")

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _bins: Dictionary = {} # item key -> {x,y,w,h,item}
var _selected_key := ""
var _grabbed_origin := Vector2i.ZERO
var _scroll_row := 0


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	$SortButton.pressed.connect(_repack)
	$CloseButton.pressed.connect(_close_operation)
	visibility_changed.connect(func():
		if not visible and _state and not _state.inventory_operation.is_empty():
			_close_operation()
	)
	$ItemGrid.gui_input.connect(_on_grid_input)
	$OperationButton.pressed.connect(_commit_operation)
	_refresh()


func _refresh() -> void:
	_sync_bins()
	_refresh_operation()
	$Gold.text = "%d" % _state.player_gold
	for child in $ItemGrid.get_children():
		child.free()
	for key in _bins:
		var bin: Dictionary = _bins[key]
		var display_y := (int(bin.y) - _scroll_row) * CELL_SIZE
		if display_y + int(bin.h) * CELL_SIZE <= 0 or display_y >= GRID_VISIBLE_ROWS * CELL_SIZE:
			continue
		var item: Dictionary = bin.item
		var item_id := int(item.get("itemID", 0))
		var button := TextureButton.new()
		button.position = Vector2(int(bin.x) * CELL_SIZE, display_y)
		button.size = Vector2(int(bin.w) * CELL_SIZE, int(bin.h) * CELL_SIZE)
		button.ignore_texture_size = true
		button.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		var icon: Dictionary = _resources.item_icon(item_id)
		if not icon.is_empty():
			button.texture_normal = icon.texture
		button.tooltip_text = "%s\n类型 %s\n数量 %d\n序号 %d" % [
			_resources.item_name(item_id),
			_resources.item_type(item_id),
			item.get("count", 0),
			item.get("seqID", 0),
		]
		button.gui_input.connect(func(event: InputEvent): _on_item_input(event, key))
		$ItemGrid.add_child(button)
		if key == _selected_key:
			var selected := ColorRect.new()
			selected.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			selected.mouse_filter = Control.MOUSE_FILTER_IGNORE
			selected.color = Color(0.1, 0.2, 1.0, 0.22)
			button.add_child(selected)
		if int(item.get("count", 1)) > 1:
			var count_label := Label.new()
			count_label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			count_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
			count_label.text = str(item.get("count", 1))
			count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
			count_label.vertical_alignment = VERTICAL_ALIGNMENT_TOP
			count_label.add_theme_font_size_override("font_size", 10)
			count_label.add_theme_color_override("font_color", Color(1, 0.9, 0.25))
			button.add_child(count_label)


func _sync_bins() -> void:
	var live: Dictionary = {}
	for item in _state.inventory:
		var key := _item_key(item)
		live[key] = true
		if _bins.has(key):
			_bins[key]["item"] = item
			continue
		var size := _item_grid_size(int(item.get("itemID", 0)))
		var position := _find_free_position(size.x, size.y)
		_bins[key] = {"x": position.x, "y": position.y, "w": size.x, "h": size.y, "item": item}
	for key in _bins.keys():
		if not live.has(key):
			_bins.erase(key)
	if not _bins.has(_selected_key):
		_selected_key = ""


func _item_grid_size(item_id: int) -> Vector2i:
	var icon: Dictionary = _resources.item_icon(item_id)
	if icon.is_empty():
		return Vector2i.ONE
	var texture: Texture2D = icon.texture
	return Vector2i(maxi(1, ceili(float(texture.get_width()) / CELL_SIZE)), maxi(1, ceili(float(texture.get_height()) / CELL_SIZE)))


func _find_free_position(width: int, height: int) -> Vector2i:
	for y in range(1000):
		for x in range(GRID_COLUMNS - width + 1):
			if not _occupied(x, y, width, height):
				return Vector2i(x, y)
	return Vector2i(0, 1000)


func _occupied(x: int, y: int, width: int, height: int, except_key: String = "") -> bool:
	for key in _bins:
		if key == except_key:
			continue
		var bin: Dictionary = _bins[key]
		if x < int(bin.x) + int(bin.w) and x + width > int(bin.x) and y < int(bin.y) + int(bin.h) and y + height > int(bin.y):
			return true
	return false


func _on_item_input(event: InputEvent, key: String) -> void:
	if not event is InputEventMouseButton or not event.pressed:
		return
	get_viewport().set_input_as_handled()
	if event.button_index == MOUSE_BUTTON_RIGHT:
		_consume_or_equip(key)
	elif event.button_index == MOUSE_BUTTON_LEFT:
		if int(_state.inventory_operation.get("invOp", OP_NONE)) != OP_NONE:
			_select_operation_item(key)
		else:
			_grab_item(key)


func _on_grid_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_scroll_row = maxi(0, _scroll_row - 1)
			_refresh()
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_scroll_row = mini(_max_scroll_row(), _scroll_row + 1)
			_refresh()
		elif event.button_index == MOUSE_BUTTON_LEFT and not _state.grabbed_item.is_empty():
			_place_grabbed(Vector2i(floori(event.position.x / CELL_SIZE), floori(event.position.y / CELL_SIZE) + _scroll_row))


func _grab_item(key: String) -> void:
	if not _bins.has(key):
		return
	var bin: Dictionary = _bins[key]
	var old_grabbed: Dictionary = _state.grabbed_item
	var selected: Dictionary = bin.item
	_grabbed_origin = Vector2i(bin.x, bin.y)
	_state.inventory.erase(selected)
	_bins.erase(key)
	if not old_grabbed.is_empty():
		_state.inventory.append(old_grabbed)
		var old_size := _item_grid_size(int(old_grabbed.get("itemID", 0)))
		_bins[_item_key(old_grabbed)] = {"x": _grabbed_origin.x, "y": _grabbed_origin.y, "w": old_size.x, "h": old_size.y, "item": old_grabbed}
	_state.grabbed_item = selected
	_state.state_changed.emit()


func _place_grabbed(grid: Vector2i) -> void:
	var item: Dictionary = _state.grabbed_item
	var size := _item_grid_size(int(item.get("itemID", 0)))
	var x := grid.x - size.x / 2
	var y := grid.y - size.y / 2
	if x < 0 or x + size.x > GRID_COLUMNS or y < 0 or _occupied(x, y, size.x, size.y):
		var fallback := _find_free_position(size.x, size.y)
		x = fallback.x
		y = fallback.y
	_state.inventory.append(item)
	_bins[_item_key(item)] = {"x": x, "y": y, "w": size.x, "h": size.y, "item": item}
	_state.grabbed_item = {}
	_state.state_changed.emit()


func _consume_or_equip(key: String) -> void:
	if not _bins.has(key):
		return
	var item: Dictionary = _bins[key].item
	var item_type: String = _resources.item_type(int(item.get("itemID", 0)))
	if item_type in ["恢复药水", "强化药水", "技能书"]:
		NetworkClient.send_consume_item(item.get("itemID", 0), item.get("seqID", 0), 1)
		return
	var wear_types := {"衣服": 1, "头盔": 2, "武器": 3, "鞋": 4, "项链": 5, "手镯": 6, "戒指": 8}
	if wear_types.has(item_type):
		NetworkClient.send_request_equip_wear(item.get("itemID", 0), item.get("seqID", 0), wear_types[item_type])


func _select_operation_item(key: String) -> void:
	var bin: Dictionary = _bins.get(key, {})
	if bin.is_empty():
		return
	var item: Dictionary = bin.item
	var operation: Dictionary = _state.inventory_operation
	var allowed: Array = operation.get("typeList", [])
	if not allowed.has(_resources.item_type(int(item.get("itemID", 0)))):
		_state.add_chat_log("该物品类型不能用于当前操作", 3)
		_selected_key = ""
		return
	_selected_key = key
	_state.inventory_operation_cost = {}
	NetworkClient.send_npc_event(operation.get("uid", 0), "", operation.get("queryTag", ""), "%d:%d" % [item.get("itemID", 0), item.get("seqID", 0)])
	_refresh()


func _commit_operation() -> void:
	if _selected_key.is_empty() or not _bins.has(_selected_key):
		return
	var item: Dictionary = _bins[_selected_key].item
	var operation: Dictionary = _state.inventory_operation
	NetworkClient.send_npc_event(operation.get("uid", 0), "", operation.get("commitTag", ""), "%d:%d" % [item.get("itemID", 0), item.get("seqID", 0)])


func _refresh_operation() -> void:
	var mode := int(_state.inventory_operation.get("invOp", OP_NONE))
	$Title.text = {OP_NONE: "【背包】", OP_TRADE: "【请选择出售物品】", OP_SECURE: "【请选择存储物品】", OP_REPAIR: "【请选择修理物品】"}.get(mode, "【背包】")
	$SortButton.visible = mode == OP_NONE
	var show_operation := mode != OP_NONE and not _selected_key.is_empty()
	$OperationBackground.visible = show_operation
	$OperationButton.visible = show_operation
	if mode == OP_TRADE:
		$OperationButton.texture_normal = TRADE_NORMAL
		$OperationButton.texture_pressed = TRADE_DOWN
	elif mode == OP_SECURE:
		$OperationButton.texture_normal = SECURE_NORMAL
		$OperationButton.texture_pressed = SECURE_DOWN
	else:
		$OperationButton.texture_normal = REPAIR_NORMAL
		$OperationButton.texture_pressed = REPAIR_DOWN
	var cost: Dictionary = _state.inventory_operation_cost
	var cost_matches := false
	if show_operation and not cost.is_empty() and _bins.has(_selected_key):
		var selected_item: Dictionary = _bins[_selected_key].item
		cost_matches = int(cost.get("itemID", 0)) == int(selected_item.get("itemID", -1)) and int(cost.get("seqID", 0)) == int(selected_item.get("seqID", -1))
	$OperationCost.visible = cost_matches
	$OperationCost.text = str(cost.get("cost", 0))


func _close_operation() -> void:
	_selected_key = ""
	_state.clear_inventory_operation()


func _repack() -> void:
	_bins.clear()
	_sync_bins()
	_scroll_row = 0
	_refresh()


func _max_scroll_row() -> int:
	var rows := 0
	for bin in _bins.values():
		rows = maxi(rows, int(bin.y) + int(bin.h))
	return maxi(0, rows - GRID_VISIBLE_ROWS)


func _item_key(item: Dictionary) -> String:
	return "%d:%d" % [item.get("itemID", 0), item.get("seqID", 0)]
