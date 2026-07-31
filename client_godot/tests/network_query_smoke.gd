extends Node


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(5.0).timeout.connect(_on_timeout)


func _on_connection_changed(connected: bool, message: String) -> void:
	print("NETWORK_QUERY_SMOKE connection: ", message)
	if connected:
		var error := NetworkClient.login("test", "123456")
		if error != OK:
			push_error("NETWORK_QUERY_SMOKE failed to send login: %d" % error)
			get_tree().quit(2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_LOGINOK:
			print("NETWORK_QUERY_SMOKE login succeeded")
			NetworkClient.query_character()
		NetworkClient.SM_LOGINERROR:
			push_error("NETWORK_QUERY_SMOKE login failed: %d" % payload[0])
			get_tree().quit(3)
		NetworkClient.SM_QUERYCHAROK:
			var name_size := mini(payload.decode_u16(0), 64)
			var character_name := payload.slice(2, 2 + name_size).get_string_from_utf8()
			print(
				"NETWORK_QUERY_SMOKE character received: ",
				character_name,
				", gender=",
				payload[68],
				", job=",
				payload[69],
				", exp=",
				payload.decode_u32(70),
			)
			get_tree().quit(0)
		NetworkClient.SM_QUERYCHARERROR:
			if not payload.is_empty() and payload[0] == 2:
				print("NETWORK_QUERY_SMOKE login succeeded; account has no character")
				get_tree().quit(0)
			else:
				push_error("NETWORK_QUERY_SMOKE query failed")
				get_tree().quit(4)


func _on_timeout() -> void:
	push_error("NETWORK_QUERY_SMOKE timed out")
	get_tree().quit(5)
