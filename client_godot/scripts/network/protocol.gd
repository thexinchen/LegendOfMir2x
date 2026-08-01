extends RefCounted

# Binary protocol helpers for parsing packed struct messages
# All structs use #pragma pack(1) (no alignment padding)

# ActionNode layout (27 bytes, packed, verified against GCC):
#   uint16_t bitfield0: type(5) | speed(9)       -> 2 bytes
#   uint8_t bitfield1: direction(5)               -> 1 byte
#   int16_t x                                     -> 2 bytes
#   int16_t y                                     -> 2 bytes
#   int16_t aimX                                  -> 2 bytes
#   int16_t aimY                                  -> 2 bytes
#   uint64_t aimUID/fromUID                       -> 8 bytes
#   uint8_t extParam[8]                           -> 8 bytes
# Total: 27 bytes

static func decode_action_node(buf: PackedByteArray, offset: int = 0) -> Dictionary:
	if buf.size() < offset + 27:
		return {}
	var bf0 := buf.decode_u16(offset)
	var result := {
		"type": bf0 & 0x1F,
		"speed": (bf0 >> 5) & 0x1FF,
		"direction": buf[offset + 2] & 0x1F,
		"x": _decode_s16(buf, offset + 3),
		"y": _decode_s16(buf, offset + 5),
		"aimX": _decode_s16(buf, offset + 7),
		"aimY": _decode_s16(buf, offset + 9),
		"aimUID": _decode_u64(buf, offset + 11),
		"extParam": buf.slice(offset + 19, offset + 27),
	}
	if result.type == 7 or result.type == 9:
		result["magicID"] = buf.decode_u32(offset + 19)
		result["modifierID"] = buf.decode_u32(offset + 23)
	return result

static func encode_action_node(action: Dictionary) -> PackedByteArray:
	var buf := PackedByteArray()
	buf.resize(27)
	buf.fill(0)
	var bf0: int = (action.get("type", 0) & 0x1F) | ((action.get("speed", 100) & 0x1FF) << 5)
	buf.encode_u16(0, bf0)
	buf[2] = action.get("direction", 0) & 0x1F
	_encode_s16(buf, 3, action.get("x", 0))
	_encode_s16(buf, 5, action.get("y", 0))
	_encode_s16(buf, 7, action.get("aimX", 0))
	_encode_s16(buf, 9, action.get("aimY", 0))
	_encode_u64(buf, 11, action.get("aimUID", 0))
	var ext: PackedByteArray = action.get("extParam", PackedByteArray())
	for i in range(mini(ext.size(), 8)):
		buf[19 + i] = ext[i]
	if action.get("type", 0) in [7, 9]:
		if action.has("magicID"):
			buf.encode_u32(19, action.magicID)
		if action.has("modifierID"):
			buf.encode_u32(23, action.modifierID)
	return buf

# SMAction (43 bytes): uid(8) + mapUID(8) + action(27)
static func decode_sm_action(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 43:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"action": decode_action_node(buf, 16),
	}

# SMOnlineOK (176 bytes): uid(8) + name(StaticBuffer<128>=132) + gender/job(1) + mapUID(8) + action(27)
static func decode_sm_online_ok(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 176:
		return {}
	var name_len := buf.decode_u16(8)
	var name_text := buf.slice(10, 10 + name_len).get_string_from_utf8()
	var gender_job := buf[140]
	return {
		"uid": _decode_u64(buf, 0),
		"name": name_text,
		"gender": gender_job & 1,
		"job": (gender_job >> 1) & 7,
		"mapUID": _decode_u64(buf, 141),
		"action": decode_action_node(buf, 149),
	}

# SMCORecord (48 bytes): uid(8) + mapUID(8) + action(27) + packed union(5)
static func decode_sm_corecord(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 48:
		return {}
	var result := {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"action": decode_action_node(buf, 16),
	}
	# Union at offset 43, 5 bytes
	# We don't know which type without context, return raw
	result["union_data"] = buf.slice(43, 48)
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

# SMDeadFadeOut (24 bytes): uid(8) + mapUID(8) + x(4) + y(4)
static func decode_sm_dead_fade_out(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 24:
		return {}
	return {
		"uid": _decode_u64(buf, 0),
		"mapUID": _decode_u64(buf, 8),
		"x": buf.decode_u32(16),
		"y": buf.decode_u32(20),
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

# SMCastMagic (36 bytes): uid(8) + mapUID(8) + magic(1) + magicParam(1) + speed(1) + direction(1) + x(2) + y(2) + aimX(2) + aimY(2) + aimUID(8)
static func decode_sm_cast_magic(buf: PackedByteArray) -> Dictionary:
	if buf.size() < 36:
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
		"aimUID": _decode_u64(buf, 28),
	}

# CMAction (43 bytes): uid(8) + mapUID(8) + action(27)
static func encode_cm_action(uid: int, map_uid: int, action: Dictionary) -> PackedByteArray:
	var buf := PackedByteArray()
	buf.resize(43)
	_encode_u64(buf, 0, uid)
	_encode_u64(buf, 8, map_uid)
	var action_buf := encode_action_node(action)
	for i in range(27):
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
