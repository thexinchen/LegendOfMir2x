extends RefCounted

# Minimal cereal binary archive deserializer
# cereal uses a simple binary format: size-prefix (VLQ) + raw bytes for each field
# This implements just enough to parse SDStartGameScene, SDHealth, SDItemStorage, etc.

var _buf: PackedByteArray
var _offset: int = 0


func _init(buf: PackedByteArray = PackedByteArray()) -> void:
	_buf = buf
	_offset = 0


func at_end() -> bool:
	return _offset >= _buf.size()


func read_u8() -> int:
	if _offset >= _buf.size():
		return 0
	var v := _buf[_offset]
	_offset += 1
	return v


func read_u16() -> int:
	if _offset + 2 > _buf.size():
		return 0
	var v := _buf.decode_u16(_offset)
	_offset += 2
	return v


func read_u32() -> int:
	if _offset + 4 > _buf.size():
		return 0
	var v := _buf.decode_u32(_offset)
	_offset += 4
	return v


func read_u64() -> int:
	if _offset + 8 > _buf.size():
		return 0
	var lo := _buf.decode_u32(_offset)
	var hi := _buf.decode_u32(_offset + 4)
	_offset += 8
	return lo | (hi << 32)


func read_s8() -> int:
	var v := read_u8()
	if v >= 128:
		return v - 256
	return v


func read_s16() -> int:
	var v := read_u16()
	if v >= 32768:
		return v - 65536
	return v


func read_s32() -> int:
	var v := read_u32()
	if v >= 2147483648:
		return v - 4294967296
	return v


func read_bool() -> bool:
	return read_u8() != 0


func read_float() -> float:
	if _offset + 4 > _buf.size():
		return 0.0
	var bytes := _buf.slice(_offset, _offset + 4)
	_offset += 4
	return bytes.decode_float(0) if false else _decode_float(bytes)


func read_string() -> String:
	# cereal binary: VLQ size prefix, then raw bytes
	var size := read_vlq()
	if size <= 0 or _offset + size > _buf.size():
		return ""
	var s := _buf.slice(_offset, _offset + size).get_string_from_utf8()
	_offset += size
	return s


func read_byte_array() -> PackedByteArray:
	var size := read_vlq()
	if size <= 0 or _offset + size > _buf.size():
		return PackedByteArray()
	var data := _buf.slice(_offset, _offset + size)
	_offset += size
	return data


func read_vlq() -> int:
	# cereal VLQ: 7 bits per byte, MSB = continuation, little-endian
	var value := 0
	var shift := 0
	while _offset < _buf.size():
		var b := _buf[_offset]
		_offset += 1
		value |= (b & 0x7F) << shift
		if not (b & 0x80):
			return value
		shift += 7
		if shift >= 64:
			return value
	return value


func skip(n: int) -> void:
	_offset += n


# --- High-level struct deserializers ---

# SDStartGameScene: uid, mapUID, x, y, direction, desp(string), name(string)
func read_sd_start_game_scene() -> Dictionary:
	return {
		"uid": read_u64(),
		"mapUID": read_u64(),
		"x": read_s16(),
		"y": read_s16(),
		"direction": read_u8(),
		"desp": read_string(),
		"name": read_string(),
	}

# SDHealth: hp, hpMax, mp, mpMax (all uint32)
func read_sd_health() -> Dictionary:
	return {
		"hp": read_u32(),
		"hpMax": read_u32(),
		"mp": read_u32(),
		"mpMax": read_u32(),
	}

# SDItem: itemID(u32), seqID(u32), count(u16), dur(u32) + more fields
# The exact layout depends on sditem.hpp, let's read the basic fields
func read_sd_item() -> Dictionary:
	var item_id := read_u32()
	var seq_id := read_u32()
	var count := read_u16()
	return {
		"itemID": item_id,
		"seqID": seq_id,
		"count": count,
	}

# SDInventory: list of SDItem (10x10 grid = 100 slots)
func read_sd_inventory() -> Array:
	var items := []
	items.resize(100)
	for i in range(100):
		items[i] = null
	# cereal writes: size_prefix(VLQ) then each element
	# For a fixed-size array, cereal may write just the elements
	# Need to check actual wire format
	var count := read_vlq()
	for i in range(mini(count, 100)):
		items[i] = read_sd_item()
	return items

# SDBuffIDList: count(VLQ) + list of buff IDs (u32 each)
func read_sd_buff_id_list() -> Array:
	var count := read_vlq()
	var ids := []
	for i in range(count):
		ids.append(read_u32())
	return ids

# SDPlayerName: uid(u64) + name(string)
func read_sd_player_name() -> Dictionary:
	return {
		"uid": read_u64(),
		"name": read_string(),
	}

# SDText: text content (string)
func read_sd_text() -> String:
	return read_string()

func _decode_float(bytes: PackedByteArray) -> float:
	# IEEE 754 little-endian
	var bits := bytes.decode_u32(0)
	return bits_to_float(bits)

func bits_to_float(bits: int) -> float:
	# Convert uint32 to float (IEEE 754)
	if bits == 0:
		return 0.0
	var sign := 1.0 if not (bits & 0x80000000) else -1.0
	var exp := (bits >> 23) & 0xFF
	var mantissa := bits & 0x7FFFFF
	if exp == 0 and mantissa == 0:
		return 0.0
	if exp == 255:
		return sign * INF if mantissa == 0 else NAN
	if exp == 0:
		return sign * mantissa * pow(2, -149)
	return sign * (1.0 + mantissa / 8388608.0) * pow(2, exp - 127)
