extends RefCounted

# Binary protocol helpers for parsing packed struct messages
# All structs use #pragma pack(1) (no alignment padding)

# ActionNode layout (28 bytes, packed):
#   uint16_t bitfield0: type(5) | speed(9)       -> 2 bytes
#   uint16_t bitfield1: direction(5) | pad(11)   -> 2 bytes  
#   int16_t x                                     -> 2 bytes
#   int16_t y                                     -> 2 bytes
#   int16_t aimX                                  -> 2 bytes
#   int16_t aimY                                  -> 2 bytes
#   uint64_t aimUID/fromUID                       -> 8 bytes
#   uint8_t extParam[8]                           -> 8 bytes
# Total: 28 bytes

static func decode_action_node(buf: PackedByteArray, offset: int = 0) -> Dictionary:
	if buf.size() < offset + 28:
		return {}
	var bf0 := buf.decode_u16(offset)
	var bf1 := buf.decode_u16(offset + 2)
	return {
		"type": bf0 & 0x1F,
		"speed": (bf0 >> 5) & 0x1FF,
		"direction": bf1 & 0x1F,
		"x": _decode_s16(buf, offset + 4),
		"y": _decode_s16(buf, offset + 6),
		"aimX": _decode_s16(buf, offset + 8),
		"aimY": _decode_s16(buf, offset + 10),
		"aimUID": _decode_u64(buf, offset + 12),
		"extParam": buf.slice(offset + 20, offset + 28),
	}

static func encode_action_node(action: Dictionary) -> PackedByteArray:
	var buf := PackedByteArray()
	buf.resize(28)
	buf.fill(0)
	var bf0: int = (action.get("type", 0) & 0x1F) | ((action.get("speed", 100) & 0x1FF) << 5)
	var bf1: int = action.get("direction", 0) & 0x1F
	buf.encode_u16(0, bf0)
	buf.encode_u16(2, bf1)
	_encode_s16(buf, 4, action.get("x", 0))
	_encode_s16(buf, 6, action.get("y", 0))
	_encode_s16(buf, 8, action.get("aimX", 0))
	_encode_s16(buf, 10, action.get("aimY", 0))
	_encode_u64(buf, 12, action.get("aimUID", 0))
	var ext: PackedByteArray = action.get("extParam", PackedByteArray())
	for i in range(mini(ext.size(), 8)):
		buf[20 + i] = ext[i]
	return buf

# SMAction (44 bytes): uid(8) + mapUID(8) + action(28)
static func decode_sm_action(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 44:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"action": decode_action_node(buf, 16),
	}

