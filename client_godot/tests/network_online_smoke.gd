extends Node

var _exp_request_sent := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(8.0).timeout.connect(_on_timeout)


func _on_connection_changed(connected: bool, message: String) -> void:
	print("NETWORK_ONLINE_SMOKE connection: ", message)
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to send enter game", 3)
		NetworkClient.SM_LOGINERROR:
			_fail("login failed: %d" % (payload[0] if not payload.is_empty() else -1), 4)
		NetworkClient.SM_ONLINEOK:
			print("NETWORK_ONLINE_SMOKE enter game succeeded")
			if NetworkClient.send_request_add_exp(1) != OK:
				_fail("failed to send original @addExp protocol", 7)
				return
			_exp_request_sent = true
		NetworkClient.SM_EXP:
			if _exp_request_sent:
				print("NETWORK_ONLINE_SMOKE original @addExp protocol round-trip succeeded: exp=", payload.decode_u32(0))
				get_tree().quit()
		NetworkClient.SM_ONLINEERROR:
			_fail("enter game failed: %d" % (payload[0] if not payload.is_empty() else -1), 5)


func _on_timeout() -> void:
	_fail("timed out", 6)


func _fail(message: String, exit_code: int) -> void:
	push_error("NETWORK_ONLINE_SMOKE %s" % message)
	get_tree().quit(exit_code)
