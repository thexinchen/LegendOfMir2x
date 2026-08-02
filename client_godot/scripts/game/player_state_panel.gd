extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")
const ItemTooltipFormatterScript = preload("res://scripts/game/item_tooltip_formatter.gd")
const ItemTooltipRendererScript = preload("res://scripts/game/item_tooltip_renderer.gd")

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _tooltip_location := -1

const WEAR_GRIDS := {
	1: Rect2(90, 100, 60, 110),
	2: Rect2(100, 65, 30, 25),
	3: Rect2(40, 80, 45, 90),
	4: Rect2(10, 240, 38, 56),
	5: Rect2(168, 88, 38, 38),
	6: Rect2(10, 155, 38, 38),
	7: Rect2(168, 155, 38, 38),
	8: Rect2(10, 195, 38, 38),
	9: Rect2(168, 195, 38, 38),
	10: Rect2(88, 265, 38, 38),
	11: Rect2(128, 265, 38, 38),
}


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	ItemTooltipRendererScript.configure($ItemTooltip, true, true)
	_state.state_changed.connect(_refresh)
	_refresh()


func _process(_delta: float) -> void:
	if $ItemTooltip.visible:
		if is_visible_in_tree():
			ItemTooltipRendererScript.update_position($ItemTooltip, false)
		else:
			_hide_item_tooltip()


func _refresh() -> void:
	var tooltip_location := _tooltip_location
	$Name.text = _state.player_name
	var name_color: int = _state.player_name_color
	$Name.add_theme_color_override("font_color", Color8(
		name_color & 0xFF,
		(name_color >> 8) & 0xFF,
		(name_color >> 16) & 0xFF,
		(name_color >> 24) & 0xFF,
	))
	var combat: Dictionary = CombatCalculatorScript.calculate(_state, _resources)
	var inv_load := CombatCalculatorScript.inventory_load(_state, _resources)
	var body_load := CombatCalculatorScript.body_load(_state, _resources)
	var weapon_load := CombatCalculatorScript.weapon_load(_state, _resources)
	var state_values := ["%d" % _state.player_level, "%.2f%%" % (_state.level_ratio() * 100.0),
		"%d/%d" % [_state.player_hp, _state.player_hp_max], "%d/%d" % [_state.player_mp, _state.player_mp_max],
		"%d/%d" % [inv_load, combat.load[2]], "%d/%d" % [body_load, combat.load[0]],
		"%d/%d" % [weapon_load, combat.load[1]], "%d" % combat.dc_hit, "%d" % combat.dc_dodge]
	var overloaded := [false, false, false, false, inv_load > combat.load[2], body_load > combat.load[0], weapon_load > combat.load[1], false, false]
	_refresh_state_values(state_values, overloaded)
	$CombatStats.text = "攻击 %d - %d          防御 %d - %d\n魔法 %d - %d          魔防 %d - %d       道术 %d - %d" % [
		combat.dc[0], combat.dc[1], combat.ac[0], combat.ac[1],
		combat.mc[0], combat.mc[1], combat.mac[0], combat.mac[1], combat.sc[0], combat.sc[1],
	]
	_refresh_elements(combat)
	for child in $EquipmentSlots.get_children():
		child.free()
	for location in WEAR_GRIDS:
		var item: Dictionary = _state.wear.get(location, {})
		var grid: Rect2 = WEAR_GRIDS[location]
		var icon := TextureButton.new()
		icon.position = grid.position - $EquipmentSlots.position
		icon.size = grid.size
		icon.ignore_texture_size = true
		icon.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		icon.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		if not item.is_empty() and location >= 4:
			var frame: Dictionary = _resources.frame("item", _resources.item_package_gfx_id(item.get("itemID", 0)) | 0x01000000)
			if not frame.is_empty():
				var image := TextureRect.new()
				var image_size: Vector2 = frame.texture.get_size()
				image.name = "Icon"
				image.position.x = (icon.size.x - image_size.x) / 2.0
				image.position.y = icon.size.y - image_size.y if location == 4 else (icon.size.y - image_size.y) / 2.0
				image.size = image_size
				image.mouse_filter = Control.MOUSE_FILTER_IGNORE
				image.texture = frame.texture
				image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
				image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
				icon.add_child(image)
		if not item.is_empty():
			icon.mouse_entered.connect(_show_item_tooltip.bind(location, item))
			icon.mouse_exited.connect(_hide_item_tooltip)
		if location >= 4:
			_add_wear_hover_overlay(icon, location)
		icon.pressed.connect(_on_wear_pressed.bind(location))
		$EquipmentSlots.add_child(icon)
	if tooltip_location >= 0 and not _state.wear.get(tooltip_location, {}).is_empty():
		_show_item_tooltip(tooltip_location, _state.wear[tooltip_location])
	else:
		_hide_item_tooltip()
	$CharacterLayers.queue_redraw()


