extends RefCounted

# Reader for cerealf::serialize() payloads.
# Envelope: PortableBinary archive bytes followed by CF_NONE / CF_ZSTD / CF_XOR.
# PortableBinary starts with a one-byte stream-endianness marker and uses uint64
# cereal::size_type values for strings and containers.

const CF_NONE := 0
const CF_ZSTD := 1
const CF_XOR := 2
const MAX_DECOMPRESSED_SIZE := 64 * 1024 * 1024
const MAX_CONTAINER_ITEMS := 1_000_000

var _buf := PackedByteArray()
var _offset := 0
var _little_endian := true
var valid := true
var error := ""


func _init(buf: PackedByteArray = PackedByteArray()) -> void:
	_buf = _decode_envelope(buf)
	_offset = 0
	if not valid:
		return
	if _buf.is_empty():
		_fail("empty PortableBinary archive")
		return
	_little_endian = _buf[0] != 0
	_offset = 1


func at_end() -> bool:
	return _offset >= _buf.size()


func remaining() -> int:
	return maxi(0, _buf.size() - _offset)


func read_u8() -> int:
	if not _require(1):
		return 0
	var value := _buf[_offset]
	_offset += 1
	return value


func read_u16() -> int:
	if not _require(2):
		return 0
	var value := _buf.decode_u16(_offset) if _little_endian else _decode_be(2)
	_offset += 2
	return value


func read_u32() -> int:
	if not _require(4):
		return 0
	var value := _buf.decode_u32(_offset) if _little_endian else _decode_be(4)
	_offset += 4
	return value


func read_u64() -> int:
	if not _require(8):
		return 0
	var value: int
	if _little_endian:
		value = _buf.decode_u32(_offset) | (_buf.decode_u32(_offset + 4) << 32)
	else:
		value = _decode_be(8)
	_offset += 8
	return value


func read_size() -> int:
	var value := read_u64()
	if value < 0 or value > MAX_CONTAINER_ITEMS:
		_fail("invalid container size: %d" % value)
		return 0
	return value


func read_s8() -> int:
	var value := read_u8()
	return value - 256 if value >= 128 else value


func read_s16() -> int:
	var value := read_u16()
	return value - 65536 if value >= 32768 else value


func read_s32() -> int:
	var value := read_u32()
	return value - 4294967296 if value >= 2147483648 else value


func read_bool() -> bool:
	return read_u8() != 0


func read_float() -> float:
	if not _require(4):
		return 0.0
	if not _little_endian:
		_fail("big-endian float archive is unsupported")
		return 0.0
	var value := _buf.decode_float(_offset)
	_offset += 4
	return value


func read_double() -> float:
	if not _require(8):
		return 0.0
	if not _little_endian:
		_fail("big-endian double archive is unsupported")
		return 0.0
	var value := _buf.decode_double(_offset)
	_offset += 8
	return value


func read_string() -> String:
	var size := read_size()
	if not valid or not _require(size):
		return ""
	var value := _buf.slice(_offset, _offset + size).get_string_from_utf8()
	_offset += size
	return value


func read_byte_array() -> PackedByteArray:
	var size := read_size()
	if not valid or not _require(size):
		return PackedByteArray()
	var value := _buf.slice(_offset, _offset + size)
	_offset += size
	return value


func read_optional(read_value: Callable) -> Variant:
	if not read_bool():
		return null
	return read_value.call()


func read_u32_vector() -> Array:
	var result: Array = []
	var count := read_size()
	for _index in range(count):
		result.append(read_u32())
	return result


func read_u64_vector() -> Array:
	var result: Array = []
	var count := read_size()
	for _index in range(count):
		result.append(read_u64())
	return result


func read_tagged_val_map() -> Dictionary:
	var result := {}
	var count := read_size()
	for _index in range(count):
		var key := read_s32()
		result[key] = read_s32()
	return result


func read_sd_item() -> Dictionary:
	var item := {
		"itemID": read_u32(),
		"seqID": read_u32(),
		"count": read_u64(),
		"duration": [read_u64(), read_u64()],
		"extAttrList": {},
	}
	var attr_count := read_size()
	for _index in range(attr_count):
		var attr_type := read_s32()
		item.extAttrList[attr_type] = read_byte_array()
	return item


