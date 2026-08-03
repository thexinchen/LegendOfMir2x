extends Node

const SIGNATURE_PATH := "res://assets/generated/build_signature.txt"
const EXPORT_PRESET_PATH := "res://export_presets.cfg"

var _received_heads: Array[int] = []
var _connection_messages: Array[String] = []


func _ready() -> void:
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connection_changed.connect(_on_connection_changed)

	if FileAccess.file_exists(EXPORT_PRESET_PATH):
		var export_preset := FileAccess.get_file_as_string(EXPORT_PRESET_PATH)
		if SIGNATURE_PATH.trim_prefix("res://") not in export_preset:
			_fail("export preset does not include the raw build signature")
			return
	if not FileAccess.file_exists(SIGNATURE_PATH):
		_fail("build signature is unavailable at runtime")
		return
	var local_signature := FileAccess.get_file_as_string(SIGNATURE_PATH).strip_edges()
	if local_signature.is_empty():
		_fail("build signature is empty")
		return

	var wide_payload := PackedByteArray()
	wide_payload.resize(410)
	wide_payload.fill(0)
	wide_payload[0] = 1
	wide_payload[9] = 2
	wide_payload[409] = 3
	var wide_encoded: Array = NetworkClient._xor_encode(wide_payload)
	var wide_data: PackedByteArray = wide_encoded[1]
	if int(wide_encoded[0]) != 18 or wide_data.size() != 25 \
			or wide_data[0] != 0x03 or wide_data[6] != 0x08 \
			or wide_data[7] != 1 or wide_data[16] != 2 or wide_data[24] != 3:
		_fail("wide fixed-message XOR encoding does not match the C++ 8-byte chunk protocol: body=%s data=%s" % [wide_encoded[0], wide_data])
		return
	var wide_decoded := NetworkClient._xor_decode(410, wide_data.slice(0, 7), wide_data.slice(7))
	if wide_decoded != wide_payload:
		_fail("wide fixed-message XOR decoding does not restore the original payload")
		return

	_feed_build_version(local_signature)
	if not _received_heads.is_empty():
		_fail("matching build version leaked to scene message handlers")
		return
	if not _connection_messages.is_empty():
		_fail("matching build version changed connection state")
		return

	_feed_build_version(local_signature + "-mismatch")
	if not _received_heads.is_empty():
		_fail("mismatching build version leaked to scene message handlers")
		return
	if _connection_messages.size() != 1 or not _connection_messages[0].contains("版本不一致"):
		_fail("mismatching build version did not report a clear rejection")
		return

	_connection_messages.clear()
	var malformed := NetworkClient._make_static_buffer_capacity(local_signature, 128)
	malformed.encode_u16(0, 129)
	_feed_payload(malformed)
	if _connection_messages.size() != 1 or not _connection_messages[0].contains("无效"):
		_fail("malformed build version was not rejected")
		return

	print("BUILD_VERSION_SMOKE passed")
	get_tree().quit(0)


func _feed_build_version(signature: String) -> void:
	_feed_payload(NetworkClient._make_static_buffer_capacity(signature, 128))


func _feed_payload(payload: PackedByteArray) -> void:
	var encoded: Array = NetworkClient._xor_encode(payload)
	var packet := PackedByteArray([NetworkClient.SM_BUILDVERSION])
	packet.append_array(NetworkClient._encode_vlq(int(encoded[0])))
	packet.append_array(encoded[1])
	NetworkClient._receive_buffer = packet
	NetworkClient._parse_packets()


func _on_message_received(head_code: int, _payload: PackedByteArray) -> void:
	_received_heads.append(head_code)


func _on_connection_changed(connected: bool, message: String) -> void:
	if not connected:
		_connection_messages.append(message)


func _fail(message: String) -> void:
	push_error("BUILD_VERSION_SMOKE: " + message)
	get_tree().quit(1)
