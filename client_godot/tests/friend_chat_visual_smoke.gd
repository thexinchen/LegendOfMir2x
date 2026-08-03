extends Control


func _ready() -> void:
	GameState.player_uid = (5 << 59) | 42
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
	if (first_row.get_child(0) as TextureRect).stretch_mode != TextureRect.STRETCH_SCALE:
		_fail("preview avatar did not stretch the complete texture like the original client")
		return
	var row_rgb := Color(231.0 / 255.0, 231.0 / 255.0, 189.0 / 255.0)
	var row_normal := first_row.get_theme_stylebox("normal") as StyleBoxFlat
	var row_hover := first_row.get_theme_stylebox("hover") as StyleBoxFlat
	var row_pressed := first_row.get_theme_stylebox("pressed") as StyleBoxFlat
	var subtitle := first_row.get_child(2) as Label
	if row_normal.bg_color != Color.TRANSPARENT or row_normal.border_color != Color(row_rgb.r, row_rgb.g, row_rgb.b, 32.0 / 255.0):
		_fail("friend row normal state diverged from C++: bg=%s border=%s" % [row_normal.bg_color, row_normal.border_color])
		return
	var hovered_color := Color(row_rgb.r, row_rgb.g, row_rgb.b, 64.0 / 255.0)
	if row_hover.bg_color != hovered_color or row_hover.border_color != hovered_color or row_pressed.bg_color != hovered_color or row_pressed.border_color != hovered_color:
		_fail("friend row hover/pressed states diverged from C++: hover=%s/%s pressed=%s/%s" % [row_hover.bg_color, row_hover.border_color, row_pressed.bg_color, row_pressed.border_color])
		return
	if subtitle.get_theme_color("font_color") != Color(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0):
		_fail("friend preview subtitle color diverged from C++: %s" % subtitle.get_theme_color("font_color"))
		return
	if panel.get_node("ContentFrame").position != Vector2.ZERO or panel.get_node("ContentFrame").size != panel.size:
		_fail("full-board foreground frame mismatch")
		return
	var composer := panel.get_node("Page/ChatPage/Composer") as Control
	var chat_input := panel.get_node("Page/ChatPage/Composer/Input") as TextEdit
	var reference_bar := panel.get_node_or_null("Page/ChatPage/Composer/ReferenceBar") as Panel
	var reference_clear := panel.get_node_or_null("Page/ChatPage/Composer/ReferenceBar/Row/Clear") as Button
	if composer.custom_minimum_size.y != 74.0 or chat_input.placeholder_text != "" or panel.get_node_or_null("Page/ChatPage/Composer/Send") != null or reference_bar == null or reference_clear == null:
		_fail("chat composer does not match the original fixed 74px input and clearable reference bar")
		return
	var input_normal := chat_input.get_theme_stylebox("normal") as StyleBoxFlat
	var input_focus := chat_input.get_theme_stylebox("focus") as StyleBoxFlat
	var expected_input_bg := Color(0, 0, 0, 80.0 / 255.0)
	if input_normal == null or input_focus == null or not input_normal.bg_color.is_equal_approx(expected_input_bg) or not input_focus.bg_color.is_equal_approx(expected_input_bg):
		_fail("chat input normal/focus backgrounds diverged from C++ FrameBg: %s/%s" % [input_normal.bg_color if input_normal != null else Color.TRANSPARENT, input_focus.bg_color if input_focus != null else Color.TRANSPARENT])
		return
	if input_normal.border_width_left != 0 or input_focus.border_width_left != 0 or input_normal.content_margin_left != 4.0 or input_normal.content_margin_top != 3.0:
		_fail("chat input border or FramePadding diverged from C++")
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
	var search_results := panel.get_node("Page/SearchPage/SearchScroll/Results")
	if search_results.get_child(0).size.y != 30:
		_fail("search suggestion row mismatch")
		return
	var suggestion := search_results.get_child(0) as Button
	var match_label := suggestion.get_node_or_null("MatchText/Match") as Label
	var suggestion_normal := suggestion.get_theme_stylebox("normal") as StyleBoxFlat
	var suggestion_hover := suggestion.get_theme_stylebox("hover") as StyleBoxFlat
	var suggestion_pressed := suggestion.get_theme_stylebox("pressed") as StyleBoxFlat
	var search_normal_color := Color(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0, 64.0 / 255.0)
	var search_hover_color := Color(row_rgb.r, row_rgb.g, row_rgb.b, 64.0 / 255.0)
	if not suggestion.text.is_empty() or match_label == null or match_label.text != "清" or match_label.get_theme_color("font_color") != Color.RED:
		_fail("search suggestion did not split and highlight the matched text like C++")
		return
	if suggestion_normal.bg_color != search_normal_color or suggestion_normal.border_color != Color(row_rgb.r, row_rgb.g, row_rgb.b, 32.0 / 255.0):
		_fail("search suggestion normal state diverged from C++: %s/%s" % [suggestion_normal.bg_color, suggestion_normal.border_color])
		return
	if suggestion_hover.bg_color != search_hover_color or suggestion_hover.border_color != search_hover_color or suggestion_pressed.bg_color != search_hover_color or suggestion_pressed.border_color != search_hover_color:
		_fail("search suggestion hover/pressed states diverged from C++")
		return
	if OS.has_environment("MIR2X_FRIEND_SUGGESTION_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_SUGGESTION_SCREENSHOT"))
	panel.get_node("Page/SearchPage/Query").text = "99"
	panel.set("_search_show_candidates", false)
	panel.call("_render_search_results")
	await get_tree().process_frame
	var id_suggestion := search_results.get_child(0) as Button
	var id_prefix := id_suggestion.get_node_or_null("MatchText/Prefix") as Label
	var id_match := id_suggestion.get_node_or_null("MatchText/Match") as Label
	var id_suffix := id_suggestion.get_node_or_null("MatchText/Suffix") as Label
	if id_prefix == null or id_match == null or id_suffix == null or id_prefix.text != "清风（" or id_match.text != "99" or id_suffix.text != "）" or id_match.get_theme_color("font_color") != Color.RED:
		_fail("numeric friend search did not highlight the exact ID like C++")
		return
	id_suggestion.emit_signal("pressed")
	await get_tree().process_frame
	if panel.get_node("Page/SearchPage/Query").text != "99" or not panel.get("_search_show_candidates") or search_results.get_child(0).size.y != 52:
		_fail("numeric friend suggestion did not switch to the candidate view")
		return
	panel.get_node("Page/SearchPage/Query").text = "清"
	panel.set("_search_show_candidates", false)
	panel.call("_render_search_results")
	await get_tree().process_frame
	panel.call("_show_search_candidates", "清")
	await get_tree().process_frame
	if search_results.get_child(0).size.y != 52:
		_fail("search candidate row mismatch")
		return
	var first_candidate := search_results.get_child(0) as Button
	if first_candidate.get_node_or_null("Add") == null or first_candidate.get_node("Add").text != "添加" or first_candidate.get_child(1).position.y != 10:
		_fail("search candidate did not use the original independent Add control")
		return
	var candidate_normal := first_candidate.get_theme_stylebox("normal") as StyleBoxFlat
	var candidate_hover := first_candidate.get_theme_stylebox("hover") as StyleBoxFlat
	var candidate_pressed := first_candidate.get_theme_stylebox("pressed") as StyleBoxFlat
	if candidate_normal.bg_color != search_normal_color or candidate_normal.border_color != Color(row_rgb.r, row_rgb.g, row_rgb.b, 32.0 / 255.0):
		_fail("search candidate normal state diverged from C++: %s/%s" % [candidate_normal.bg_color, candidate_normal.border_color])
		return
	if candidate_hover.bg_color != search_hover_color or candidate_hover.border_color != search_hover_color or candidate_pressed.bg_color != search_hover_color or candidate_pressed.border_color != search_hover_color:
		_fail("search candidate hover/pressed states diverged from C++")
		return
	if not first_candidate.get_child(0) is TextureRect or (first_candidate.get_child(0) as TextureRect).stretch_mode != TextureRect.STRETCH_SCALE:
		_fail("search candidate avatar did not stretch the complete texture like the original client")
		return
	var input_frame := panel.get_node("Page/SearchPage/InputFrame") as NinePatchRect
	if input_frame.size != Vector2(332, 30) or panel.get_node("Page/SearchPage/SearchIcon").position != Vector2(8, 5) or panel.get_node("Page/SearchPage/Clear").get_theme_font_size("font_size") != 15:
		_fail("native search frame, icon or clear-control geometry mismatch")
		return
	var many_results: Array = [friend]
	for index in range(8):
		many_results.append({"id": 300 + index, "cpid": (2 << 32) | (300 + index), "type": 2, "name": "清风%d" % index, "gender": bool(index % 2), "job": index % 3})
	panel.set("_search_results", many_results)
	panel.call("_render_search_results")
	await get_tree().process_frame
	var search_bar: VScrollBar = panel.get_node("Page/SearchPage/SearchScroll").get_v_scroll_bar()
	if search_bar.max_value - search_bar.page <= 0 or panel.call("_current_scrollbar") != search_bar:
		_fail("search results did not use the shared native-thumb scroll path")
		return
	if OS.has_environment("MIR2X_FRIEND_SEARCH_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_SEARCH_SCREENSHOT"))
	panel.get_node("Page/SearchPage/Clear").emit_signal("pressed")
	if not panel.get_node("Page/SearchPage/Query").text.is_empty() or search_results.get_child_count() != 0 or panel.get("_search_show_candidates"):
		_fail("search clear control did not reset input, candidates and results")
		return
	if not _test_friend_feedback(panel, friend):
		return
	panel.call("_open_group_page")
	await get_tree().process_frame
	await get_tree().process_frame
	if not panel.get_node("Page/ListScroll").visible or not panel.get_node("Toolbar/GroupConfirm").visible or not panel.get_node("Toolbar/Invert").visible:
		_fail("group toolbar/list choreography mismatch")
		return
	if panel.get_node("Toolbar").get_child(0).name != "Invert" or panel.get_node("Page/ListScroll").scroll_vertical != 0:
		_fail("group toolbar order or independent scroll mismatch")
		return
	panel.call("_toggle_group_member", friend.cpid)
	panel.call("_invert_group_selection")
	if panel.get("_selected_group").has(friend.cpid) or panel.get("_selected_group").size() != GameState.chat_friends.size() - 1:
		_fail("group selection/invert mismatch")
		return
	var group_request: Array = [[]]
	panel.group_name_requested.connect(func(ids: Array): group_request[0] = ids)
	panel.call("_request_group_name")
	if group_request[0].size() != panel.get("_selected_group").size():
		_fail("group-name request member list mismatch")
		return
	if OS.has_environment("MIR2X_FRIEND_GROUP_SCREENSHOT"):
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_GROUP_SCREENSHOT"))
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
	var stranger_strip := panel.get_node("Page/ChatPage/Messages/MessageRows").get_child(0) as Panel
	var stranger_text := stranger_strip.get_node_or_null("Text") as Label
	var stranger_add := stranger_strip.get_node_or_null("Add") as Button
	var stranger_block := stranger_strip.get_node_or_null("Block") as Button
	var stranger_style := stranger_strip.get_theme_stylebox("panel") as StyleBoxFlat
	if stranger_strip.name != "StrangerOperations" or stranger_strip.custom_minimum_size.y != 54 or stranger_strip.size.y != 54:
		_fail("stranger operation strip did not keep the original fixed 54px geometry")
		return
	if stranger_style.bg_color != Color(row_rgb.r, row_rgb.g, row_rgb.b, 64.0 / 255.0) or stranger_style.corner_radius_top_left != 4 or stranger_style.corner_radius_bottom_right != 4:
		_fail("stranger operation strip background diverged from C++")
		return
	if stranger_text == null or stranger_text.position != Vector2(8, 6) or stranger_text.get_theme_font_size("font_size") != 12:
		_fail("stranger operation explanation geometry diverged from C++")
		return
	if stranger_add == null or stranger_block == null or stranger_add.position != Vector2(8, 27) or stranger_block.position.y != 27 or stranger_add.get_signal_connection_list("pressed").is_empty() or stranger_block.get_signal_connection_list("pressed").is_empty():
		_fail("stranger operation buttons did not match the original positions and actions")
		return
	if OS.has_environment("MIR2X_FRIEND_STRANGER_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_STRANGER_SCREENSHOT"))
	panel.call("_open_chat", friend.cpid)
	await get_tree().process_frame
	await get_tree().process_frame
	var message_rows := panel.get_node("Page/ChatPage/Messages/MessageRows")
	var first_message := message_rows.get_child(0) as HBoxContainer
	if first_message == null or not first_message.get_child(0) is TextureRect or (first_message.get_child(0) as TextureRect).stretch_mode != TextureRect.STRETCH_SCALE:
		_fail("message avatar did not stretch the complete texture like the original client")
		return
	var incoming_bubble := first_message.get_child(1) as PanelContainer
	var outgoing_bubble := (message_rows.get_child(1) as HBoxContainer).get_child(1) as PanelContainer
	var incoming_style := incoming_bubble.get_theme_stylebox("panel") as StyleBoxFlat
	var outgoing_style := outgoing_bubble.get_theme_stylebox("panel") as StyleBoxFlat
	var half_alpha := 128.0 / 255.0
	var expected_incoming_width := clampf(40.0 + "晚上一起去矿洞吗？".to_utf8_buffer().size() * 6.0, 80.0, message_rows.size.x - 70.0)
	var expected_outgoing_width := clampf(40.0 + "好，城门口集合。".to_utf8_buffer().size() * 6.0, 80.0, message_rows.size.x - 70.0)
	if incoming_style.bg_color != Color(1, 0, 0, half_alpha) or outgoing_style.bg_color != Color(0, 128.0 / 255.0, 0, half_alpha) or incoming_style.border_width_left != 0 or outgoing_style.border_width_left != 0:
		_fail("message bubble colors/borders diverged from C++: incoming=%s outgoing=%s borders=%d/%d" % [incoming_style.bg_color, outgoing_style.bg_color, incoming_style.border_width_left, outgoing_style.border_width_left])
		return
	if incoming_bubble.size.x != expected_incoming_width or outgoing_bubble.size.x != expected_outgoing_width:
		_fail("message bubble UTF-8 widths diverged from C++: incoming=%s/%s outgoing=%s/%s" % [incoming_bubble.size.x, expected_incoming_width, outgoing_bubble.size.x, expected_outgoing_width])
		return
	if (incoming_bubble.get_child(0) as VBoxContainer).get_child_count() != 2 or (outgoing_bubble.get_child(0) as VBoxContainer).get_child_count() != 1:
		_fail("outgoing bubble kept the non-original sender header")
		return
	panel.call("_show_reference", 1001, "清风：晚上一起去矿洞吗？")
	await get_tree().process_frame
	var active_reference_bar := panel.get_node("Page/ChatPage/Composer/ReferenceBar") as Panel
	if not active_reference_bar.visible or active_reference_bar.size.y != 20.0 or panel.get_node("Page/ChatPage/Composer").size.y != 74.0 or panel.get("_refer_id") != 1001:
		_fail("reference bar did not preserve the original fixed composer geometry: visible=%s bar=%s composer=%s refer=%s" % [active_reference_bar.visible, active_reference_bar.size, panel.get_node("Page/ChatPage/Composer").size, panel.get("_refer_id")])
		return
	var reference_preview := panel.get_node("Page/ChatPage/Composer/ReferenceBar/Row/Preview") as Label
	if reference_preview.get_theme_color("font_color") != Color.WHITE:
		_fail("composer reference preview did not use the original default white text: %s" % reference_preview.get_theme_color("font_color"))
		return
	panel.get_node("Page/ChatPage/Composer/ReferenceBar/Row/Clear").emit_signal("pressed")
	if active_reference_bar.visible or panel.get("_refer_id") != null:
		_fail("reference clear control did not reset the pending reference")
		return
	panel.set("_pending_messages", {1: {"to": friend.cpid, "text": "发送中的消息", "refer": null}})
	panel.call("_refresh")
	await get_tree().process_frame
	if panel.get_node("Page/ChatPage/Messages/MessageRows").get_child_count() != 3:
		_fail("pending message bubble missing")
		return
	var pending_bubble := (panel.get_node("Page/ChatPage/Messages/MessageRows").get_child(2) as HBoxContainer).get_child(1) as PanelContainer
	var pending_style := pending_bubble.get_theme_stylebox("panel") as StyleBoxFlat
	if pending_style.bg_color != Color(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0, half_alpha) or pending_style.border_width_left != 0 or (pending_bubble.get_child(0) as VBoxContainer).get_child_count() != 1:
		_fail("pending bubble style/header diverged from C++: color=%s border=%d" % [pending_style.bg_color, pending_style.border_width_left])
		return
	var accepted := {"id": 101, "cpid": (2 << 32) | 101, "type": 2, "name": "新朋友", "gender": true, "job": 2}
	GameState.add_chat_peer(accepted, true, "新朋友已经通过你的好友申请，现在可以开始聊天了。")
	if GameState.chat_conversations[0].cpid != accepted.cpid or GameState.chat_conversations[0].preview.is_empty():
		_fail("accepted-friend preview missing")
		return
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	var notified := {"id": 505, "cpid": (2 << 32) | 505, "type": 2, "name": "远方来客", "gender": true, "job": 1}
	GameState.chat_log = []
	main.call("_apply_friend_result", notified, true)
	if not panel.call("_is_friend", notified.cpid) or not _expect_notice("远方来客已经通过了你的好友请求", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		_fail("server-side accepted friend result mismatch")
		return
	var accepted_log_count := GameState.chat_log.size()
	main.call("_apply_friend_result", notified, true)
	if GameState.chat_log.size() != accepted_log_count:
		_fail("duplicate accepted friend result produced another notice")
		return
	main.call("_apply_friend_result", friend, false)
	if not _expect_notice("清风已经拒绝了你的好友请求", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		_fail("server-side rejected friend result mismatch")
		return
	var main_friend := main.call("_ensure_extra_panel", "res://scenes/game/panels/friend_chat.tscn") as Control
	var main_selection := {}
	main_selection[int(friend.cpid)] = true
	main_friend.set("_selected_group", main_selection)
	main_friend.call("_request_group_name")
	var input_panel := main.call("_ensure_extra_panel", "res://scenes/game/panels/input_string.tscn") as Control
	if main.get("_pending_chat_group").size() != 1 or not input_panel.visible:
		_fail("shared group-name dialog did not open")
		return
	main.call("_on_input_cancelled")
	if not main.get("_pending_chat_group").is_empty():
		_fail("group-name cancellation left stale context")
		return
	main.call("_apply_chat_messages", [_message(1003, 120, friend.cpid, GameState.self_chat_cpid(), "闪烁测试")])
	if not main.get_node("ControlPanel").get("_button_blinks").has("Friend"):
		_fail("incoming friend message did not start HUD blink")
		return
	main.hide()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"):
		panel.call("_show_reference", 1001, "清风：晚上一起去矿洞吗？")
		chat_input.grab_focus()
		await get_tree().process_frame
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FRIEND_CHAT_SCREENSHOT"))
	print("FRIEND CHAT VISUAL PASS: frames, rows, pending, preview, resize and HUD blink")
	get_tree().quit()


func _test_friend_feedback(panel: Control, friend: Dictionary) -> bool:
	var saved_friends := GameState.chat_friends.duplicate(true)
	var saved_peers := GameState.chat_peers.duplicate(true)
	var saved_conversations := GameState.chat_conversations.duplicate(true)
	var saved_log := GameState.chat_log.duplicate(true)
	var probe := {"id": 404, "cpid": (2 << 32) | 404, "type": 2, "name": "测试好友", "gender": false, "job": 1}
	GameState.chat_peers[probe.cpid] = probe
	GameState.chat_log = []
	panel.call("_apply_add_friend_result", probe, NetworkClient.SM_OK + 1, 0, true)
	if not _expect_notice("无效的请求。", Color.WHITE, Color(0.0, 0.5, 0.0, 1.0)):
		return false
	var expected := {
		3: "测试好友已经拒绝了你的好友请求",
		4: "等待测试好友处理你的好友验证",
		5: "重复添加好友测试好友",
		1: "你已经被测试好友加入了黑名单",
	}
	for result in expected:
		panel.call("_apply_add_friend_result", probe, NetworkClient.SM_OK, result, true)
		if not _expect_notice(expected[result], Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
			return false
	panel.call("_show_page", 3)
	panel.call("_apply_add_friend_result", probe, NetworkClient.SM_OK, 2, true)
	if panel.get("_page") != 0 or not panel.call("_is_friend", probe.cpid) or not _expect_notice("测试好友已经通过了你的好友请求", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		_fail("accepted friend did not enter preview with original notice")
		return false
	var accepted_log_count := GameState.chat_log.size()
	panel.call("_apply_add_friend_result", probe, NetworkClient.SM_OK, 2, false)
	if GameState.chat_log.size() != accepted_log_count:
		_fail("existing friend acceptance produced another notice")
		return false
	panel.call("_apply_friend_action_result", int(friend.cpid), "accept", NetworkClient.SM_OK)
	if not _expect_notice("你已经通过清风的好友申请", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		return false
	panel.call("_apply_friend_action_result", int(friend.cpid), "reject", NetworkClient.SM_OK)
	if not _expect_notice("你已经拒绝清风的好友申请", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		return false
	panel.call("_apply_friend_action_result", int(friend.cpid), "block", NetworkClient.SM_OK)
	if not _expect_notice("你已经拉黑清风", Color(0.0, 1.0, 0.0, 1.0), Color.TRANSPARENT):
		return false
	panel.call("_apply_friend_action_result", int(friend.cpid), "accept", NetworkClient.SM_OK + 1)
	if not _expect_notice("无效的请求", Color(1.0, 0.25, 0.25, 1.0), Color.TRANSPARENT):
		return false
	panel.call("_apply_friend_action_result", int(friend.cpid), "block", NetworkClient.SM_OK + 1)
	if not _expect_notice("无效的拉黑请求", Color(1.0, 0.25, 0.25, 1.0), Color.TRANSPARENT):
		return false
	GameState.chat_friends = saved_friends
	GameState.chat_peers = saved_peers
	GameState.chat_conversations = saved_conversations
	GameState.chat_log = saved_log
	GameState.state_changed.emit()
	return true


func _expect_notice(expected_text: String, expected_color: Color, expected_background: Color) -> bool:
	var line: Dictionary = GameState.chat_log[-1]
	if line.text == expected_text and line.color == expected_color and line.background_color == expected_background:
		return true
	_fail("friend notice mismatch: %s" % line)
	return false


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
