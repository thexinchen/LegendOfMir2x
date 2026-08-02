extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default() or resources.item_attributes.is_empty():
		_fail("item combat metadata unavailable")
		return
	var weapon_id := _find_item(resources, "武器")
	var dress_id := _find_item(resources, "衣服")
	var shoe_id := _find_wear_icon(resources, "鞋")
	var necklace_id := _find_wear_icon(resources, "项链")
	if weapon_id == 0 or dress_id == 0 or shoe_id == 0 or necklace_id == 0:
		_fail("wearable item metadata unavailable")
		return

	GameState.player_name = "角色状态测试"
	GameState.player_name_color = 0xFF332211
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.player_exp = 1100
	GameState.player_level = 1
	GameState.player_hp = 275
	GameState.player_hp_max = 300
	GameState.player_mp = 88
	GameState.player_mp_max = 100
	var weapon := _item(weapon_id, 77)
	weapon.extAttrList = {1: PackedByteArray([1, 100, 0, 0, 0, 0])}
	GameState.wear = {3: weapon}
	GameState.inventory = [_item(dress_id, 78)]
	var combat := CombatCalculatorScript.calculate(GameState, resources)
	var weapon_attr: Dictionary = resources.item_attribute(weapon_id)
	if combat.dc[0] != 1 / 2 + weapon_attr.dc[0] or combat.dc[1] != 1 + weapon_attr.dc[1] + 100 or combat.load[1] != 12 + weapon_attr.load[1]:
		_fail("combat calculation mismatch: %s" % combat)
		return
	GameState.wear[4] = _item(shoe_id, 80)
	GameState.wear[5] = _item(necklace_id, 81)

	var panel: Control = load("res://scenes/game/panels/player_state.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	var name_label := panel.get_node("Name") as Label
	if name_label.text != "角色状态测试" or name_label.get_theme_color("font_color") != Color8(0x11, 0x22, 0x33):
		_fail("player name text/color does not match C++ state board: text=%s color=%s" % [name_label.text, name_label.get_theme_color("font_color")])
		return
	if panel.get_node("EquipmentSlots").get_child_count() != 11:
		_fail("all eleven wear grids are not interactive")
		return
	var equipment_slots := panel.get_node("EquipmentSlots")
	var shoe_slot := _slot_at(equipment_slots, Vector2(-8, 182))
	var necklace_slot := _slot_at(equipment_slots, Vector2(150, 30))
	var weapon_slot := _slot_at(equipment_slots, Vector2(22, 22))
	var shoe_frame: Dictionary = resources.frame("item", resources.item_package_gfx_id(shoe_id) | 0x01000000)
	var necklace_frame: Dictionary = resources.frame("item", resources.item_package_gfx_id(necklace_id) | 0x01000000)
	var shoe_image := shoe_slot.get_node_or_null("Icon") as TextureRect if shoe_slot else null
	var necklace_image := necklace_slot.get_node_or_null("Icon") as TextureRect if necklace_slot else null
	if shoe_image == null or shoe_slot.texture_normal != null or shoe_image.size != shoe_frame.texture.get_size() or shoe_image.position != Vector2((shoe_slot.size.x - shoe_image.size.x) / 2.0, shoe_slot.size.y - shoe_image.size.y):
		_fail("shoe icon native-size bottom alignment mismatch")
		return
	if necklace_image == null or necklace_slot.texture_normal != null or necklace_image.size != necklace_frame.texture.get_size() or necklace_image.position != (necklace_slot.size - necklace_image.size) / 2.0:
		_fail("ordinary wear icon native-size centering mismatch")
		return
	var shoe_hover := shoe_slot.get_node_or_null("HoverOverlay") as TextureRect if shoe_slot else null
	var necklace_hover := necklace_slot.get_node_or_null("HoverOverlay") as TextureRect if necklace_slot else null
	if shoe_hover == null or shoe_hover.texture == null or shoe_hover.position != Vector2(-1, -6) or not is_equal_approx(shoe_hover.modulate.a, 128.0 / 255.0):
		_fail("shoe slot hover overlay does not match C++")
		return
	if necklace_hover == null or necklace_hover.texture == null or necklace_hover.position != Vector2(-1, -3) or not is_equal_approx(necklace_hover.modulate.a, 128.0 / 255.0):
		_fail("ordinary wear slot hover overlay does not match C++")
		return
	if weapon_slot == null or weapon_slot.get_node_or_null("HoverOverlay") != null:
		_fail("large paper-doll wear slot unexpectedly has a hover overlay")
		return
	shoe_slot.mouse_entered.emit()
	if not shoe_hover.visible:
		_fail("wear hover overlay did not appear on mouse enter")
		return
	shoe_slot.mouse_exited.emit()
	if shoe_hover.visible:
		_fail("wear hover overlay did not hide on mouse exit")
		return
	if not panel.call("_can_wear", weapon_id, 3) or panel.call("_can_wear", weapon_id, 5):
		_fail("wear slot validation mismatch")
		return
	var panel_resources: RefCounted = panel.get("_resources")
	var original_dress_name: String = panel_resources.item_names[dress_id]
	panel_resources.item_names[dress_id] = "无性别衣服"
	if panel.call("_can_wear", dress_id, 1):
		_fail("unmarked dress bypassed original gender requirement")
		return
	panel_resources.item_names[dress_id] = original_dress_name
	GameState.grabbed_item = _item(dress_id, 79)
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	panel.call("_on_wear_pressed", 3)
	if not GameState.grabbed_item.is_empty() or AudioService.last_seff_id != resources.item_sound_effect(dress_id):
		_fail("invalid wear placement did not return the item with original sound")
		return
	if panel.get_node("StateValues").get_child_count() != 9 or "攻击" not in panel.get_node("CombatStats").text:
		_fail("state rows or combat labels missing")
		return
	get_viewport().warp_mouse(Vector2(520, 100))
	panel.call("_show_item_tooltip", 3, weapon)
	var tooltip := panel.get_node("ItemTooltip") as Panel
	var tooltip_lines := _label_texts(tooltip)
	var tooltip_style := tooltip.get_theme_stylebox("panel") as StyleBoxFlat
	var tooltip_label := tooltip.get_child(0) as Label
	if not tooltip.visible or tooltip.size.x != 220 or tooltip_label.position != Vector2(10, 10) or tooltip_label.get_theme_font_size("font_size") != 12 or not tooltip_label.get_theme_font("font").resource_path.ends_with("/01_Yahei.ttf") or not tooltip_lines.any(func(line): return "【名称】" in line) or not tooltip_lines.any(func(line): return "攻击" in line) or tooltip_style.border_width_left != 1 or tooltip_style.corner_radius_top_left != 5:
		_fail("equipment tooltip content/style mismatch: %s" % tooltip_lines)
		return
	if OS.has_environment("MIR2X_PLAYER_STATE_SCREENSHOT"):
		var screenshot_shoe_slot := _slot_at(panel.get_node("EquipmentSlots"), Vector2(-8, 182))
		if screenshot_shoe_slot == null:
			_fail("shoe slot missing before visual capture")
			return
		screenshot_shoe_slot.mouse_entered.emit()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PLAYER_STATE_SCREENSHOT"))
	print("PLAYER STATE PASS: metadata, combat, loads, eleven wear grids and visuals")
	get_tree().quit()


func _find_item(resources: RefCounted, type_name: String) -> int:
	for item_id in resources.item_types:
		if resources.item_types[item_id] == type_name and not resources.item_attribute(item_id).is_empty():
			return int(item_id)
	return 0


func _find_wear_icon(resources: RefCounted, type_name: String) -> int:
	for item_id_value in resources.item_types:
		var item_id := int(item_id_value)
		if resources.item_types[item_id] == type_name and not resources.frame("item", resources.item_package_gfx_id(item_id) | 0x01000000).is_empty():
			return item_id
	return 0


func _item(item_id: int, seq_id: int) -> Dictionary:
	return {"itemID": item_id, "seqID": seq_id, "count": 1, "duration": [0, 0], "extAttrList": {}}


func _label_texts(parent: Node) -> Array[String]:
	var result: Array[String] = []
	for child in parent.get_children():
		if child is Label:
			result.append(child.text)
	return result


func _slot_at(parent: Node, position: Vector2) -> TextureButton:
	for child in parent.get_children():
		if child is TextureButton and child.position == position:
			return child
	return null


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
