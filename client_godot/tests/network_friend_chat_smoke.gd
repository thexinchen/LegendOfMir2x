extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")

var _online := false
var _friend_list_seen := false
var _query_done := false
var _message_sent := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(12.0).timeout.connect(func(): _fail("timed out", 6))


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 3)
		NetworkClient.SM_ONLINEOK:
			_online = true
			var online := Protocol.decode_sm_online_ok(payload)
			NetworkClient.query_chat_peers(str(online.get("name", "")), _on_query_response)
		NetworkClient.SM_FRIENDLIST:
			var reader := CerealReader.new(payload)
			reader.read_sd_chat_peer_list()
			if not reader.valid or not reader.at_end():
				_fail("invalid friend list: %s" % reader.error, 4)
				return
			_friend_list_seen = true
			_finish_if_ready()
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login/online", 5)


func _on_query_response(head_code: int, payload: PackedByteArray) -> void:
	if head_code != NetworkClient.SM_OK:
		_fail("peer query rejected", 7)
		return
	var reader := CerealReader.new(payload)
	var peers := reader.read_sd_chat_peer_list()
	if not reader.valid or not reader.at_end() or peers.is_empty():
		_fail("peer query response invalid: %s" % reader.error, 8)
		return
	_query_done = true
	var self_peer: Dictionary = peers[0]
	NetworkClient.send_chat_message(int(self_peer.get("cpid", 0)), "Godot好友聊天协议测试", null, _on_send_response)


func _on_send_response(head_code: int, payload: PackedByteArray) -> void:
	if head_code != NetworkClient.SM_OK:
		_fail("chat send rejected", 9)
		return
	var reader := CerealReader.new(payload)
	var seq := reader.read_sd_chat_message_db_seq()
	if not reader.valid or not reader.at_end() or int(seq.get("id", 0)) == 0:
		_fail("chat send sequence invalid: %s" % reader.error, 10)
		return
	_message_sent = true
	_finish_if_ready()


func _finish_if_ready() -> void:
	if not _online or not _friend_list_seen or not _query_done or not _message_sent:
		return
	print("NETWORK FRIEND CHAT PASS")
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _fail(message: String, code: int) -> void:
	push_error("NETWORK_FRIEND_CHAT_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
