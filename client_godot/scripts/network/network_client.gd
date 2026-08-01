extends Node

signal connection_changed(connected: bool, message: String)
signal message_received(head_code: int, payload: PackedByteArray)

# CMType enum (client -> server)
const CM_PING := 1
const CM_LOGIN := 2
const CM_QUERYCHAR := 3
const CM_CREATECHAR := 4
const CM_DELETECHAR := 5
const CM_CHANGEPASSWORD := 6
const CM_ONLINE := 7
const CM_ACTION := 8
const CM_SETMAGICKEY := 9
const CM_SETRUNTIMECONFIG := 10
const CM_QUERYCORECORD := 11
const CM_PICKUP := 19
const CM_QUERYGOLD := 21
const CM_QUERYPLAYERNAME := 23
const CM_QUERYPLAYERWLDESP := 24
const CM_NPCEVENT := 29
const CM_DROPITEM := 31
const CM_CONSUMEITEM := 32
const CM_BUY := 34
const CM_PLAYERSAY := 40
const CM_PLAYERBROADCAST := 41
const CM_REQUESTEQUIPWEAR := 42
const CM_REQUESTGRABWEAR := 43
const CM_REQUESTEQUIPBELT := 44
const CM_REQUESTGRABBELT := 45
const CM_REQUESTJOINTEAM := 46
const CM_REQUESTLEAVETEAM := 47
const CM_CREATEACCOUNT := 28

# SMType enum (server -> client)
const SM_OK := 1
const SM_ERROR := 2
const SM_PING := 3
const SM_UID := 4
const SM_LOGINOK := 5
const SM_LOGINERROR := 6
const SM_CREATEACCOUNTOK := 7
const SM_CREATEACCOUNTERROR := 8
const SM_CHANGEPASSWORDOK := 9
const SM_CHANGEPASSWORDERROR := 10
const SM_QUERYCHAROK := 11
const SM_QUERYCHARERROR := 12
const SM_CREATECHAROK := 13
const SM_CREATECHARERROR := 14
const SM_CREATECHATGROUP := 15
const SM_ADDFRIENDACCEPTED := 16
const SM_ADDFRIENDREJECTED := 17
const SM_DELETECHAROK := 18
const SM_DELETECHARERROR := 19
const SM_ONLINEOK := 20
const SM_ONLINEERROR := 21
const SM_STARTGAMESCENE := 22
const SM_PLAYERCONFIG := 23
const SM_FRIENDLIST := 24
const SM_LEARNEDMAGICLIST := 25
const SM_PLAYERWLDESP := 26
const SM_ACTION := 27
const SM_COREORD := 28
const SM_HEALTH := 29
const SM_NEXTSTRIKE := 30
const SM_NOTIFYDEAD := 31
const SM_DEADFADEOUT := 32
const SM_EXP := 33
const SM_BUFF := 34
const SM_BUFFIDLIST := 35
const SM_MISS := 36
const SM_CASTMAGIC := 37
const SM_OFFLINE := 38
const SM_PICKUPERROR := 39
const SM_REMOVEGROUNDITEM := 40
const SM_NPCXMLLAYOUT := 41
const SM_NPCSELL := 42
const SM_STARTINVOP := 43
const SM_STARTINPUT := 44
const SM_GOLD := 45
const SM_INVOPCOST := 46
const SM_STRIKEGRID := 47
const SM_SELLITEMLIST := 48
const SM_TEXT := 49
const SM_PLAYERNAME := 50
const SM_BUILDVERSION := 51
const SM_INVENTORY := 52
const SM_BELT := 53
const SM_UPDATEITEM := 54
const SM_REMOVEITEM := 55
const SM_REMOVESECUREDITEM := 56
const SM_BUYSUCCEED := 57
const SM_BUYERROR := 58
const SM_GROUNDITEMIDLIST := 59
const SM_GROUNDFIREWALLLIST := 60
const SM_EQUIPWEAR := 61
const SM_EQUIPWEARERROR := 62
const SM_GRABWEAR := 63
const SM_GRABWEARERROR := 64
const SM_EQUIPBELT := 65
const SM_EQUIPBELTERROR := 66
const SM_GRABBELT := 67
const SM_GRABBELTERROR := 68
const SM_SHOWSECUREDITEMLIST := 69
const SM_TEAMMEMBERLIST := 70
const SM_TEAMCANDIDATE := 71
const SM_TEAMERROR := 72
const SM_QUESTDESPUPDATE := 73
const SM_QUESTDESPLIST := 74
const SM_CHATMESSAGELIST := 75
const SM_PLAYERSAY := 76
const SM_PLAYERBROADCAST := 77

