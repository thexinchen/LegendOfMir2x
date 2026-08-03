extends "res://scripts/game/closable_panel.gd"

signal group_name_requested(member_ids: Array)

const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const PAGE_PREVIEW := 0
const PAGE_CHAT := 1
const PAGE_FRIENDS := 2
const PAGE_SEARCH := 3
const PAGE_GROUP := 4
const SYSTEM_CPID := (1 << 32) | 0xFFFFFF01
const FRIEND_RESPONSE_EVENT := "_RSVD_NAME_AFRESP_8368138412597"
const FRIEND_NOTICE_BACKGROUND := Color(0.0, 0.5, 0.0, 1.0)

var _state: Node
var _page := PAGE_PREVIEW
var _selected_cpid := 0
var _search_serial := 0
var _search_results: Array = []
var _search_show_candidates := false
var _selected_group: Dictionary = {}
var _refer_id: Variant = null
var _resources: RefCounted = ActorResourceScript.new()
var _pending_messages: Dictionary = {}
var _next_pending_id := 1
var _resize_edge := -1
var _page_scroll := {PAGE_PREVIEW: 0.0, PAGE_CHAT: 0.0, PAGE_FRIENDS: 0.0, PAGE_SEARCH: 0.0, PAGE_GROUP: 0.0}
var _restoring_scroll := false


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	$Toolbar/Friends.pressed.connect(func(): _show_page(PAGE_FRIENDS))
	$Toolbar/Back.pressed.connect(func(): _show_page(PAGE_PREVIEW))
	$Toolbar/Search.pressed.connect(func(): _show_page(PAGE_SEARCH))
	$Toolbar/CreateGroup.pressed.connect(_open_group_page)
	$Toolbar/GroupConfirm.pressed.connect(_request_group_name)
	$Toolbar/Invert.pressed.connect(_invert_group_selection)
	$Page/SearchPage/Query.text_changed.connect(_search)
	$Page/SearchPage/Query.text_submitted.connect(_show_search_candidates)
	$Page/SearchPage/Clear.pressed.connect(_clear_search)
	$Page/ChatPage/Composer/ReferenceBar/Row/Clear.pressed.connect(_clear_reference)
	$Page/ChatPage/Composer/Input.gui_input.connect(_chat_input)
	$SliderHit.gui_input.connect(_on_slider_input)
	_hide_stock_scrollbars()
	_configure_group_toolbar()
	_register_special_peers()
	_refresh()


func _process(_delta: float) -> void:
	var bar := _current_scrollbar()
	if bar and not _restoring_scroll:
		_page_scroll[_page] = bar.value
	_update_slider_thumb()


func _hide_stock_scrollbars() -> void:
	for scroll in [$Page/ListScroll, $Page/ChatPage/Messages, $Page/SearchPage/SearchScroll]:
		var bar: VScrollBar = scroll.get_v_scroll_bar()
		bar.modulate = Color.TRANSPARENT
		bar.mouse_filter = Control.MOUSE_FILTER_IGNORE


func _configure_group_toolbar() -> void:
	var create_off: Dictionary = _resources.frame("proguse", 0x00000910)
	var create_down: Dictionary = _resources.frame("proguse", 0x00000911)
	var invert_off: Dictionary = _resources.frame("proguse", 0x00000860)
	var invert_down: Dictionary = _resources.frame("proguse", 0x00000861)
	$Toolbar/GroupConfirm.texture_normal = create_off.get("texture")
	$Toolbar/GroupConfirm.texture_pressed = create_down.get("texture")
	$Toolbar/Invert.texture_normal = invert_off.get("texture")
	$Toolbar/Invert.texture_pressed = invert_down.get("texture")
	var order := [$Toolbar/Invert, $Toolbar/GroupConfirm, $Toolbar/CreateGroup, $Toolbar/Search, $Toolbar/Friends, $Toolbar/Back]
	for index in range(order.size()):
		$Toolbar.move_child(order[index], index)


