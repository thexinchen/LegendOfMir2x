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
	if panel.get_node("StateValues").get_child_count() != 9 or "攻击" not in panel.get_node("CombatStats").text:
		_fail("state rows or combat labels missing")
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


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