# StaticBuffer<N> = uint16_t size + uint8_t[N+1], padded to 2-byte alignment
# N=64: 2+65=67 -> pad to 68; N=128: 2+129=131 -> pad to 132
const STATIC_ID_SIZE := 68    # StaticBuffer<64>
const STATIC_NAME_SIZE := 68  # StaticBuffer<64>
const STATIC_PWD_SIZE := 68   # StaticBuffer<64>
const STATIC_BIGBUF_SIZE := 132 # StaticBuffer<128>

# ActionNode size: bitfields(4) + x(2) + y(2) + aimX(2) + aimY(2) + aimUID(8) + extParam(8) = 28
const ACTION_NODE_SIZE := 28

# System constants
const SYS_MAPGRIDXP := 48
const SYS_MAPGRIDYP := 32
const SYS_OBJMAXW := 3
const SYS_OBJMAXH := 25
const SYS_DEFFPS := 10
const SYS_DEFSPEED := 100
const SYS_U32NIL := 4294967295  # 0xFFFFFFFF
const SYS_U64NIL := -1           # 0xFFFFFFFFFFFFFFFF as signed int64

var _peer := StreamPeerTCP.new()
var _receive_buffer := PackedByteArray()
var _last_status := StreamPeerTCP.STATUS_NONE


func _process(_delta: float) -> void:
	_peer.poll()
	var status := _peer.get_status()
	if status != _last_status:
		_last_status = status
		match status:
			StreamPeerTCP.STATUS_CONNECTED:
				connection_changed.emit(true, "已连接服务器")
			StreamPeerTCP.STATUS_ERROR:
				connection_changed.emit(false, "连接服务器失败")
			StreamPeerTCP.STATUS_NONE:
				connection_changed.emit(false, "服务器连接已断开")
	if status != StreamPeerTCP.STATUS_CONNECTED:
		return
	var available := _peer.get_available_bytes()
	if available > 0:
		var read_result := _peer.get_data(available)
		if read_result[0] == OK:
			_receive_buffer.append_array(read_result[1])
			_parse_packets()


func connect_to_server() -> Error:
	var status := _peer.get_status()
	if status == StreamPeerTCP.STATUS_CONNECTED or status == StreamPeerTCP.STATUS_CONNECTING:
		return OK
	_peer = StreamPeerTCP.new()
	_receive_buffer.clear()
	_last_status = StreamPeerTCP.STATUS_NONE
	var host := str(ProjectSettings.get_setting("network/server_host", "127.0.0.1"))
	var port := int(ProjectSettings.get_setting("network/server_port", 7000))
	var error := _peer.connect_to_host(host, port)
	if error != OK:
		connection_changed.emit(false, "无法连接 %s:%d" % [host, port])
	return error


func is_connected_to_server() -> bool:
	return _peer.get_status() == StreamPeerTCP.STATUS_CONNECTED


func disconnect_from_server() -> void:
	_peer.disconnect_from_host()
	_receive_buffer.clear()


func login(account: String, password: String) -> Error:
	return _send_fixed_message(CM_LOGIN, _make_account_payload(account, password))


func create_account(account: String, password: String) -> Error:
	return _send_fixed_message(CM_CREATEACCOUNT, _make_account_payload(account, password))


func change_password(account: String, old_password: String, new_password: String) -> Error:
	var payload := PackedByteArray()
	payload.append_array(_make_static_buffer(account))
	payload.append_array(_make_static_buffer(old_password))
	payload.append_array(_make_static_buffer(new_password))
	return _send_fixed_message(CM_CHANGEPASSWORD, payload)


func query_character() -> Error:
	return _send_empty_message(CM_QUERYCHAR)


