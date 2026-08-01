class_name ActorResource
extends RefCounted

const MAGIC := "M2SP"

var base_path: String = ""
var offsets: Dictionary = {}
var monster_meta: Dictionary = {}
var item_meta: Dictionary = {}
var item_attributes: Dictionary = {}
var item_names: Dictionary = {}
var item_types: Dictionary = {}
var skill_meta: Dictionary = {}
var buff_meta: Dictionary = {}
var magic_meta: Dictionary = {}
var magic_names: Dictionary = {}
var magic_ids_by_name: Dictionary = {}
var _textures: Dictionary = {}


func configure(path: String) -> bool:
	base_path = path
	var loaded := false
	for family in ["hero", "hair", "helmet", "weapon", "monster", "npc", "item", "equip", "proguse", "magic"]:
		loaded = _load_index(family) or loaded
	_load_monster_meta()
	_load_item_meta()
	_load_item_names()
	_load_item_types()
	_load_skill_meta()
	_load_buff_meta()
	_load_magic_meta()
	_load_magic_names()
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


func monster_dead_fade_out(monster_id: int) -> bool:
	var meta: PackedInt32Array = monster_meta.get(monster_id, PackedInt32Array())
	return meta.size() >= 7 and bool(meta[6])


func monster_spawn_look(monster_id: int) -> int:
	var meta: PackedInt32Array = monster_meta.get(monster_id, PackedInt32Array())
	return meta[7] if meta.size() >= 8 else 0


func monster_transform(monster_id: int) -> Dictionary:
	var meta: PackedInt32Array = monster_meta.get(monster_id, PackedInt32Array())
	if meta.size() < 19 or meta[14] <= 0:
		return {}
	var flags := int(meta[18])
	return {
		"hidden_look": meta[8],
		"hidden_stand": PackedInt32Array([meta[9], meta[10], meta[11]]),
		"active_transform": PackedInt32Array([meta[12], meta[13], meta[14]]),
		"hidden_transform": PackedInt32Array([meta[15], meta[16], meta[17]]),
		"active_reverse": bool(flags & 1),
		"hidden_reverse": bool(flags & 2),
		"hidden_focusable": bool(flags & 4),
		"fixed_direction": bool(flags & 8),
	}


func monster_seff(monster_id: int, action_type: int) -> int:
	var meta: PackedInt32Array = monster_meta.get(monster_id, PackedInt32Array())
	if meta.size() < 6:
		return 0xFFFFFFFF
	match action_type:
		1: return meta[2]
		7: return meta[3]
		11: return meta[4]
		13: return meta[5]
	return 0xFFFFFFFF


func item_shape(item_id: int) -> int:
	return item_meta.get(item_id, PackedInt32Array([0, 0]))[0]


func item_package_gfx_id(item_id: int) -> int:
	return item_meta.get(item_id, PackedInt32Array([0, 0]))[1]


func item_is_packable(item_id: int) -> bool:
	return bool(item_meta.get(item_id, PackedInt32Array([0, 0, 0]))[2])


func item_attribute(item_id: int) -> Dictionary:
	return item_attributes.get(item_id, {})


func item_weapon_sound(item_id: int) -> int:
	return item_attribute(item_id).get("weapon_sound", 7)


func item_can_mine(item_id: int) -> bool:
	return bool(item_attribute(item_id).get("mine", false))


func item_weight(item_id: int) -> int:
	return item_attribute(item_id).get("weight", 0)


func item_name(item_id: int) -> String:
	return item_names.get(item_id, "物品 %d" % item_id)


func item_type(item_id: int) -> String:
	return item_types.get(item_id, "")


func item_icon(item_id: int) -> Dictionary:
	var package_gfx_id := item_package_gfx_id(item_id)
	var icon := frame("item", package_gfx_id | 0x02000000)
	return frame("item", package_gfx_id | 0x01000000) if icon.is_empty() else icon


func ground_item(item_id: int) -> Dictionary:
	return frame("item", item_package_gfx_id(item_id))


func skill_layout(magic_id: int) -> PackedInt32Array:
	return skill_meta.get(magic_id, PackedInt32Array())


func buff_layout(buff_id: int) -> PackedInt32Array:
	return buff_meta.get(buff_id, PackedInt32Array())


func magic_layout(magic_id: int, stage: int) -> PackedInt32Array:
	return magic_meta.get(magic_id * 8 + stage, PackedInt32Array())


func magic_seff(magic_id: int, stage: int) -> int:
	var meta := magic_layout(magic_id, stage)
	return meta[8] if meta.size() >= 9 else 0xFFFFFFFF


func magic_target_offset(magic_id: int, stage: int, direction: int) -> Vector2i:
	var meta := magic_layout(magic_id, stage)
	var index := 9 + clampi(direction, 0, 15) * 2
	if meta.size() <= index + 1:
		return Vector2i.ZERO
	return Vector2i(meta[index], meta[index + 1])


func magic_id(name: String) -> int:
	return magic_ids_by_name.get(name, 0)


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
	var version := file.get_32()
	if version not in [1, 2, 3, 4, 5]:
		return
	var count := file.get_32()
	for _index in range(count):
		var monster_id := file.get_32()
		var look_id := file.get_16()
		var shadow := file.get_8()
		var flags := file.get_8()
		var meta := PackedInt32Array([look_id, shadow])
		if version >= 2:
			for _seff_index in range(4):
				meta.append(file.get_32())
		if version >= 3:
			meta.append(flags & 1)
		if version >= 4:
			meta.append(file.get_16())
		if version >= 5:
			meta.append(file.get_16())
			for _transform_index in range(10):
				meta.append(file.get_8())
		monster_meta[monster_id] = meta


