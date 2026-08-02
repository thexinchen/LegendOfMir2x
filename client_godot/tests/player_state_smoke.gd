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
	if weapon_id == 0 or dress_id == 0:
		_fail("wearable item metadata unavailable")
		return

	GameState.player_name = "角色状态测试"
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

	var panel: Control = load("res://scenes/game/panels/player_state.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("EquipmentSlots").get_child_count() != 11:
		_fail("all eleven wear grids are not interactive")
		return
	if not panel.call("_can_wear", weapon_id, 3) or panel.call("_can_wear", weapon_id, 5):
		_fail("wear slot validation mismatch")
		return
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
	if not tooltip.visible or tooltip.size.x != 220 or not tooltip_lines.any(func(line): return "【名称】" in line) or not tooltip_lines.any(func(line): return "攻击" in line) or tooltip_style.border_width_left != 1 or tooltip_style.corner_radius_top_left != 5:
		_fail("equipment tooltip content/style mismatch: %s" % tooltip_lines)
		return
	if OS.has_environment("MIR2X_PLAYER_STATE_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PLAYER_STATE_SCREENSHOT"))
	print("PLAYER STATE PASS: metadata, combat, loads, eleven wear grids and visuals")
	get_tree().quit()


func _find_item(resources: RefCounted, type_name: String) -> int:
	for item_id in resources.item_types:
		if resources.item_types[item_id] == type_name and not resources.item_attribute(item_id).is_empty():
			return int(item_id)
	return 0


func _item(item_id: int, seq_id: int) -> Dictionary:
	return {"itemID": item_id, "seqID": seq_id, "count": 1, "duration": [0, 0], "extAttrList": {}}


func _label_texts(parent: Node) -> Array[String]:
	var result: Array[String] = []
	for child in parent.get_children():
		if child is Label:
			result.append(child.text)
	return result


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