func _current_scrollbar() -> VScrollBar:
	if _page == PAGE_PREVIEW or _page == PAGE_FRIENDS or _page == PAGE_GROUP:
		return $Page/ListScroll.get_v_scroll_bar()
	if _page == PAGE_CHAT:
		return $Page/ChatPage/Messages.get_v_scroll_bar()
	if _page == PAGE_SEARCH:
		return $Page/SearchPage/SearchScroll.get_v_scroll_bar()
	return null


func _update_slider_thumb() -> void:
	var bar := _current_scrollbar()
	var reach := maxf(0.0, bar.max_value - bar.page) if bar else 0.0
	var ratio := clampf(bar.value / reach, 0.0, 1.0) if reach > 0.0 else 0.0
	$SliderThumb.position = Vector2(size.x - 40.0, 58.0 + ratio * (size.y - 140.0))


func _on_slider_input(event: InputEvent) -> void:
	var active: bool = event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed
	active = active or (event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT))
	if not active:
		return
	var bar := _current_scrollbar()
	if bar:
		var reach := maxf(0.0, bar.max_value - bar.page)
		var ratio := clampf((event.position.y - 12.0) / maxf(1.0, size.y - 140.0), 0.0, 1.0)
		bar.value = ratio * reach
	$SliderHit.accept_event()


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			_resize_edge = _edge_at(event.position)
			if _resize_edge >= 0:
				accept_event()
				return
		elif _resize_edge >= 0:
			_resize_edge = -1
			accept_event()
			return
	elif event is InputEventMouseMotion and _resize_edge >= 0:
		_apply_resize(event.relative)
		accept_event()
		return
	super._gui_input(event)


func _edge_at(local: Vector2) -> int:
	var left := local.x < 12.0
	var right := local.x >= size.x - 10.0
	var top := local.y < 10.0
	var bottom := local.y >= size.y - 10.0
	if top:
		return 0 if left else (2 if right else 1)
	if bottom:
		return 5 if left else (7 if right else 6)
	return 3 if left else (4 if right else -1)


func _apply_resize(delta: Vector2) -> void:
	var new_position := position
	var new_size := size
	if _resize_edge in [0, 3, 5]:
		var applied_x := minf(delta.x, new_size.x - custom_minimum_size.x)
		new_position.x += applied_x
		new_size.x -= applied_x
	elif _resize_edge in [2, 4, 7]:
		new_size.x = maxf(custom_minimum_size.x, new_size.x + delta.x)
	if _resize_edge <= 2:
		var applied_y := minf(delta.y, new_size.y - custom_minimum_size.y)
		new_position.y += applied_y
		new_size.y -= applied_y
	elif _resize_edge >= 5:
		new_size.y = maxf(custom_minimum_size.y, new_size.y + delta.y)
	position = new_position
	size = new_size


func _register_special_peers() -> void:
	_state.chat_peers[SYSTEM_CPID] = {"id": 0xFFFFFF01, "cpid": SYSTEM_CPID, "type": 1, "name": "系统助手"}
	var self_cpid: int = _state.self_chat_cpid()
	if self_cpid != (2 << 32):
		_state.chat_peers[self_cpid] = {"id": self_cpid & 0xFFFFFFFF, "cpid": self_cpid, "type": 2, "name": _state.player_name, "gender": _state.player_gender, "job": _state.player_job}


func _show_page(page: int) -> void:
	var previous_bar := _current_scrollbar()
	if previous_bar:
		_page_scroll[_page] = previous_bar.value
	_page = page
	_restoring_scroll = true
	$Page/ListScroll.visible = page == PAGE_PREVIEW or page == PAGE_FRIENDS or page == PAGE_GROUP
	$Page/ChatPage.visible = page == PAGE_CHAT
	$Page/SearchPage.visible = page == PAGE_SEARCH
	$Toolbar/Back.visible = page != PAGE_PREVIEW
	$Toolbar/Friends.visible = page == PAGE_PREVIEW
	$Toolbar/Search.visible = page == PAGE_FRIENDS
	$Toolbar/CreateGroup.visible = page == PAGE_FRIENDS
	$Toolbar/GroupConfirm.visible = page == PAGE_GROUP
	$Toolbar/Invert.visible = page == PAGE_GROUP
	_refresh()
	_restore_page_scroll(page)