func create_character(character_name: String, job: int, male: bool) -> Error:
	var payload := _make_static_buffer(character_name)
	payload.append(job)
	payload.append(1 if male else 0)
	return _send_fixed_message(CM_CREATECHAR, payload)


func delete_character(password: String) -> Error:
	return _send_fixed_message(CM_DELETECHAR, _make_static_buffer(password))


func enter_game() -> Error:
	return _send_empty_message(CM_ONLINE)


func send_ping(tick: int) -> Error:
	var payload := PackedByteArray()
	payload.resize(4)
	payload.encode_u32(0, tick)
	return _send_raw_message(CM_PING, payload)


func send_action(action_data: PackedByteArray) -> Error:
	return _send_fixed_message(CM_ACTION, action_data)


func send_player_say(content: String) -> Error:
	var payload := PackedByteArray()
	payload.resize(128)
	payload.fill(0)
	var encoded := content.to_utf8_buffer()
	for i in range(mini(encoded.size(), 127)):
		payload[i] = encoded[i]
	return _send_fixed_message(CM_PLAYERSAY, payload)


func send_player_broadcast(content: String) -> Error:
	var payload := PackedByteArray()
	payload.resize(128)
	payload.fill(0)
	var encoded := content.to_utf8_buffer()
	for i in range(mini(encoded.size(), 127)):
		payload[i] = encoded[i]
	return _send_fixed_message(CM_PLAYERBROADCAST, payload)


func send_pickup(x: int, y: int, map_uid: int) -> Error:
	var payload := PackedByteArray()
	payload.resize(18)
	payload.encode_u16(0, x)
	payload.encode_u16(2, y)
	payload.encode_u64(4, map_uid & 0xFFFFFFFF)
	payload.encode_u32(8, (map_uid >> 32) & 0xFFFFFFFF)
	payload.encode_u32(12, 0)
	payload.encode_u32(16, 0)
	return _send_fixed_message(CM_PICKUP, payload)


func send_query_player_name(uid: int) -> Error:
	var payload := PackedByteArray()
	payload.resize(8)
	_encode_u64(payload, 0, uid)
	return _send_fixed_message(CM_QUERYPLAYERNAME, payload)


func send_query_player_wldesp(uid: int) -> Error:
	var payload := PackedByteArray()
	payload.resize(8)
	_encode_u64(payload, 0, uid)
	return _send_fixed_message(CM_QUERYPLAYERWLDESP, payload)


func send_query_corecord(uid: int) -> Error:
	var payload := PackedByteArray()
	payload.resize(8)
	_encode_u64(payload, 0, uid)
	return _send_fixed_message(CM_QUERYCORECORD, payload)


func _make_account_payload(account: String, password: String) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.append_array(_make_static_buffer(account))
	payload.append_array(_make_static_buffer(password))
	return payload


func _make_static_buffer(value: String) -> PackedByteArray:
	var encoded := value.to_utf8_buffer()
	if encoded.size() > 64:
		encoded = encoded.slice(0, 64)
	var result := PackedByteArray()
	result.resize(STATIC_ID_SIZE)
	result.fill(0)
	result[0] = encoded.size() & 0xff
	result[1] = (encoded.size() >> 8) & 0xff
	for index in encoded.size():
		result[index + 2] = encoded[index]
	return result


func _send_fixed_message(head_code: int, payload: PackedByteArray) -> Error:
	if not is_connected_to_server():
		var connect_error := connect_to_server()
		if connect_error != OK:
			return connect_error
		return ERR_BUSY
	var compressed := _xor_encode(payload)
	var packet := PackedByteArray([head_code])
	packet.append_array(_encode_vlq(compressed[0]))
	packet.append_array(compressed[1])
	return _peer.put_data(packet)


func _send_raw_message(head_code: int, payload: PackedByteArray) -> Error:
	if not is_connected_to_server():
		return ERR_UNCONFIGURED
	var packet := PackedByteArray([head_code])
	packet.append_array(payload)
	return _peer.put_data(packet)


func _send_empty_message(head_code: int) -> Error:
	if not is_connected_to_server():
		return ERR_UNCONFIGURED
	return _peer.put_data(PackedByteArray([head_code]))