func read_sd_wear() -> Dictionary:
	var result := {}
	var count := read_size()
	for _index in range(count):
		var wear_type := read_s32()
		result[wear_type] = read_sd_item()
	return result


func read_sd_belt() -> Array:
	var result: Array = []
	for _index in range(6):
		result.append(read_sd_item())
	return result


func read_sd_inventory() -> Array:
	var result: Array = []
	var count := read_size()
	for _index in range(count):
		result.append(read_sd_item())
	return result


func read_sd_wldesp() -> Dictionary:
	return {
		"wear": read_sd_wear(),
		"hair": read_u32(),
		"hairColor": read_u32(),
	}


func read_sd_uid_wldesp() -> Dictionary:
	return {"uid": read_u64(), "desp": read_sd_wldesp()}


func read_sd_start_game_scene() -> Dictionary:
	return {
		"uid": read_u64(),
		"mapUID": read_u64(),
		"x": read_s32(),
		"y": read_s32(),
		"direction": read_s32(),
		"desp": read_sd_wldesp(),
		"name": read_string(),
	}


func read_sd_item_storage() -> Dictionary:
	return {
		"gold": read_u64(),
		"wear": read_sd_wear(),
		"belt": read_sd_belt(),
		"inventory": read_sd_inventory(),
	}


func read_sd_health() -> Dictionary:
	var result := {
		"uid": read_u64(),
		"hp": read_s32(),
		"mp": read_s32(),
		"maxHP": read_s32(),
		"maxMP": read_s32(),
		"hpRecover": read_s32(),
		"mpRecover": read_s32(),
	}
	result["buffedMaxHP"] = read_tagged_val_map()
	result["buffedMaxMP"] = read_tagged_val_map()
	result["buffedHPRecover"] = read_tagged_val_map()
	result["buffedMPRecover"] = read_tagged_val_map()
	return result


func read_sd_player_name() -> Dictionary:
	return {
		"uid": read_u64(),
		"name": read_string(),
		"nameColor": read_u32(),
	}


func read_sd_ground_item_id_list() -> Dictionary:
	var result := {"mapUID": read_u64(), "grids": []}
	var count := read_size()
	for _index in range(count):
		result.grids.append({
			"x": read_s32(),
			"y": read_s32(),
			"items": read_u32_vector(),
		})
	return result


func read_sd_ground_firewall_list() -> Dictionary:
	var result := {"mapUID": read_u64(), "firewalls": []}
	var count := read_size()
	for _index in range(count):
		result.firewalls.append({
			"x": read_s32(),
			"y": read_s32(),
			"count": read_s32(),
		})
	return result


func read_sd_buff_id_list() -> Dictionary:
	return {"uid": read_u64(), "ids": read_u32_vector()}


func read_sd_learned_magic_list() -> Array:
	var result: Array = []
	var count := read_size()
	for _index in range(count):
		result.append({"magicID": read_u32(), "exp": read_s32()})
	return result


func read_sd_equip_wear() -> Dictionary:
	return {"uid": read_u64(), "wltype": read_s32(), "item": read_sd_item()}


func read_sd_grab_wear() -> Dictionary:
	return {"wltype": read_s32(), "item": read_sd_item()}


func read_sd_equip_belt() -> Dictionary:
	return {"slot": read_s32(), "item": read_sd_item()}


func read_sd_grab_belt() -> Dictionary:
	return {"slot": read_s32(), "item": read_sd_item()}


func read_sd_show_secured_item_list() -> Array:
	var result: Array = []
	var count := read_size()
	for _index in range(count):
		result.append(read_sd_item())
	return result


func read_sd_update_item() -> Dictionary:
	return read_sd_item()


func read_sd_team_player() -> Dictionary:
	return {"uid": read_u64(), "level": read_u32(), "name": read_string()}


func read_sd_team_candidate() -> Dictionary:
	return read_sd_team_player()


func read_sd_team_member_list() -> Dictionary:
	var result := {"teamLeader": read_u64(), "members": []}
	var count := read_size()
	for _index in range(count):
		result.members.append(read_sd_team_player())
	return result