func _load_item_meta() -> void:
	var file := FileAccess.open("%s/sprites/item.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	var version := file.get_32()
	if version not in [1, 2, 3, 4, 5]:
		return
	var count := file.get_32()
	for _index in range(count):
		var item_id := file.get_32()
		var shape := file.get_16()
		var flags := file.get_16()
		var package_gfx_id := file.get_32()
		item_meta[item_id] = PackedInt32Array([shape, package_gfx_id, flags])
		if version >= 2:
			item_attributes[item_id] = {
				"weight": _read_s32(file),
				"dc": _read_pair(file),
				"mc": _read_pair(file),
				"sc": _read_pair(file),
				"ac": _read_pair(file),
				"mac": _read_pair(file),
				"dc_hit": _read_s32(file),
				"mc_hit": _read_s32(file),
				"dc_dodge": _read_s32(file),
				"mc_dodge": _read_s32(file),
				"speed": _read_s32(file),
				"comfort": _read_s32(file),
				"luck_curse": _read_s32(file),
				"dc_elem": _read_values(file, 7),
				"ac_elem": _read_values(file, 7),
				"load": _read_values(file, 3),
			}
			if version >= 3:
				item_attributes[item_id]["double_hand"] = file.get_8() != 0
				if version >= 4:
					item_attributes[item_id]["weapon_sound"] = file.get_8()
					if version >= 5:
						item_attributes[item_id]["mine"] = file.get_8() != 0
						file.get_8()
					else:
						file.get_buffer(2)
				else:
					file.get_buffer(3)


func _read_s32(file: FileAccess) -> int:
	var value := file.get_32()
	return value - 0x100000000 if value >= 0x80000000 else value


func _read_pair(file: FileAccess) -> PackedInt32Array:
	return PackedInt32Array([_read_s32(file), _read_s32(file)])


func _read_values(file: FileAccess, count: int) -> PackedInt32Array:
	var result := PackedInt32Array()
	for _index in range(count):
		result.append(_read_s32(file))
	return result


func _load_item_names() -> void:
	var file := FileAccess.open("%s/sprites/item_name.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var item_id := file.get_32()
		var length := file.get_16()
		item_names[item_id] = file.get_buffer(length).get_string_from_utf8()


func _load_item_types() -> void:
	var file := FileAccess.open("%s/sprites/item_type.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var item_id := file.get_32()
		var length := file.get_16()
		item_types[item_id] = file.get_buffer(length).get_string_from_utf8()


func _load_skill_meta() -> void:
	var file := FileAccess.open("%s/sprites/skill.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	var version := file.get_32()
	if version not in [1, 2]:
		return
	var count := file.get_32()
	for _index in range(count):
		var magic_id := file.get_32()
		var icon_id := file.get_32()
		var cool_down := file.get_32() if version >= 2 else 0
		var page := file.get_8()
		var x := file.get_8()
		var y := file.get_8()
		var flags := file.get_8()
		skill_meta[magic_id] = PackedInt32Array([icon_id, page, x, y, flags, cool_down])


func _load_buff_meta() -> void:
	var file := FileAccess.open("%s/sprites/buff.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var buff_id := file.get_32()
		var icon_id := file.get_32()
		var favor := file.get_8()
		if favor & 0x80:
			favor -= 0x100
		file.get_buffer(3)
		buff_meta[buff_id] = PackedInt32Array([icon_id, favor])


func _load_magic_meta() -> void:
	var file := FileAccess.open("%s/sprites/magic.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	var version := file.get_32()
	if version not in [1, 2, 3, 4]:
		return
	var count := file.get_32()
	for _index in range(count):
		var magic_id := file.get_32()
		var gfx_id := file.get_32()
		var mod_color := file.get_32()
		var frame_count := file.get_16()
		var gfx_id_count := file.get_16()
		var speed := file.get_16()
		var stage := file.get_8()
		var type := file.get_8()
		var gfx_dir_type := file.get_8()
		var flags := file.get_8()
		var meta := PackedInt32Array([
			gfx_id, mod_color, frame_count, gfx_id_count, speed, type, gfx_dir_type, flags,
		])
		if version >= 2:
			meta.append(file.get_32())
		if version >= 4:
			for _offset_index in 32:
				var target_offset := file.get_16()
				meta.append(target_offset - 0x10000 if target_offset >= 0x8000 else target_offset)
		magic_meta[magic_id * 8 + stage] = meta


func _load_magic_names() -> void:
	var file := FileAccess.open("%s/sprites/magic_name.m2xmeta" % base_path, FileAccess.READ)
	if file == null or file.get_buffer(4).get_string_from_ascii() != MAGIC:
		return
	if file.get_32() != 1:
		return
	var count := file.get_32()
	for _index in range(count):
		var id := file.get_32()
		var length := file.get_16()
		var name := file.get_buffer(length).get_string_from_utf8()
		magic_names[id] = name
		magic_ids_by_name[name] = id
