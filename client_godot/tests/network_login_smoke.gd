extends Node

const TEST_ACCOUNT := "codex-smoke-invalid@example.com"
const TEST_PASSWORD := "Invalid1!"

enum Step {
	LOGIN,
	QUERY_CHARACTER,
	CREATE_CHARACTER,
	DELETE_CHARACTER,
	CREATE_ACCOUNT,
	CHANGE_PASSWORD,
}

var step := Step.LOGIN


func _ready() -> void:
	assert(NetworkClient._make_static_buffer("x").size() == 68)
	assert(NetworkClient._make_account_payload("x", "y").size() == 136)
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(5.0).timeout.connect(_on_timeout)


func _on_connection_changed(connected: bool, message: String) -> void:
	print("NETWORK_SMOKE connection: ", message)
	if connected:
		var error := OK
		match step:
			Step.LOGIN:
				error = NetworkClient.login(TEST_ACCOUNT, TEST_PASSWORD)
			Step.CREATE_ACCOUNT:
				error = NetworkClient.create_account("invalid-account", "bad")
			Step.CHANGE_PASSWORD:
				error = NetworkClient.change_password("invalid-account", "bad", "bad-new")
		if error != OK:
			push_error("NETWORK_SMOKE failed to send step %d: %d" % [step, error])
			get_tree().quit(2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	var error_code := payload[0] if not payload.is_empty() else 0
	match step:
		Step.LOGIN:
			if head_code == NetworkClient.SM_LOGINERROR:
				print("NETWORK_SMOKE login response received, error=", error_code)
				step = Step.QUERY_CHARACTER
				NetworkClient.query_character()
		Step.QUERY_CHARACTER:
			if head_code == NetworkClient.SM_QUERYCHARERROR:
				print("NETWORK_SMOKE query-character response received, error=", error_code)
				step = Step.CREATE_CHARACTER
				NetworkClient.create_character("smoke", 1, true)
		Step.CREATE_CHARACTER:
			if head_code == NetworkClient.SM_CREATECHARERROR:
				print("NETWORK_SMOKE create-character response received, error=", error_code)
				step = Step.DELETE_CHARACTER
				NetworkClient.delete_character(TEST_PASSWORD)
		Step.DELETE_CHARACTER:
			if head_code == NetworkClient.SM_DELETECHARERROR:
				print("NETWORK_SMOKE delete-character response received, error=", error_code)
				step = Step.CREATE_ACCOUNT
				_reconnect()
		Step.CREATE_ACCOUNT:
			if head_code == NetworkClient.SM_CREATEACCOUNTERROR:
				print("NETWORK_SMOKE create-account response received, error=", payload.decode_u32(0))
				step = Step.CHANGE_PASSWORD
				_reconnect()
		Step.CHANGE_PASSWORD:
			if head_code == NetworkClient.SM_CHANGEPASSWORDERROR:
				print("NETWORK_SMOKE change-password response received, error=", payload.decode_u32(0))
				print("NETWORK_SMOKE all account-flow packets passed")
				get_tree().quit(0)


func _on_timeout() -> void:
	push_error("NETWORK_SMOKE timed out")
	get_tree().quit(3)


func _reconnect() -> void:
	NetworkClient.disconnect_from_server()
	await get_tree().create_timer(0.1).timeout
	NetworkClient.connect_to_server()