func _xor_encode(payload: PackedByteArray) -> Array:
	var mask_size := (payload.size() + 7) / 8
	var mask := PackedByteArray()
	mask.resize(mask_size)
	mask.fill(0)
	var body := PackedByteArray()
	for index in payload.size():
		if payload[index] != 0:
			mask[index / 8] |= 1 << (index % 8)
			body.append(payload[index])
	mask.append_array(body)
	return [body.size(), mask]


func _encode_vlq(value: int) -> PackedByteArray:
	var result := PackedByteArray()
	while true:
		var bits := value & 0x7f
		value >>= 7
		result.append(bits | 0x80 if value > 0 else bits)
		if value == 0:
			return result
	return result


func _encode_u64(buf: PackedByteArray, offset: int, value: int) -> void:
	buf.encode_u32(offset, value & 0xFFFFFFFF)
	buf.encode_u32(offset + 4, (value >> 32) & 0xFFFFFFFF)


func _decode_u64(buf: PackedByteArray, offset: int) -> int:
	return buf.decode_u32(offset) | (buf.decode_u32(offset + 4) << 32)


func _parse_packets() -> void:
	while not _receive_buffer.is_empty():
		var parsed := _try_parse_packet()
		if not parsed[0]:
			return
		var consumed: int = parsed[1]
		message_received.emit(parsed[2], parsed[3])
		_receive_buffer = _receive_buffer.slice(consumed)


func _try_parse_packet() -> Array:
	var cursor := 0
	var encoded_head := _receive_buffer[cursor]
	cursor += 1
	var head_code := encoded_head & 0x7f
	if encoded_head & 0x80:
		var response_result := _try_decode_vlq(cursor)
		if not response_result[0]:
			return [false]
		cursor = response_result[2]
	var attribute := _server_message_attribute(head_code)
	if attribute.is_empty():
		push_error("暂不支持服务器消息: %d" % head_code)
		_receive_buffer.clear()
		return [false]
	var message_type: int = attribute[0]
	var data_length: int = attribute[1]
	if message_type == 0:
		return [true, cursor, head_code, PackedByteArray()]
	if message_type == 2:
		if _receive_buffer.size() < cursor + data_length:
			return [false]
		return [true, cursor + data_length, head_code, _receive_buffer.slice(cursor, cursor + data_length)]
	var size_result := _try_decode_vlq(cursor)
	if not size_result[0]:
		return [false]
	var body_size: int = size_result[1]
	cursor = size_result[2]
	if message_type == 3:
		if _receive_buffer.size() < cursor + body_size:
			return [false]
		return [true, cursor + body_size, head_code, _receive_buffer.slice(cursor, cursor + body_size)]
	var mask_size := (data_length + 7) / 8
	if _receive_buffer.size() < cursor + mask_size + body_size:
		return [false]
	var mask := _receive_buffer.slice(cursor, cursor + mask_size)
	cursor += mask_size
	var body := _receive_buffer.slice(cursor, cursor + body_size)
	return [true, cursor + body_size, head_code, _xor_decode(data_length, mask, body)]


func _try_decode_vlq(offset: int) -> Array:
	var value := 0
	var shift := 0
	var cursor := offset
	while cursor < _receive_buffer.size():
		var byte := _receive_buffer[cursor]
		value |= (byte & 0x7f) << shift
		cursor += 1
		if not byte & 0x80:
			return [true, value, cursor]
		shift += 7
		if shift >= 64:
			push_error("服务器发送了无效的变长整数")
			return [false]
	return [false]


func _xor_decode(data_length: int, mask: PackedByteArray, body: PackedByteArray) -> PackedByteArray:
	var result := PackedByteArray()
	result.resize(data_length)
	result.fill(0)
	var body_index := 0
	for index in data_length:
		if mask[index / 8] & (1 << (index % 8)):
			if body_index >= body.size():
				break
			result[index] = body[body_index]
			body_index += 1
	return result


