extends Control

const Protocol = preload("res://scripts/network/protocol.gd")


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.player_direction = 5
	GameState.player_gender = 0
	GameState.player_desp = {"wear": {}}
	GameState.view_x = float(371 * 48 - 400)
	GameState.view_y = float(132 * 32 - 234)
	GameState.creatures = {
		303: {
			"uid": 303, "type": 1, "monster_id": 224, "name": "TARGET",
			"x": 372, "y": 132, "direction": 5, "action_type": 2,
			"action_speed": 100, "action_started_ms": Time.get_ticks_msec(),
		},
	}
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	if not main.get_node("WorldRenderer").call("load_map", 24):
		_fail("unable to load visual map")
		return
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 202, {
		"type": 12, "speed": 100, "x": 371, "y": 132, "aimUID": 303,
	}))
	GameState.player_action_started_ms = Time.get_ticks_msec() - 400
	if GameState.player_direction != 7:
		_fail("spin-kick direction was not derived from the adjacent target")
		return
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_SPINKICK_SCREENSHOT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-spinkick.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("SPINKICK VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _sm_action(uid: int, map_uid: int, action: Dictionary) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(43)
	payload.encode_u64(0, uid)
	payload.encode_u64(8, map_uid)
	var encoded_action := Protocol.encode_action_node(action)
	for index in encoded_action.size():
		payload[16 + index] = encoded_action[index]
	return payload


func _fail(message: String) -> void:
	push_error("SPINKICK_VISUAL %s" % message)
	get_tree().quit(1)