func _restore_page_scroll(page: int) -> void:
	await get_tree().process_frame
	if _page != page:
		return
	var bar := _current_scrollbar()
	if bar:
		var reach := maxf(0.0, bar.max_value - bar.page)
		bar.value = clampf(float(_page_scroll.get(page, 0.0)), 0.0, reach)
	_restoring_scroll = false


func _open_group_page() -> void:
	_selected_group.clear()
	_show_page(PAGE_GROUP)


func _refresh() -> void:
	_register_special_peers()
	match _page:
		PAGE_PREVIEW:
			$Title.text = "【聊天记录】"
			_fill_preview()
		PAGE_FRIENDS:
			$Title.text = "【好友列表】"
			_fill_friends()
		PAGE_CHAT:
			var peer: Dictionary = _state.chat_peers.get(_selected_cpid, {})
			$Title.text = peer.get("name", "好友名称")
			_fill_messages()
		PAGE_SEARCH:
			$Title.text = "【查找用户】"
		PAGE_GROUP:
			$Title.text = "【创建群聊】"
			_fill_group_members()


func _clear(container: Node) -> void:
	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()


func _fill_preview() -> void:
	var rows := $Page/ListScroll/Rows
	_clear(rows)
	for conversation in _state.chat_conversations:
		var cpid := int(conversation.get("cpid", 0))
		var peer: Dictionary = _state.chat_peers.get(cpid, {})
		var title: String = peer.get("name", "未知用户 %d" % (cpid & 0xFFFFFFFF))
		_add_row(rows, peer, title, conversation.get("preview", ""), func(): _open_chat(cpid), 58)


func _fill_friends() -> void:
	var rows := $Page/ListScroll/Rows
	_clear(rows)
	var system_peer: Dictionary = _state.chat_peers.get(SYSTEM_CPID, {})
	_add_row(rows, system_peer, "系统助手", "", func(): _open_chat(SYSTEM_CPID))
	var self_cpid: int = _state.self_chat_cpid()
	var self_peer: Dictionary = _state.chat_peers.get(self_cpid, {})
	_add_row(rows, self_peer, "自己  %s" % _state.player_name, "", func(): _open_chat(self_cpid))
	for peer in _state.chat_friends:
		var cpid := int(peer.get("cpid", 0))
		_add_row(rows, peer, peer.get("name", "未知好友"), "", func(): _open_chat(cpid))


func _add_row(parent: Node, peer: Dictionary, title: String, subtitle: String, action: Callable, row_height := 52) -> Button:
	var button := Button.new()
	button.custom_minimum_size = Vector2(0, row_height)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.text = ""
	button.focus_mode = Control.FOCUS_NONE
	button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	button.add_theme_stylebox_override("normal", _row_style(Color.TRANSPARENT, 32.0 / 255.0))
	var hover_color := Color(231.0 / 255.0, 231.0 / 255.0, 189.0 / 255.0, 64.0 / 255.0)
	button.add_theme_stylebox_override("hover", _row_style(hover_color, 64.0 / 255.0))
	button.add_theme_stylebox_override("pressed", _row_style(hover_color, 64.0 / 255.0))
	button.pressed.connect(action)
	parent.add_child(button)
	var avatar := TextureRect.new()
	avatar.position = Vector2(4, 4)
	avatar.size = Vector2(42, 50) if row_height == 58 else Vector2(40, 44)
	avatar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	avatar.stretch_mode = TextureRect.STRETCH_SCALE
	avatar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var avatar_frame := _peer_avatar(peer)
	if not avatar_frame.is_empty():
		avatar.texture = avatar_frame.get("texture")
	button.add_child(avatar)
	var title_label := Label.new()
	title_label.position = Vector2(52, 7 if row_height == 58 else 14)
	title_label.size = Vector2(290, 20)
	title_label.text = title
	title_label.add_theme_font_size_override("font_size", 14)
	title_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	button.add_child(title_label)
	if not subtitle.is_empty():
		var subtitle_label := Label.new()
		subtitle_label.position = Vector2(52, 29)
		subtitle_label.size = Vector2(290, 18)
		subtitle_label.text = subtitle
		subtitle_label.add_theme_font_size_override("font_size", 12)
		subtitle_label.add_theme_color_override("font_color", Color(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0))
		subtitle_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		button.add_child(subtitle_label)
	return button


