extends Node

const CerealReader = preload("res://scripts/network/cereal_reader.gd")


func _ready() -> void:
	if not _test_player_name_envelopes():
		get_tree().quit(1)
		return
	if not _test_start_game_scene():
		get_tree().quit(1)
		return
	if not _test_item_storage():
		get_tree().quit(1)
		return
	print("CEREAL READER PASS")
	get_tree().quit()


func _test_player_name_envelopes() -> bool:
	var archive := PackedByteArray([1])
	_append_u64(archive, 0x0102030405060708)
	_append_string(archive, "Hero")
	_append_u32(archive, 0x11223344)
	for payload in [_none_envelope(archive), _xor_envelope(archive), _zstd_envelope(archive)]:
		var reader := CerealReader.new(payload)
		var value := reader.read_sd_player_name()
		if not _expect(reader.valid, "player-name reader invalid: %s" % reader.error):
			return false
		if not _expect(value.uid == 0x0102030405060708, "player-name uid mismatch"):
			return false
		if not _expect(value.name == "Hero", "player-name text mismatch"):
			return false
		if not _expect(value.nameColor == 0x11223344, "player-name color mismatch"):
			return false
		if not _expect(reader.at_end(), "player-name archive has trailing bytes"):
			return false
	return true


func _test_start_game_scene() -> bool:
	var archive := PackedByteArray([1])
	_append_u64(archive, 101)
	_append_u64(archive, 202)
	_append_s32(archive, 12)
	_append_s32(archive, 34)
	_append_s32(archive, 5)
	_append_u64(archive, 0) # empty SDWear unordered_map
	_append_u32(archive, 7)
	_append_u32(archive, 0xAABBCCDD)
	_append_string(archive, "测试角色")
	var reader := CerealReader.new(_none_envelope(archive))
	var value := reader.read_sd_start_game_scene()
	return true \
		and _expect(reader.valid, "start-game reader invalid: %s" % reader.error) \
		and _expect(value.uid == 101 and value.mapUID == 202, "start-game uid mismatch") \
		and _expect(value.x == 12 and value.y == 34 and value.direction == 5, "start-game action mismatch") \
		and _expect(value.desp.hair == 7 and value.desp.hairColor == 0xAABBCCDD, "start-game look mismatch") \
		and _expect(value.name == "测试角色", "start-game name mismatch") \
		and _expect(reader.at_end(), "start-game archive has trailing bytes")


func _test_item_storage() -> bool:
	var archive := PackedByteArray([1])
	_append_u64(archive, 9876) # gold
	_append_u64(archive, 0) # empty wear
	for _slot in range(6):
		_append_item(archive, 0, 0, 1)
	_append_u64(archive, 1) # inventory vector size
	_append_item(archive, 42, 9, 3)
	var reader := CerealReader.new(_none_envelope(archive))
	var value := reader.read_sd_item_storage()
	return true \
		and _expect(reader.valid, "item-storage reader invalid: %s" % reader.error) \
		and _expect(value.gold == 9876, "item-storage gold mismatch") \
		and _expect(value.belt.size() == 6, "item-storage belt size mismatch") \
		and _expect(value.inventory.size() == 1, "item-storage inventory size mismatch") \
		and _expect(value.inventory[0].itemID == 42, "item-storage item id mismatch") \
		and _expect(value.inventory[0].seqID == 9 and value.inventory[0].count == 3, "item-storage item data mismatch") \
		and _expect(reader.at_end(), "item-storage archive has trailing bytes")


func _append_item(buf: PackedByteArray, item_id: int, seq_id: int, count: int) -> void:
	_append_u32(buf, item_id)
	_append_u32(buf, seq_id)
	_append_u64(buf, count)
	_append_u64(buf, 0)
	_append_u64(buf, 0)
	_append_u64(buf, 0) # empty extAttrList


func _none_envelope(archive: PackedByteArray) -> PackedByteArray:
	var result := archive.duplicate()
	result.append(0)
	return result


func _zstd_envelope(archive: PackedByteArray) -> PackedByteArray:
	var result := archive.compress(FileAccess.COMPRESSION_ZSTD)
	result.append(1)
	return result


func _xor_envelope(archive: PackedByteArray) -> PackedByteArray:
	var mask_size := (archive.size() + 7) / 8
	var result := PackedByteArray()
	result.resize(mask_size)
	result.fill(0)
	for index in range(archive.size()):
		if archive[index] != 0:
			result[index / 8] |= 1 << (index % 8)
			result.append(archive[index])
	result.append(archive.size())
	result.append(2)
	return result


func _append_string(buf: PackedByteArray, value: String) -> void:
	var encoded := value.to_utf8_buffer()
	_append_u64(buf, encoded.size())
	buf.append_array(encoded)


func _append_s32(buf: PackedByteArray, value: int) -> void:
	_append_u32(buf, value & 0xFFFFFFFF)


func _append_u32(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 4)
	buf.encode_u32(offset, value)


func _append_u64(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 8)
	buf.encode_u32(offset, value & 0xFFFFFFFF)
	buf.encode_u32(offset + 4, (value >> 32) & 0xFFFFFFFF)


func _expect(condition: bool, message: String) -> bool:
	if condition:
		return true
	push_error(message)
	return false