func read_sd_quest_desp_update() -> Dictionary:
	var result := {"name": read_string(), "fsm": read_string(), "desp": null}
	if read_bool():
		result.desp = read_string()
	return result


func read_sd_quest_desp_list() -> Dictionary:
	var result := {}
	var quest_count := read_size()
	for _quest_index in range(quest_count):
		var quest_name := read_string()
		var state_map := {}
		var state_count := read_size()
		for _state_index in range(state_count):
			var fsm := read_string()
			state_map[fsm] = read_string()
		result[quest_name] = state_map
	return result


func read_sd_start_input() -> Dictionary:
	return {
		"uid": read_u64(),
		"title": read_string(),
		"commitTag": read_string(),
		"show": read_bool(),
	}


func read_sd_npc_xml_layout() -> Dictionary:
	return {
		"npcUID": read_u64(),
		"eventPath": read_string(),
		"xmlLayout": read_string(),
	}


func read_sd_npc_sell() -> Dictionary:
	return {"npcUID": read_u64(), "itemList": read_u32_vector()}


func read_sd_sell_item_list() -> Dictionary:
	var result := {"npcUID": read_u64(), "list": []}
	var count := read_size()
	for _index in range(count):
		var sell_item := {"item": read_sd_item(), "costList": []}
		var cost_count := read_size()
		for _cost_index in range(cost_count):
			sell_item.costList.append({"itemID": read_u32(), "count": read_size()})
		result.list.append(sell_item)
	return result


func read_sd_runtime_config() -> Dictionary:
	var result := {}
	var count := read_size()
	for _index in range(count):
		var config_id := read_s32()
		result[config_id] = read_byte_array()
	return result


func read_sd_player_config() -> Dictionary:
	var magic_keys := {}
	var magic_count := read_size()
	for _index in range(magic_count):
		var magic_id := read_u32()
		magic_keys[magic_id] = read_s8()
	return {"magicKeys": magic_keys, "runtimeConfig": read_sd_runtime_config()}


func _decode_envelope(source: PackedByteArray) -> PackedByteArray:
	if source.is_empty():
		_fail("empty cerealf payload")
		return PackedByteArray()
	var flag := source[source.size() - 1]
	var encoded := source.slice(0, source.size() - 1)
	match flag:
		CF_NONE:
			return encoded
		CF_XOR:
			return _decode_xor(encoded)
		CF_ZSTD:
			var decoded := encoded.decompress(MAX_DECOMPRESSED_SIZE, FileAccess.COMPRESSION_ZSTD)
			if decoded.is_empty():
				_fail("failed to decompress Zstd cerealf payload")
			return decoded
		_:
			_fail("invalid cerealf compression flag: %d" % flag)
			return PackedByteArray()


func _decode_xor(encoded: PackedByteArray) -> PackedByteArray:
	if encoded.is_empty():
		_fail("truncated XOR cerealf payload")
		return PackedByteArray()
	var output_size := encoded[encoded.size() - 1]
	var compressed := encoded.slice(0, encoded.size() - 1)
	var mask_size := (output_size + 7) / 8
	if compressed.size() < mask_size:
		_fail("truncated XOR mask")
		return PackedByteArray()
	var result := PackedByteArray()
	result.resize(output_size)
	result.fill(0)
	var data_offset := mask_size
	for index in range(output_size):
		if compressed[index / 8] & (1 << (index % 8)):
			if data_offset >= compressed.size():
				_fail("truncated XOR data")
				return PackedByteArray()
			result[index] = compressed[data_offset]
			data_offset += 1
	if data_offset != compressed.size():
		_fail("unexpected trailing XOR data")
	return result


func _require(size: int) -> bool:
	if not valid:
		return false
	if size < 0 or _offset + size > _buf.size():
		_fail("truncated archive at offset %d: need %d, have %d" % [_offset, size, remaining()])
		return false
	return true


func _decode_be(size: int) -> int:
	var result := 0
	for index in range(size):
		result = (result << 8) | _buf[_offset + index]
	return result


func _fail(message: String) -> void:
	if valid:
		error = message
	valid = false