func _row_style(fill: Color, border_alpha: float) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = fill
	style.border_color = Color(231.0 / 255.0, 231.0 / 255.0, 189.0 / 255.0, border_alpha)
	style.set_border_width_all(1)
	return style


func _open_chat(cpid: int) -> void:
	_selected_cpid = cpid
	_state.mark_chat_read(cpid)
	NetworkClient.request_latest_chat_messages([cpid], 50, true, true)
	_show_page(PAGE_CHAT)


func _fill_messages() -> void:
	var conversation: Dictionary = {}
	for current in _state.chat_conversations:
		if int(current.get("cpid", 0)) == _selected_cpid:
			conversation = current
			break
	var rows := $Page/ChatPage/Messages/MessageRows
	_clear(rows)
	var self_cpid: int = _state.self_chat_cpid()
	for message in conversation.get("messages", []):
		var from := int(message.get("from", 0))
		var sender: Dictionary = _state.chat_peers.get(from, {})
		var sender_name: String = "我" if from == self_cpid else sender.get("name", "未知")
		var text: String = _state.chat_message_text(message)
		var refer: Variant = message.get("refer")
		if refer != null and _state.chat_messages.has(int(refer)):
			text = "引用：%s\n%s" % [_state.chat_message_text(_state.chat_messages[int(refer)]), text]
		_add_message_bubble(rows, sender, sender_name, text, from == self_cpid, message)
	for pending_value in _pending_messages.values():
		var pending: Dictionary = pending_value
		if int(pending.get("to", 0)) == _selected_cpid:
			var pending_text: String = pending.get("text", "")
			var pending_refer: Variant = pending.get("refer")
			if pending_refer != null and _state.chat_messages.has(int(pending_refer)):
				pending_text = "引用：%s\n%s" % [_state.chat_message_text(_state.chat_messages[int(pending_refer)]), pending_text]
			var self_peer: Dictionary = _state.chat_peers.get(self_cpid, {})
			_add_message_bubble(rows, self_peer, "我", pending_text, true, pending, true)
	var selected_peer: Dictionary = _state.chat_peers.get(_selected_cpid, {})
	if int(selected_peer.get("type", 0)) == 2 and _selected_cpid != self_cpid and not _is_friend(_selected_cpid):
		_add_stranger_operations(rows, selected_peer)
	await get_tree().process_frame
	$Page/ChatPage/Messages.scroll_vertical = int($Page/ChatPage/Messages.get_v_scroll_bar().max_value)


func _is_friend(cpid: int) -> bool:
	for peer_value in _state.chat_friends:
		if int(peer_value.get("cpid", 0)) == cpid:
			return true
	return false


func _add_stranger_operations(parent: Node, peer: Dictionary) -> void:
	var panel := VBoxContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.906, 0.906, 0.741, 0.25)
	style.corner_radius_top_left = 4
	style.corner_radius_top_right = 4
	style.corner_radius_bottom_left = 4
	style.corner_radius_bottom_right = 4
	var background := PanelContainer.new()
	background.add_theme_stylebox_override("panel", style)
	var label := Label.new()
	label.text = "对方不是你的好友，你可以添加对方为好友，或者屏蔽对方的消息。"
	label.add_theme_font_size_override("font_size", 12)
	panel.add_child(label)
	var actions := HBoxContainer.new()
	var add := Button.new()
	add.text = "添加"
	add.pressed.connect(_request_friend.bind(int(peer.get("cpid", 0)), false))
	var block := Button.new()
	block.text = "屏蔽"
	block.pressed.connect(func(): NetworkClient.send_block_player(int(peer.get("cpid", 0)), func(head: int, _payload: PackedByteArray): _apply_friend_action_result(int(peer.get("cpid", 0)), "block", head)))
	actions.add_child(add)
	actions.add_child(block)
	panel.add_child(actions)
	background.add_child(panel)
	parent.add_child(background)