# SMOnlineOK (176 bytes): uid(8) + name(StaticBuffer<128>=131) + gender/job(1) + mapUID(8) + action(28)
# Note: StaticBuffer<128> = 2+129=131, odd but NOT padded because it's followed by uint8_t bitfield
# Total with pack(1): 8 + 131 + 1 + 8 + 28 = 176
static func decode_sm_online_ok(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 176:
		return {}
	var name_len := buf.decode_u16(8)
	var name_text := buf.slice(10, 10 + name_len).get_string_from_utf8()
	var gender_job := buf[139]
	return {
		"uid": _decode_u64(buf, 0),
		"name": name_text,
		"gender": gender_job & 1,
		"job": (gender_job >> 1) & 7,
		"mapUID": _decode_u64(buf, 140),
		"action": decode_action_node(buf, 148),
	}

# SMCORecord (52 bytes): uid(8) + mapUID(8) + action(28) + union(8)
static func decode_sm_corecord(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 52:
		return {}
	var result := {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"action": decode_action_node(buf, 16),
	}
	# Union at offset 44, 8 bytes
	# We don't know which type without context, return raw
	result["union_data"] = buf.slice(44, 52)
	return result

# SMExp (4 bytes)
static func decode_sm_exp(buf: PackedByteArray) -> int:
	if buf.size() < 4:
		return 0
	return buf.decode_u32(0)

# SMGold (4 bytes)
static func decode_sm_gold(buf: PackedByteArray) -> int:
	if buf.size() < 4:
		return 0
	return buf.decode_u32(0)

# SMBuff (16 bytes): uid(8) + type(4) + state(4)
static func decode_sm_buff(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 16:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"type": buf.decode_u32(8),
		"state": buf.decode_u32(12),
	}

# SMMiss (8 bytes): uid
static func decode_sm_miss(buf: PackedByteArray) -> int:
	if buf.size() < 8:
		return 0
	return _decode_u64(buf, 0)

# SMNotifyDead (8 bytes): uid
static func decode_sm_notify_dead(buf: PackedByteArray) -> int:
	if buf.size() < 8:
		return 0
	return _decode_u64(buf, 0)

# SMOffline (16 bytes): uid(8) + mapUID(8)
static func decode_sm_offline(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 16:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
	}

# SMPing (4 bytes): tick
static func decode_sm_ping(buf: PackedByteArray) -> int:
	if buf.size() < 4:
		return 0
	return buf.decode_u32(0)

# SMStrikeGrid (8 bytes): x(4) + y(4)
static func decode_sm_strike_grid(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 8:
		return {}
	return {
		"x": buf.decode_u32(0),
		"y": buf.decode_u32(4),
	}

# SMPlayerSay (136 bytes): uid(8) + content[128]
static func decode_sm_player_say(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 136:
		return {}
	var content: PackedByteArray = buf.slice(8, 8 + 128)
	var null_pos: int = 128
	for i in range(128):
		if content[i] == 0:
			null_pos = i
			break
	return {
		"uid": _decode_u64(buf, 0),
		"content": content.slice(0, null_pos).get_string_from_utf8(),
	}

# SMPlayerBroadcast (136 bytes): uid(8) + content[128]
static func decode_sm_player_broadcast(buf: PackedByteArray) -> Dictionary:
	return decode_sm_player_say(buf)

# SMCastMagic (28 bytes): uid(8) + mapUID(8) + magic(1) + magicParam(1) + speed(1) + direction(1) + x(2) + y(2) + aimX(2) + aimY(2) + aimUID(8)
static func decode_sm_cast_magic(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 28:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"magic": buf[16],
		"magicParam": buf[17],
		"speed": buf[18],
		"direction": buf[19],
		"x": buf.decode_u16(20),
		"y": buf.decode_u16(22),
		"aimX": buf.decode_u16(24),
		"aimY": buf.decode_u16(26),
		"aimUID": _decode_u64(buf, 28) if buf.size() >= 36 else 0,
	}

# CMAction (44 bytes): uid(8) + mapUID(8) + action(28)
static func encode_cm_action(uid: int, map_uid: int, action: Dictionary) -> PackedByteArray:
	var buf := PackedByteArray()
	buf.resize(44)
	_encode_u64(buf, 0, uid)
	_encode_u64(buf, 8, map_uid)
	var action_buf := encode_action_node(action)
	for i in range(28):
		buf[16 + i] = action_buf[i]
	return buf

# CMPing (4 bytes): tick
static func encode_cm_ping(tick: int) -> PackedByteArray:
	var buf := PackedByteArray()
	buf.resize(4)
	buf.encode_u32(0, tick)
	return buf

# --- low-level helpers ---

static func _decode_u64(buf: PackedByteArray, offset: int) -> int:
	var lo := buf.decode_u32(offset)
	var hi := buf.decode_u32(offset + 4)
	return lo | (hi << 32)

static func _encode_u64(buf: PackedByteArray, offset: int, value: int) -> void:
	buf.encode_u32(offset, value & 0xFFFFFFFF)
	buf.encode_u32(offset + 4, (value >> 32) & 0xFFFFFFFF)

static func _decode_s16(buf: PackedByteArray, offset: int) -> int:
	var v := buf.decode_u16(offset)
	if v >= 32768:
		return v - 65536
	return v

static func _encode_s16(buf: PackedByteArray, offset: int, value: int) -> void:
	if value < 0:
		value += 65536
	buf.encode_u16(offset, value & 0xFFFF)