func _add_wear_hover_overlay(icon: TextureButton, location: int) -> void:
	var frame: Dictionary = _resources.frame("proguse", 0x06000002 if location == 4 else 0x06000001)
	if frame.is_empty():
		return
	var overlay := TextureRect.new()
	overlay.name = "HoverOverlay"
	overlay.position = Vector2(-1, -6 if location == 4 else -3)
	overlay.size = frame.texture.get_size()
	overlay.texture = frame.texture
	overlay.modulate = Color(1, 1, 1, 128.0 / 255.0)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.hide()
	icon.add_child(overlay)
	icon.mouse_entered.connect(overlay.show)
	icon.mouse_exited.connect(overlay.hide)


func _refresh_state_values(values: Array, overloaded: Array) -> void:
	$StateValues.text = ""
	for child in $StateValues.get_children():
		child.free()
	for index in range(values.size()):
		var label := Label.new()
		label.position = Vector2(0, index * 24)
		label.size = Vector2($StateValues.size.x, 20)
		label.text = values[index]
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		label.add_theme_font_size_override("font_size", 12)
		label.add_theme_color_override("font_color", Color.RED if overloaded[index] else Color.WHITE)
		$StateValues.add_child(label)


func _on_wear_pressed(location: int) -> void:
	if not _state.grabbed_item.is_empty():
		var grabbed: Dictionary = _state.grabbed_item
		if _can_wear(int(grabbed.get("itemID", 0)), location):
			NetworkClient.send_request_equip_wear(grabbed.get("itemID", 0), grabbed.get("seqID", 0), location)
		else:
			_state.inventory.append(grabbed)
			AudioService.play_seff_at(_resources.item_sound_effect(int(grabbed.get("itemID", 0))), 0, 0, 0, 0)
			_state.grabbed_item = {}
			_state.state_changed.emit()
	elif not _state.wear.get(location, {}).is_empty():
		NetworkClient.send_request_grab_wear(location)


func _can_wear(item_id: int, location: int) -> bool:
	var item_type: String = _resources.item_type(item_id)
	var expected := {
		1: ["衣服"], 2: ["头盔"], 3: ["武器"], 4: ["鞋"], 5: ["项链"],
		6: ["手镯"], 7: ["手镯"], 8: ["戒指"], 9: ["戒指"], 10: ["火把"],
		11: ["魅力", "护身符", "药粉"],
	}
	if item_type not in expected.get(location, []):
		return false
	if location == 1:
		var name: String = _resources.item_name(item_id)
		if "（男）" in name:
			return bool(_state.player_gender)
		if "（女）" in name:
			return not bool(_state.player_gender)
		return false
	return true


func _show_item_tooltip(location: int, item: Dictionary) -> void:
	_tooltip_location = location
	var lines: Array[String] = ItemTooltipFormatterScript.plain_layout_lines(item, _resources)
	ItemTooltipRendererScript.show_lines($ItemTooltip, lines, 220.0, 40.0, Vector2(10, 10), 15.0, 12)
	ItemTooltipRendererScript.update_position($ItemTooltip, false)


func _hide_item_tooltip() -> void:
	_tooltip_location = -1
	$ItemTooltip.hide()


func _refresh_elements(combat: Dictionary) -> void:
	for child in $Elements.get_children():
		child.free()
	for row in [["攻击元素", 0, 0x06000010], ["防御元素", 30, 0x06000010], ["弱点元素", 60, 0x06000020]]:
		var title := Label.new()
		title.position = Vector2(-8, row[1] - 4)
		title.text = row[0]
		title.add_theme_font_size_override("font_size", 15)
		title.add_theme_color_override("font_color", Color(0.86, 0.8, 0.61))
		$Elements.add_child(title)
		for index in range(7):
			var value: int = combat.dc_elem[index] if row[0] == "攻击元素" else combat.ac_elem[index]
			if (row[0] == "弱点元素" and value >= 0) or (row[0] != "弱点元素" and value <= 0):
				continue
			var frame: Dictionary = _resources.frame("proguse", row[2] + index)
			if not frame.is_empty():
				var image := TextureRect.new()
				image.position = Vector2(44 + index * 37, row[1] - 2)
				image.texture = frame.texture
				image.mouse_filter = Control.MOUSE_FILTER_IGNORE
				$Elements.add_child(image)
			var value_label := Label.new()
			value_label.position = Vector2(64 + index * 37, row[1] - 3)
			value_label.text = "%+d" % value
			value_label.add_theme_font_size_override("font_size", 12)
			value_label.add_theme_color_override("font_color", Color.RED if value < 0 else Color.GREEN)
			$Elements.add_child(value_label)