func _add_message_bubble(parent: Node, peer: Dictionary, sender_name: String, text: String, mine: bool, message: Dictionary, pending := false) -> void:
	var row := HBoxContainer.new()
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_theme_constant_override("separation", 7)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var avatar := TextureRect.new()
	avatar.custom_minimum_size = Vector2(42, 42)
	avatar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	avatar.stretch_mode = TextureRect.STRETCH_SCALE
	var avatar_frame: Dictionary = _peer_avatar(peer)
	if not avatar_frame.is_empty():
		avatar.texture = avatar_frame.get("texture")
	var bubble := PanelContainer.new()
	var bubble_width := clampf(40.0 + text.to_utf8_buffer().size() * 6.0, 80.0, maxf(80.0, parent.size.x - 70.0))
	bubble.custom_minimum_size = Vector2(bubble_width, 48)
	bubble.size_flags_horizontal = Control.SIZE_SHRINK_END if mine else Control.SIZE_SHRINK_BEGIN
	var style := StyleBoxFlat.new()
	style.bg_color = Color(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0) if pending else (Color(0, 128.0 / 255.0, 0, 128.0 / 255.0) if mine else Color(1, 0, 0, 128.0 / 255.0))
	style.corner_radius_top_left = 4
	style.corner_radius_top_right = 4
	style.corner_radius_bottom_left = 4
	style.corner_radius_bottom_right = 4
	style.content_margin_left = 8
	style.content_margin_right = 8
	style.content_margin_top = 15 if mine else 3
	style.content_margin_bottom = 5
	bubble.add_theme_stylebox_override("panel", style)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 0)
	if not mine:
		var name_label := Label.new()
		name_label.text = sender_name
		name_label.add_theme_font_size_override("font_size", 10)
		content.add_child(name_label)
	var text_label := Label.new()
	text_label.text = text
	text_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	text_label.add_theme_font_size_override("font_size", 12)
	text_label.add_theme_color_override("font_color", Color.WHITE)
	content.add_child(text_label)
	var xml: String = _state.chat_message_xml(message)
	if FRIEND_RESPONSE_EVENT in xml:
		_add_friend_request_actions(content, xml)
	bubble.add_child(content)
	bubble.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
			var seq: Variant = message.get("seq")
			if seq != null:
				_show_reference(int(seq.get("id", 0)), "%s：%s" % [sender_name, text.left(150)])
	)
	if mine:
		row.add_child(spacer)
		row.add_child(bubble)
		row.add_child(avatar)
	else:
		row.add_child(avatar)
		row.add_child(bubble)
		row.add_child(spacer)
	parent.add_child(row)


func _add_friend_request_actions(parent: VBoxContainer, xml: String) -> void:
	var regex := RegEx.new()
	if regex.compile('cpid="([0-9]+)"') != OK:
		return
	var matched := regex.search(xml)
	if not matched:
		return
	var cpid := int(matched.get_string(1))
	var row := HBoxContainer.new()
	var accept := Button.new()
	accept.text = "同意并互加" if "addfriend" in xml else "同意"
	accept.pressed.connect(func(): _respond_friend_request(cpid, true, "addfriend" in xml, false))
	var reject := Button.new()
	reject.text = "拒绝"
	reject.pressed.connect(func(): _respond_friend_request(cpid, false, false, false))
	var block := Button.new()
	block.text = "拒绝并拉黑"
	block.pressed.connect(func(): _respond_friend_request(cpid, false, false, true))
	row.add_child(accept)
	row.add_child(reject)
	row.add_child(block)
	parent.add_child(row)