# Server message attribute table: [type, dataLen]
# type 0: empty, type 1: fixed+compressed, type 2: fixed+raw, type 3: variable
func _server_message_attribute(head_code: int) -> Array:
	match head_code:
		SM_OK:                   return [3, 0]
		SM_ERROR:                return [3, 0]
		SM_PING:                 return [2, 4]
		SM_UID:                  return [2, 8]
		SM_LOGINOK:              return [0, 0]
		SM_LOGINERROR:           return [1, 1]
		SM_CREATEACCOUNTOK:      return [0, 0]
		SM_CREATEACCOUNTERROR:   return [1, 4]
		SM_CHANGEPASSWORDOK:     return [0, 0]
		SM_CHANGEPASSWORDERROR:  return [1, 4]
		SM_QUERYCHAROK:          return [1, 74]
		SM_QUERYCHARERROR:       return [1, 1]
		SM_CREATECHAROK:         return [0, 0]
		SM_CREATECHARERROR:      return [1, 1]
		SM_CREATECHATGROUP:      return [3, 0]
		SM_ADDFRIENDACCEPTED:    return [3, 0]
		SM_ADDFRIENDREJECTED:    return [3, 0]
		SM_DELETECHAROK:         return [0, 0]
		SM_DELETECHARERROR:      return [1, 1]
		SM_ONLINEOK:             return [1, 176]
		SM_ONLINEERROR:          return [1, 1]
		SM_STARTGAMESCENE:       return [3, 0]
		SM_PLAYERCONFIG:         return [3, 0]
		SM_FRIENDLIST:           return [3, 0]
		SM_LEARNEDMAGICLIST:     return [3, 0]
		SM_PLAYERWLDESP:         return [3, 0]
		SM_ACTION:               return [1, 44]
		SM_COREORD:              return [1, 52]
		SM_HEALTH:               return [3, 0]
		SM_NEXTSTRIKE:           return [0, 0]
		SM_NOTIFYDEAD:           return [1, 8]
		SM_DEADFADEOUT:          return [1, 24]
		SM_EXP:                  return [1, 4]
		SM_BUFF:                 return [1, 16]
		SM_BUFFIDLIST:           return [3, 0]
		SM_MISS:                 return [1, 8]
		SM_CASTMAGIC:            return [1, 28]
		SM_OFFLINE:              return [1, 16]
		SM_PICKUPERROR:          return [1, 4]
		SM_REMOVEGROUNDITEM:     return [1, 12]
		SM_NPCXMLLAYOUT:         return [3, 0]
		SM_NPCSELL:              return [3, 0]
		SM_STARTINVOP:           return [3, 0]
		SM_STARTINPUT:           return [3, 0]
		SM_GOLD:                 return [1, 4]
		SM_INVOPCOST:            return [1, 16]
		SM_STRIKEGRID:           return [1, 8]
		SM_SELLITEMLIST:         return [3, 0]
		SM_TEXT:                 return [3, 0]
		SM_PLAYERNAME:           return [3, 0]
		SM_BUILDVERSION:         return [1, 130]
		SM_INVENTORY:            return [3, 0]
		SM_BELT:                 return [3, 0]
		SM_UPDATEITEM:           return [3, 0]
		SM_REMOVEITEM:           return [1, 10]
		SM_REMOVESECUREDITEM:    return [1, 8]
		SM_BUYSUCCEED:           return [1, 20]
		SM_BUYERROR:             return [1, 22]
		SM_GROUNDITEMIDLIST:     return [3, 0]
		SM_GROUNDFIREWALLLIST:   return [3, 0]
		SM_EQUIPWEAR:            return [3, 0]
		SM_EQUIPWEARERROR:       return [1, 10]
		SM_GRABWEAR:             return [3, 0]
		SM_GRABWEARERROR:        return [1, 2]
		SM_EQUIPBELT:            return [3, 0]
		SM_EQUIPBELTERROR:       return [1, 10]
		SM_GRABBELT:             return [3, 0]
		SM_GRABBELTERROR:        return [1, 2]
		SM_SHOWSECUREDITEMLIST:  return [3, 0]
		SM_TEAMMEMBERLIST:       return [3, 0]
		SM_TEAMCANDIDATE:        return [3, 0]
		SM_TEAMERROR:            return [1, 1]
		SM_QUESTDESPUPDATE:      return [3, 0]
		SM_QUESTDESPLIST:        return [3, 0]
		SM_CHATMESSAGELIST:      return [3, 0]
		SM_PLAYERSAY:            return [1, 136]
		SM_PLAYERBROADCAST:      return [1, 136]
		_: return []
