class_name ActorResource
extends RefCounted

const MAGIC := "M2SP"

var base_path: String = ""
var offsets: Dictionary = {}
var monster_meta: Dictionary = {}
var item_meta: Dictionary = {}
var _textures: Dictionary = {}


func configure(path: String) -> bool:
	base_path = path
	var loaded := false
	for family in ["hero", "hair", "helmet", "weapon", "monster", "npc", "item"]:
		loaded = _load_index(family) or loaded
	_load_monster_meta()
	_load_item_meta()
	return loaded


func configure_default() -> bool:
	var candidates: Array[String] = []
	var env_path := OS.get_environment("MIR2X_WORLD_RES")
	if not env_path.is_empty():
		candidates.append(env_path)
	candidates.append("res://world_res")
	if not OS.has_feature("editor"):
		candidates.append(OS.get_executable_path().get_base_dir().path_join("world_res"))
	for candidate in candidates:
		if FileAccess.file_exists("%s/sprites/item.m2xindex" % candidate):
			return configure(candidate)
	return false


func frame(family: String, key: int) -> Dictionary:
	var cache_key := "%s:%08X" % [family, key]
	if _textures.has(cache_key):
		return _textures[cache_key]
	var offset: Vector2i = offsets.get(cache_key, Vector2i.ZERO)
	var bytes := FileAccess.get_file_as_bytes("%s/sprites/%s/%08X.png" % [base_path, family, key])
	if bytes.is_empty():
		_textures[cache_key] = {}
		return {}
	var image := Image.new()
	if image.load_png_from_buffer(bytes) != OK:
		_textures[cache_key] = {}
		return {}
	var result := {"texture": ImageTexture.create_from_image(image), "offset": offset}
	_textures[cache_key] = result
	return result


func monster_look(monster_id: int) -> int:
	return monster_meta.get(monster_id, PackedInt32Array([monster_id, 1]))[0]


func monster_has_shadow(monster_id: int) -> bool:
	return bool(monster_meta.get(monster_id, PackedInt32Array([monster_id, 1]))[1])


func item_shape(item_id: int) -> int:
	return item_meta.get(item_id, PackedInt32Array([0, 0]))[0]


func item_package_gfx_id(item_id: int) -> int:
	return item_meta.get(item_id, PackedInt32Array([0, 0]))[1]


func _load_index(family: String) -> bool:
	var file := FileAccess.open("%s/sprites/%s.m2xindex" % [base_path, family], FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return false
	if file.get_32() != 1:
		return false
	var count := file.get_32()
	for _index in range(count):
		var key := file.get_32()
		var dx := file.get_16()
		var dy := file.get_16()
		if dx & 0x8000:
			dx -= 0x10000
		if dy & 0x8000:
			dy -= 0x10000
		offsets["%s:%08X" % [family, key]] = Vector2i(dx, dy)
	return true


func _load_monster_meta() -> void:
	var file := FileAccess.open("%s/sprites/monster.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var monster_id := file.get_32()
		var look_id := file.get_16()
		var shadow := file.get_8()
		file.get_8()
		monster_meta[monster_id] = PackedInt32Array([look_id, shadow])


func _load_item_meta() -> void:
	var file := FileAccess.open("%s/sprites/item.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var item_id := file.get_32()
		var shape := file.get_16()
		file.get_16()
		var package_gfx_id := file.get_32()
		item_meta[item_id] = PackedInt32Array([shape, package_gfx_id])