func _respond_friend_request(cpid: int, accept: bool, add_friend: bool, block: bool) -> void:
	$Status.text = ""
	if accept:
		NetworkClient.send_accept_friend(cpid, func(head: int, _payload: PackedByteArray): _apply_friend_action_result(cpid, "accept", head))
		if add_friend:
			_request_friend(cpid, false)
	else:
		NetworkClient.send_reject_friend(cpid, func(head: int, _payload: PackedByteArray): _apply_friend_action_result(cpid, "reject", head))
		if block:
			NetworkClient.send_block_player(cpid, func(head: int, _payload: PackedByteArray): _apply_friend_action_result(cpid, "block", head))


func _apply_friend_action_result(cpid: int, action: String, head: int) -> void:
	var name: String = _state.chat_peers.get(cpid, {}).get("name", "对方")
	if head != NetworkClient.SM_OK:
		_state.add_chat_log("无效的拉黑请求" if action == "block" else "无效的请求", 3)
		return
	match action:
		"accept": _state.add_chat_log("你已经通过%s的好友申请" % name, 1)
		"reject": _state.add_chat_log("你已经拒绝%s的好友申请" % name, 1)
		"block": _state.add_chat_log("你已经拉黑%s" % name, 1)


func _peer_avatar(peer: Dictionary) -> Dictionary:
	var peer_type := int(peer.get("type", 0))
	if peer_type == 3:
		return _resources.frame("proguse", 0x00001300)
	if peer_type == 1:
		return _resources.frame("proguse", 0x00001100)
	if peer_type == 2:
		var job := int(peer.get("job", 1))
		var job_index := 0
		if job & 2: job_index = 1
		elif job & 4: job_index = 2
		var face_id := 0x02000000 + job_index * 2 + (0 if bool(peer.get("gender", false)) else 1)
		return _resources.frame("proguse", face_id)
	return _resources.frame("proguse", 0x010007CF)


func _chat_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ENTER and not event.ctrl_pressed and not event.shift_pressed:
		get_viewport().set_input_as_handled()
		_send()


func _show_reference(message_id: int, preview: String) -> void:
	_refer_id = message_id
	$Page/ChatPage/Composer/ReferenceBar/Row/Preview.text = preview
	$Page/ChatPage/Composer/ReferenceBar.show()


func _clear_reference() -> void:
	_refer_id = null
	$Page/ChatPage/Composer/ReferenceBar/Row/Preview.text = ""
	$Page/ChatPage/Composer/ReferenceBar.hide()


func _send() -> void:
	var input: TextEdit = $Page/ChatPage/Composer/Input
	var text := input.text.strip_edges()
	if text.is_empty() or _selected_cpid == 0:
		return
	$Status.text = "发送中…"
	var sent_text := text
	var sent_refer: Variant = _refer_id
	var pending_id := _next_pending_id
	_next_pending_id += 1
	_pending_messages[pending_id] = {"to": _selected_cpid, "text": sent_text, "refer": sent_refer}
	_fill_messages()
	var error := NetworkClient.send_chat_message(_selected_cpid, text, _refer_id, func(head: int, payload: PackedByteArray):
		_pending_messages.erase(pending_id)
		if head != NetworkClient.SM_OK:
			$Status.text = "消息发送失败"
			_fill_messages()
			return
		var reader := CerealReader.new(payload)
		var seq := reader.read_sd_chat_message_db_seq()
		if not reader.valid:
			$Status.text = "发送确认无效"
			return
		var message_payload := NetworkClient._serialize_cereal_string("<layout><par>%s</par></layout>" % sent_text)
		_state.add_chat_message({"seq": seq, "refer": sent_refer, "from": _state.self_chat_cpid(), "to": _selected_cpid, "message": message_payload})
		$Status.text = ""
	)
	if error == OK:
		input.clear()
		_clear_reference()
	else:
		_pending_messages.erase(pending_id)
		_fill_messages()
		$Status.text = "尚未连接服务器"


