extends Control


func _ready() -> void:
	GameState.player_uid = (5 << 59) | (42 << 35)
	GameState.player_name = "亚当"
	GameState.player_gender = 1
	GameState.player_job = 0
	var friend := {"id": 99, "cpid": (2 << 32) | 99, "type": 2, "name": "清风", "gender": false, "job": 1}
	var group := {"id": 77, "cpid": (3 << 32) | 77, "type": 3, "name": "比奇远征队", "members": [{"dbid": 42}, {"dbid": 99}]}
	GameState.set_chat_friends([friend, group])
	GameState.add_chat_message(_message(1001, 100, friend.cpid, GameState.self_chat_cpid(), "晚上一起去矿洞吗？"))
	GameState.add_chat_message(_message(1002, 110, GameState.self_chat_cpid(), friend.cpid, "好，城门口集合。"))
	var panel: Control = load("res://scenes/game/panels/friend_chat.tscn").instantiate()
	add_child(panel)
	panel.position = Vector2(174, 68)
	panel.call("_open_chat", friend.cpid)
	await get_tree().process_frame
	await get_tree().process_frame
	if OS.has_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"):
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"))
	print("FRIEND CHAT VISUAL PASS")
	get_tree().quit()


func _message(id: int, timestamp: int, from: int, to: int, text: String) -> Dictionary:
	return {
		"seq": {"id": id, "timestamp": timestamp},
		"refer": null,
		"from": from,
		"to": to,
		"message": NetworkClient._serialize_cereal_string("<layout><par>%s</par></layout>" % text),
	}
