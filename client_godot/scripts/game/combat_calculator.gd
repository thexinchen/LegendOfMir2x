class_name CombatCalculator
extends RefCounted

const ELEMENT_COUNT := 7


static func calculate(state: Node, resources: RefCounted) -> Dictionary:
	var level: int = state.player_level
	var result := {
		"dc": PackedInt32Array([0, 0]),
		"mc": PackedInt32Array([0, 0]),
		"sc": PackedInt32Array([0, 0]),
		"ac": PackedInt32Array([level / 2, level / 2 + level % 2]),
		"mac": PackedInt32Array([level / 2, level / 2 + level % 2]),
		"dc_hit": 0,
		"mc_hit": 0,
		"dc_dodge": 0,
		"mc_dodge": 0,
		"speed": 0,
		"comfort": 0,
		"luck_curse": 0,
		"dc_elem": PackedInt32Array([0, 0, 0, 0, 0, 0, 0]),
		"ac_elem": PackedInt32Array([0, 0, 0, 0, 0, 0, 0]),
		"load": PackedInt32Array([20 + level * 2, 10 + level * 2, 100 + level * 10]),
	}

	if state.player_job & 1:
		_add_level_pair(result.dc, level)
	if state.player_job & 2:
		_add_level_pair(result.sc, level)
	if state.player_job & 4:
		_add_level_pair(result.mc, level)

	for location in state.wear:
		var item: Dictionary = state.wear[location]
		var attr: Dictionary = resources.item_attribute(int(item.get("itemID", 0)))
		if attr.is_empty():
			continue
		for pair_name in ["dc", "mc", "sc", "ac", "mac"]:
			_add_pair(result[pair_name], attr.get(pair_name, PackedInt32Array([0, 0])))
		for scalar_name in ["dc_hit", "mc_hit", "dc_dodge", "mc_dodge", "speed", "comfort", "luck_curse"]:
			result[scalar_name] += int(attr.get(scalar_name, 0))
		_add_values(result.dc_elem, attr.get("dc_elem", PackedInt32Array()), ELEMENT_COUNT)
		_add_values(result.ac_elem, attr.get("ac_elem", PackedInt32Array()), ELEMENT_COUNT)
		_add_values(result.load, attr.get("load", PackedInt32Array()), 3)
		# The shared C++ CombatNode currently applies only the weapon's EA_DC bonus.
		var dc_attr: PackedByteArray = item.get("extAttrList", {}).get(1, PackedByteArray())
		if dc_attr.size() >= 5 and dc_attr[0] == 1:
			result.dc[1] += dc_attr.decode_s32(1)
		elif dc_attr.size() >= 4:
			result.dc[1] += dc_attr.decode_s32(0)

	var basic_sword_id: int = resources.magic_id("基本剑术")
	if basic_sword_id > 0:
		for magic_value in state.learned_magic:
			if int(magic_value.get("magicID", 0)) == basic_sword_id:
				result.dc_hit += 2
				break
	return result


static func inventory_load(state: Node, resources: RefCounted) -> int:
	var result := 0
	for item_value in state.inventory:
		result += resources.item_weight(int(item_value.get("itemID", 0)))
	return result


static func body_load(state: Node, resources: RefCounted) -> int:
	var result := 0
	for location in state.wear:
		if int(location) != 3:
			result += resources.item_weight(int(state.wear[location].get("itemID", 0)))
	return result


static func weapon_load(state: Node, resources: RefCounted) -> int:
	return resources.item_weight(int(state.wear.get(3, {}).get("itemID", 0)))


static func _add_level_pair(pair: PackedInt32Array, level: int) -> void:
	pair[0] += level / 2
	pair[1] += level / 2 + level % 2


static func _add_pair(target: PackedInt32Array, value: PackedInt32Array) -> void:
	if value.size() >= 2:
		target[0] += value[0]
		target[1] += value[1]


static func _add_values(target: PackedInt32Array, value: PackedInt32Array, count: int) -> void:
	for index in range(mini(count, value.size())):
		target[index] += value[index]