func _search(text: String) -> void:
	_search_serial += 1
	var serial := _search_serial
	var results := $Page/SearchPage/SearchScroll/Results
	_clear(results)
	_search_results.clear()
	_search_show_candidates = false
	if text.strip_edges().is_empty():
		return
	NetworkClient.query_chat_peers(text.strip_edges(), func(head: int, payload: PackedByteArray):
		if serial != _search_serial or head != NetworkClient.SM_OK:
			return
		var reader := CerealReader.new(payload)
		var peers := reader.read_sd_chat_peer_list()
		if not reader.valid:
			return
		for peer in peers:
			_state.add_chat_peer(peer)
		_search_results = peers
		_render_search_results()
	)


func _show_search_candidates(_text: String) -> void:
	if $Page/SearchPage/Query.text.strip_edges().is_empty():
		return
	_search_show_candidates = true
	_render_search_results()


func _clear_search() -> void:
	$Page/SearchPage/Query.clear()
	_search_results.clear()
	_search_show_candidates = false
	_clear($Page/SearchPage/SearchScroll/Results)
	_page_scroll[PAGE_SEARCH] = 0.0
	$Page/SearchPage/SearchScroll.scroll_vertical = 0
	$Page/SearchPage/Query.grab_focus()


func _render_search_results() -> void:
	var results := $Page/SearchPage/SearchScroll/Results
	_clear(results)
	var query: String = $Page/SearchPage/Query.text.strip_edges()
	for peer_value in _search_results:
		var peer: Dictionary = peer_value
		var cpid := int(peer.get("cpid", 0))
		if _search_show_candidates:
			_add_search_candidate(results, peer, cpid)
		else:
			_add_search_suggestion(results, peer, query)


func _add_search_candidate(parent: Node, peer: Dictionary, cpid: int) -> void:
	var row := _add_row(parent, peer, "%s（%d）" % [peer.get("name", "未知"), peer.get("id", 0)], "", func(): pass)
	row.mouse_default_cursor_shape = Control.CURSOR_ARROW
	var title := row.get_child(1) as Label
	title.position.y = 10
	if cpid == _state.self_chat_cpid():
		return
	var add := Button.new()
	add.name = "Add"
	add.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	add.position = Vector2(-48, 17)
	add.size = Vector2(44, 24)
	add.text = "添加"
	add.add_theme_font_size_override("font_size", 12)
	add.pressed.connect(_request_friend.bind(cpid, true))
	row.add_child(add)


func _add_search_suggestion(parent: Node, peer: Dictionary, query: String) -> void:
	var button := Button.new()
	button.custom_minimum_size = Vector2(0, 30)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.focus_mode = Control.FOCUS_NONE
	button.add_theme_font_size_override("font_size", 14)
	button.text = "      %s（%d）" % [peer.get("name", "未知"), peer.get("id", 0)]
	button.add_theme_stylebox_override("normal", _row_style(Color(0.5, 0.5, 0.5, 0.25), 0.125))
	button.add_theme_stylebox_override("hover", _row_style(Color(0.906, 0.906, 0.741, 0.25), 0.25))
	button.pressed.connect(func():
		$Page/SearchPage/Query.text = str(peer.get("id", 0)) if query == str(peer.get("id", 0)) else peer.get("name", "")
		_search_show_candidates = true
		_render_search_results()
	)
	parent.add_child(button)
	var icon := TextureRect.new()
	icon.position = Vector2(8, 5)
	icon.size = Vector2(20, 20)
	var frame: Dictionary = _resources.frame("proguse", 0x00001200)
	if not frame.is_empty():
		icon.texture = frame.get("texture")
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	button.add_child(icon)


func _request_friend(cpid: int, switch_to_preview: bool = true) -> void:
	var peer: Dictionary = _state.chat_peers.get(cpid, {})
	NetworkClient.send_add_friend(cpid, func(head: int, payload: PackedByteArray):
		if head != NetworkClient.SM_OK:
			_apply_add_friend_result(peer, head, 0, switch_to_preview)
			return
		var reader := CerealReader.new(payload)
		_apply_add_friend_result(peer, head, reader.read_sd_add_friend_notif(), switch_to_preview)
	)


