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
	if actors.monster_seff(224, 7) == 0xFFFFFFFF:
		_fail("monster SEFF metadata unavailable")
		return
	var magic_seff_count := 0
	for meta_value in actors.magic_meta.values():
		var meta: PackedInt32Array = meta_value
		magic_seff_count += 1 if meta.size() >= 9 and meta[8] != 0xFFFFFFFF else 0
	if magic_seff_count == 0:
		_fail("magic-stage SEFF metadata unavailable")
		return
	var weapon_sound_count := 0
	for attributes_value in actors.item_attributes.values():
		var attributes: Dictionary = attributes_value
		weapon_sound_count += 1 if attributes.get("weapon_sound", 7) < 7 else 0
	if weapon_sound_count == 0:
		_fail("weapon sound metadata unavailable")
		return
	print("WORLD RESOURCE PASS: map=%d size=%dx%d tiles=%d actor_frames=%d magic_seff=%d weapon_sound=%d" % [world.map_id, world.width, world.height, world.tiles.size(), actors.offsets.size(), magic_seff_count, weapon_sound_count])
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("WORLD_RESOURCE_SMOKE %s" % message)
	get_tree().quit(1)
