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
	await get_tree().process_frame
	var preview_rows := panel.get_node("Page/ListScroll/Rows")
	var first_row := preview_rows.get_child(0) as Button
	if first_row == null or first_row.size.y != 58 or first_row.get_child_count() < 3 or not first_row.get_child(0) is TextureRect:
		_fail("preview row avatar/style geometry mismatch")
		return
	if panel.get_node("ContentFrame").position != Vector2.ZERO or panel.get_node("ContentFrame").size != panel.size:
		_fail("full-board foreground frame mismatch")
		return
	if OS.has_environment("MIR2X_FRIEND_LIST_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_LIST_SCREENSHOT"))
	for index in range(8):
		var extra := {"id": 200 + index, "cpid": (2 << 32) | (200 + index), "type": 2, "name": "好友%d" % index, "gender": false, "job": 1}
		GameState.add_chat_peer(extra, true, "列表滚动测试")
	await get_tree().process_frame
	var list_bar: VScrollBar = panel.get_node("Page/ListScroll").get_v_scroll_bar()
	if list_bar.max_value - list_bar.page <= 0:
		_fail("friend list scroll reach missing")
		return
	var slider_event := InputEventMouseButton.new()
	slider_event.button_index = MOUSE_BUTTON_LEFT
	slider_event.pressed = true
	slider_event.position = Vector2(10, panel.size.y - 116)
	panel.call("_on_slider_input", slider_event)
	if list_bar.value < list_bar.max_value - list_bar.page - 1:
		_fail("native slider did not reach list bottom")
		return
	GameState.chat_conversations = GameState.chat_conversations.filter(func(value): return int(value.get("cpid", 0)) == friend.cpid)
	GameState.state_changed.emit()
	panel.call("_show_page", 3)
	panel.get_node("Page/SearchPage/Query").text = "清"
	panel.set("_search_results", [friend])
	panel.set("_search_show_candidates", false)
	panel.call("_render_search_results")
	await get_tree().process_frame
	if panel.get_node("Page/SearchPage/Results").get_child(0).size.y != 30:
		_fail("search suggestion row mismatch")
		return
	panel.call("_show_search_candidates", "清")
	await get_tree().process_frame
	if panel.get_node("Page/SearchPage/Results").get_child(0).size.y != 52:
		_fail("search candidate row mismatch")
		return
	panel.set("_resize_edge", 7)
	panel.call("_apply_resize", Vector2(40, 30))
	if panel.size != Vector2(491, 494):
		_fail("bottom-right resize mismatch: %s" % panel.size)
		return
	panel.size = Vector2(451, 464)
	var stranger := {"id": 150, "cpid": (2 << 32) | 150, "type": 2, "name": "陌生人", "gender": true, "job": 1}
	GameState.add_chat_peer(stranger)
	panel.call("_open_chat", stranger.cpid)
	await get_tree().process_frame
	if panel.get_node("Page/ChatPage/Messages/MessageRows").get_child_count() != 1:
		_fail("stranger operation strip missing")
		return
	panel.call("_open_chat", friend.cpid)
	await get_tree().process_frame
	await get_tree().process_frame
	panel.set("_pending_messages", {1: {"to": friend.cpid, "text": "发送中的消息", "refer": null}})
	panel.call("_refresh")
	await get_tree().process_frame
	if panel.get_node("Page/ChatPage/Messages/MessageRows").get_child_count() != 3:
		_fail("pending message bubble missing")
		return
	var accepted := {"id": 101, "cpid": (2 << 32) | 101, "type": 2, "name": "新朋友", "gender": true, "job": 2}
	GameState.add_chat_peer(accepted, true, "新朋友已经通过你的好友申请，现在可以开始聊天了。")
	if GameState.chat_conversations[0].cpid != accepted.cpid or GameState.chat_conversations[0].preview.is_empty():
		_fail("accepted-friend preview missing")
		return
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	main.call("_apply_chat_messages", [_message(1003, 120, friend.cpid, GameState.self_chat_cpid(), "闪烁测试")])
	if not main.get_node("ControlPanel").get("_button_blinks").has("Friend"):
		_fail("incoming friend message did not start HUD blink")
		return
	main.hide()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"):
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"))
	print("FRIEND CHAT VISUAL PASS: frames, rows, pending, preview, resize and HUD blink")
	get_tree().quit()


func _message(id: int, timestamp: int, from: int, to: int, text: String) -> Dictionary:
	return {
		"seq": {"id": id, "timestamp": timestamp},
		"refer": null,
		"from": from,
		"to": to,
		"message": NetworkClient._serialize_cereal_string("<layout><par>%s</par></layout>" % text),
	}


func _fail(message: String) -> void:
	push_error("FRIEND_CHAT_VISUAL_SMOKE %s" % message)
	get_tree().quit(1)
