extends Node

const WorldResourceScript = preload("res://scripts/game/world_resource.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var world: RefCounted = WorldResourceScript.new()
	if not world.load_map(24):
		_fail(world.last_error)
		return
	if world.width != 600 or world.height != 600 or world.tiles.size() != 89993:
		_fail("unexpected map data: %dx%d tiles=%d" % [world.width, world.height, world.tiles.size()])
		return
	if world.minimap_id != 0x19001003:
		_fail("unexpected minimap ID: %08X" % world.minimap_id)
		return
	if world.map_name != "道馆":
		_fail("unexpected map name: %s" % world.map_name)
		return
	if world.bgm_id != 0x00010002:
		_fail("unexpected BGM ID: %08X" % world.bgm_id)
		return
	var first_texture_id: int = world.tiles.values()[0]
	if world.texture(first_texture_id) == null:
		_fail("failed to decode map texture %08X" % first_texture_id)
		return
	var actors: RefCounted = ActorResourceScript.new()
	if not actors.configure(world.base_path):
		_fail("failed to load actor indices")
		return
	if actors.offsets.size() < 345000 or actors.monster_look(224) <= 0 or actors.item_meta.size() < 1000:
		_fail("incomplete actor indices")
		return
	if actors.item_details.size() != actors.item_meta.size() or actors.buff_names.is_empty():
		_fail("item-detail or buff-name metadata incomplete")
		return
	var durable_item_count := 0
	var described_item_count := 0
	for item_id_value in actors.item_details:
		var detail: Dictionary = actors.item_detail(int(item_id_value))
		durable_item_count += 1 if int(detail.get("duration", 0)) > 0 else 0
		described_item_count += 1 if not str(detail.get("description", "")).is_empty() else 0
	if durable_item_count == 0 or described_item_count == 0:
		_fail("item-detail metadata lacks durable or described records")
		return
	if actors.monster_seff(224, 7) == 0xFFFFFFFF:
		_fail("monster SEFF metadata unavailable")
		return
	var special_spawn_count := 0
	for monster_id_value in actors.monster_meta:
		special_spawn_count += 1 if actors.monster_spawn_look(int(monster_id_value)) > 0 else 0
	if special_spawn_count != 1:
		_fail("monster meta v4 special spawn look mismatch: %d" % special_spawn_count)
		return
	var transform_count := 0
	var hidden_focusable_count := 0
	var reveal_on_hit_count := 0
	for monster_id_value in actors.monster_meta:
		var transform: Dictionary = actors.monster_transform(int(monster_id_value))
		if transform.is_empty():
			continue
		transform_count += 1
		hidden_focusable_count += 1 if transform.hidden_focusable else 0
		reveal_on_hit_count += 1 if transform.reveal_on_hit else 0
		if transform.hidden_stand[2] <= 0 or transform.active_transform[2] <= 0 or transform.hidden_transform[2] <= 0:
			_fail("monster meta v5 contains an empty transformation sequence")
			return
	if transform_count != 10 or hidden_focusable_count != 1 or reveal_on_hit_count != 1:
		_fail("monster meta v5 transformation rules mismatch: transform=%d hidden_focusable=%d reveal_on_hit=%d" % [transform_count, hidden_focusable_count, reveal_on_hit_count])
		return
	var fade_monster_count := 0
	var persistent_corpse_count := 0
	for monster_id_value in actors.monster_meta:
		if actors.monster_dead_fade_out(int(monster_id_value)):
			fade_monster_count += 1
		else:
			persistent_corpse_count += 1
	if fade_monster_count == 0 or persistent_corpse_count == 0:
		_fail("monster meta v3 death lifecycle flags unavailable")
		return
	var magic_seff_count := 0
	var magic_target_offset_count := 0
	for meta_value in actors.magic_meta.values():
		var meta: PackedInt32Array = meta_value
		magic_seff_count += 1 if meta.size() >= 9 and meta[8] != 0xFFFFFFFF else 0
		if meta.size() >= 41:
			for offset_index in range(9, 41):
				magic_target_offset_count += 1 if meta[offset_index] != 0 else 0
	if magic_seff_count == 0:
		_fail("magic-stage SEFF metadata unavailable")
		return
	if magic_target_offset_count == 0:
		_fail("magic target-offset metadata v4 unavailable")
		return
	var weapon_sound_count := 0
	var mine_weapon_count := 0
	for attributes_value in actors.item_attributes.values():
		var attributes: Dictionary = attributes_value
		weapon_sound_count += 1 if attributes.get("weapon_sound", 7) < 7 else 0
		mine_weapon_count += 1 if attributes.get("mine", false) else 0
	if weapon_sound_count == 0:
		_fail("weapon sound metadata unavailable")
		return
	if mine_weapon_count == 0:
		_fail("item meta v5 did not export mine-capable weapons")
		return
	print("WORLD RESOURCE PASS: map=%d size=%dx%d tiles=%d actor_frames=%d magic_seff=%d magic_target_offsets=%d fade_monster=%d persistent_corpse=%d special_spawn=%d transform=%d weapon_sound=%d mine_weapon=%d durable=%d described=%d buffs=%d" % [world.map_id, world.width, world.height, world.tiles.size(), actors.offsets.size(), magic_seff_count, magic_target_offset_count, fade_monster_count, persistent_corpse_count, special_spawn_count, transform_count, weapon_sound_count, mine_weapon_count, durable_item_count, described_item_count, actors.buff_names.size()])
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("WORLD_RESOURCE_SMOKE %s" % message)
	get_tree().quit(1)
