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
	var first_texture_id: int = world.tiles.values()[0]
	if world.texture(first_texture_id) == null:
		_fail("failed to decode map texture %08X" % first_texture_id)
		return
	var actors: RefCounted = ActorResourceScript.new()
	if not actors.configure(world.base_path):
		_fail("failed to load actor indices")
		return
	if actors.offsets.size() < 300000 or actors.monster_look(224) <= 0 or actors.item_meta.size() < 1000:
		_fail("incomplete actor indices")
		return
	print("WORLD RESOURCE PASS: map=%d size=%dx%d tiles=%d actor_frames=%d" % [world.map_id, world.width, world.height, world.tiles.size(), actors.offsets.size()])
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("WORLD_RESOURCE_SMOKE %s" % message)
	get_tree().quit(1)