func _apply_add_friend_result(peer: Dictionary, head: int, result: int, switch_to_preview: bool) -> void:
	$Status.text = ""
	if head != NetworkClient.SM_OK:
		_state.add_chat_log("无效的请求。", 0, FRIEND_NOTICE_BACKGROUND)
		return
	var name: String = peer.get("name", "对方")
	match result:
		2:
			var was_friend := _is_friend(int(peer.get("cpid", 0)))
			if not was_friend:
				_state.add_chat_peer(peer, true, "%s已经通过你的好友申请，现在可以开始聊天了。" % name)
				_state.add_chat_log("%s已经通过了你的好友请求" % name, 1)
			if switch_to_preview:
				_show_page(PAGE_PREVIEW)
		3: _state.add_chat_log("%s已经拒绝了你的好友请求" % name, 1)
		4: _state.add_chat_log("等待%s处理你的好友验证" % name, 1)
		5: _state.add_chat_log("重复添加好友%s" % name, 1)
		1: _state.add_chat_log("你已经被%s加入了黑名单" % name, 1)


func _fill_group_members() -> void:
	var parent := $Page/ListScroll/Rows
	_clear(parent)
	for peer_value in _state.chat_friends:
		var peer: Dictionary = peer_value
		var cpid := int(peer.get("cpid", 0))
		var button := _add_row(parent, peer, peer.get("name", "未知好友"), "", _toggle_group_member.bind(cpid))
		var box := ColorRect.new()
		box.set_anchors_preset(Control.PRESET_TOP_RIGHT)
		box.position = Vector2(-28, 17)
		box.size = Vector2(16, 16)
		box.color = Color.TRANSPARENT
		box.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var border := StyleBoxFlat.new()
		border.bg_color = Color.TRANSPARENT
		border.border_color = Color(0.906, 0.906, 0.741, 0.5)
		border.set_border_width_all(1)
		var check_panel := Panel.new()
		check_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		check_panel.add_theme_stylebox_override("panel", border)
		check_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
		box.add_child(check_panel)
		if _selected_group.has(cpid):
			var check := TextureRect.new()
			check.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			var frame: Dictionary = _resources.frame("proguse", 0x00000480)
			check.texture = frame.get("texture")
			check.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			check.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
			check.mouse_filter = Control.MOUSE_FILTER_IGNORE
			box.add_child(check)
		button.add_child(box)


func _toggle_group_member(cpid: int) -> void:
	if _selected_group.has(cpid):
		_selected_group.erase(cpid)
	else:
		_selected_group[cpid] = true
	_fill_group_members()


func _invert_group_selection() -> void:
	for peer_value in _state.chat_friends:
		var cpid := int(peer_value.get("cpid", 0))
		if _selected_group.has(cpid):
			_selected_group.erase(cpid)
		else:
			_selected_group[cpid] = true
	_fill_group_members()


func _request_group_name() -> void:
	if _selected_group.is_empty():
		return
	var ids: Array = []
	for cpid in _selected_group:
		ids.append(int(cpid) & 0xFFFFFFFF)
	if ids.size() > 512:
		$Status.text = "群聊成员不能超过 512 人"
		return
	group_name_requested.emit(ids)


func create_group_named(group_name: String, ids: Array) -> void:
	var clean_name := group_name.strip_edges()
	if clean_name.is_empty() or ids.is_empty():
		$Status.text = "无效的群聊名称"
		return
	NetworkClient.create_chat_group(clean_name, ids, func(head: int, payload: PackedByteArray):
		if head != NetworkClient.SM_OK:
			$Status.text = "创建群聊失败"
			return
		var reader := CerealReader.new(payload)
		var peer := reader.read_sd_chat_peer()
		if reader.valid:
			_state.add_chat_peer(peer, true, "你已经加入了群聊，现在就可以聊天了。")
			_open_chat(int(peer.get("cpid", 0)))
	)
