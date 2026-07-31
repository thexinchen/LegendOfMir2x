extends Node

signal connection_changed(connected: bool, message: String)
signal message_received(head_code: int, payload: PackedByteArray)

const CM_LOGIN := 2
const CM_QUERYCHAR := 3
const CM_CREATECHAR := 4
const CM_DELETECHAR := 5
const CM_CHANGEPASSWORD := 6
const CM_ONLINE := 7
const CM_CREATEACCOUNT := 28

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
const SM_DELETECHAROK := 18
const SM_DELETECHARERROR := 19
const SM_ONLINEOK := 20
const SM_ONLINEERROR := 21

# StaticBuffer<64> stores uint16 + uint8[65], then C++ adds one tail-padding
# byte to satisfy the struct's two-byte alignment.
const STATIC_ID_SIZE := 68

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


func _server_message_attribute(head_code: int) -> Array:
	match head_code:
		4: return [2, 8]
		5: return [0, 0]
		6: return [1, 1]
		7: return [0, 0]
		8: return [1, 4]
		9: return [0, 0]
		10: return [1, 4]
		11: return [1, 74]
		12: return [1, 1]
		13: return [0, 0]
		14: return [1, 1]
		18: return [0, 0]
		19: return [1, 1]
		20: return [1, 176]
		21: return [1, 1]
		51: return [1, 132]
		_: return []
