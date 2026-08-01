extends "res://scripts/game/closable_panel.gd"

const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const PAGE_PREVIEW := 0
const PAGE_CHAT := 1
const PAGE_FRIENDS := 2
const PAGE_SEARCH := 3
const PAGE_GROUP := 4
const SYSTEM_CPID := (1 << 32) | 0xFFFFFF01
const FRIEND_RESPONSE_EVENT := "_RSVD_NAME_AFRESP_8368138412597"

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


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	$Toolbar/Friends.pressed.connect(func(): _show_page(PAGE_FRIENDS))
	$Toolbar/Back.pressed.connect(func(): _show_page(PAGE_PREVIEW))
	$Toolbar/Search.pressed.connect(func(): _show_page(PAGE_SEARCH))
	$Toolbar/CreateGroup.pressed.connect(func(): _show_page(PAGE_GROUP))
	$Page/SearchPage/Query.text_changed.connect(_search)
	$Page/SearchPage/Query.text_submitted.connect(_show_search_candidates)
	$Page/ChatPage/Composer/Send.pressed.connect(_send)
	$Page/ChatPage/Composer/Input.gui_input.connect(_chat_input)
	$Page/GroupPage/Confirm.pressed.connect(_create_group)
	$SliderHit.gui_input.connect(_on_slider_input)
	_hide_stock_scrollbars()
	_register_special_peers()
	_refresh()


func _process(_delta: float) -> void:
	_update_slider_thumb()


func _hide_stock_scrollbars() -> void:
	for scroll in [$Page/ListScroll, $Page/ChatPage/Messages]:
		var bar: VScrollBar = scroll.get_v_scroll_bar()
		bar.modulate = Color.TRANSPARENT
		bar.mouse_filter = Control.MOUSE_FILTER_IGNORE


func _current_scrollbar() -> VScrollBar:
	if _page == PAGE_PREVIEW or _page == PAGE_FRIENDS:
		return $Page/ListScroll.get_v_scroll_bar()
	if _page == PAGE_CHAT:
		return $Page/ChatPage/Messages.get_v_scroll_bar()
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
	_page = page
	$Page/ListScroll.visible = page == PAGE_PREVIEW or page == PAGE_FRIENDS
	$Page/ChatPage.visible = page == PAGE_CHAT
	$Page/SearchPage.visible = page == PAGE_SEARCH
	$Page/GroupPage.visible = page == PAGE_GROUP
	$Toolbar/Back.visible = page != PAGE_PREVIEW
	$Toolbar/Friends.visible = page == PAGE_PREVIEW
	$Toolbar/Search.visible = page == PAGE_FRIENDS
	$Toolbar/CreateGroup.visible = page == PAGE_FRIENDS
	_refresh()


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


func _add_row(parent: Node, peer: Dictionary, title: String, subtitle: String, action: Callable, row_height := 52) -> void:
	var button := Button.new()
	button.custom_minimum_size = Vector2(0, row_height)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.text = ""
	button.focus_mode = Control.FOCUS_NONE
	button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	button.add_theme_stylebox_override("normal", _row_style(Color(0, 0, 0, 0), 0.125))
	button.add_theme_stylebox_override("hover", _row_style(Color(0.906, 0.906, 0.741, 0.25), 0.25))
	button.add_theme_stylebox_override("pressed", _row_style(Color(0.906, 0.906, 0.741, 0.375), 0.375))
	button.pressed.connect(action)
	parent.add_child(button)
	var avatar := TextureRect.new()
	avatar.position = Vector2(4, 4)
	avatar.size = Vector2(42, 50) if row_height == 58 else Vector2(40, 44)
	avatar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	avatar.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
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
		subtitle_label.add_theme_color_override("font_color", Color(0.5, 0.5, 0.5))
		subtitle_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		button.add_child(subtitle_label)


func _row_style(fill: Color, border_alpha: float) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = fill
	style.border_color = Color(0.906, 0.906, 0.741, border_alpha)
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
	add.pressed.connect(_request_friend.bind(int(peer.get("cpid", 0))))
	var block := Button.new()
	block.text = "屏蔽"
	block.pressed.connect(func(): NetworkClient.send_block_player(int(peer.get("cpid", 0)), func(_head: int, _payload: PackedByteArray): pass))
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
	avatar.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	var avatar_frame: Dictionary = _peer_avatar(peer)
	if not avatar_frame.is_empty():
		avatar.texture = avatar_frame.get("texture")
	var bubble := PanelContainer.new()
	bubble.custom_minimum_size = Vector2(90, 48)
	bubble.size_flags_horizontal = Control.SIZE_SHRINK_END if mine else Control.SIZE_SHRINK_BEGIN
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.35, 0.35, 0.35, 0.65) if pending else (Color(0.08, 0.38, 0.08, 0.88) if mine else Color(0.43, 0.10, 0.08, 0.88))
	style.border_color = Color(0.72, 0.72, 0.46, 0.65)
	style.set_border_width_all(1)
	style.corner_radius_top_left = 4
	style.corner_radius_top_right = 4
	style.corner_radius_bottom_left = 4
	style.corner_radius_bottom_right = 4
	style.content_margin_left = 8
	style.content_margin_right = 8
	style.content_margin_top = 5
	style.content_margin_bottom = 5
	bubble.add_theme_stylebox_override("panel", style)
	var content := VBoxContainer.new()
	var name_label := Label.new()
	name_label.text = sender_name
	name_label.add_theme_font_size_override("font_size", 10)
	name_label.add_theme_color_override("font_color", Color(0.82, 0.82, 0.68))
	var text_label := Label.new()
	text_label.text = text
	text_label.custom_minimum_size.x = minf(250.0, maxf(70.0, text.length() * 13.0))
	text_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	text_label.add_theme_font_size_override("font_size", 12)
	text_label.add_theme_color_override("font_color", Color.WHITE)
	content.add_child(name_label)
	content.add_child(text_label)
	var xml: String = _state.chat_message_xml(message)
	if FRIEND_RESPONSE_EVENT in xml:
		_add_friend_request_actions(content, xml)
	bubble.add_child(content)
	bubble.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
			var seq: Variant = message.get("seq")
			if seq != null:
				_refer_id = int(seq.get("id", 0))
				$Page/ChatPage/Reference.text = "引用：%s：%s" % [sender_name, text.left(80)]
				$Page/ChatPage/Reference.show()
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
	var done := func(head: int, _payload: PackedByteArray):
		$Status.text = "好友申请已处理" if head == NetworkClient.SM_OK else "好友申请处理失败"
	if accept:
		NetworkClient.send_accept_friend(cpid, done)
		if add_friend:
			NetworkClient.send_add_friend(cpid, func(_head: int, _payload: PackedByteArray): pass)
	else:
		NetworkClient.send_reject_friend(cpid, done)
		if block:
			NetworkClient.send_block_player(cpid, func(_head: int, _payload: PackedByteArray): pass)


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
		_refer_id = null
		$Page/ChatPage/Reference.hide()
	else:
		_pending_messages.erase(pending_id)
		_fill_messages()
		$Status.text = "尚未连接服务器"


func _search(text: String) -> void:
	_search_serial += 1
	var serial := _search_serial
	var results := $Page/SearchPage/Results
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


func _render_search_results() -> void:
	var results := $Page/SearchPage/Results
	_clear(results)
	var query: String = $Page/SearchPage/Query.text.strip_edges()
	for peer_value in _search_results:
		var peer: Dictionary = peer_value
		var cpid := int(peer.get("cpid", 0))
		if _search_show_candidates:
			var action: Callable = func(): pass
			if cpid != _state.self_chat_cpid():
				action = _request_friend.bind(cpid)
			_add_row(results, peer, "%s（%d）" % [peer.get("name", "未知"), peer.get("id", 0)], "点击发送好友申请" if cpid != _state.self_chat_cpid() else "", action)
		else:
			_add_search_suggestion(results, peer, query)


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


func _request_friend(cpid: int) -> void:
	var peer: Dictionary = _state.chat_peers.get(cpid, {})
	NetworkClient.send_add_friend(cpid, func(head: int, payload: PackedByteArray):
		if head != NetworkClient.SM_OK:
			$Status.text = "无效的好友请求"
			return
		var reader := CerealReader.new(payload)
		var result := reader.read_sd_add_friend_notif()
		match result:
			2:
				_state.add_chat_peer(peer, true, "%s已经通过你的好友申请，现在可以开始聊天了。" % peer.get("name", "对方"))
				$Status.text = "%s 已成为你的好友" % peer.get("name", "对方")
			3: $Status.text = "对方拒绝了好友申请"
			4: $Status.text = "等待对方处理好友申请"
			5: $Status.text = "对方已经是你的好友"
			1: $Status.text = "你已被对方屏蔽"
			_: $Status.text = "无效的好友请求"
	)


func _fill_group_members() -> void:
	var parent := $Page/GroupPage/Members
	_clear(parent)
	for peer in _state.chat_friends:
		if int(peer.get("type", 0)) != 2:
			continue
		var cpid := int(peer.get("cpid", 0))
		var check := CheckBox.new()
		check.text = peer.get("name", "未知好友")
		check.button_pressed = _selected_group.has(cpid)
		check.toggled.connect(func(enabled: bool):
			if enabled: _selected_group[cpid] = true
			else: _selected_group.erase(cpid)
		)
		parent.add_child(check)


func _create_group() -> void:
	var group_name: String = $Page/GroupPage/Name.text.strip_edges()
	if group_name.is_empty() or _selected_group.is_empty():
		$Status.text = "请输入群名并选择好友"
		return
	var ids: Array = []
	for cpid in _selected_group:
		ids.append(int(cpid) & 0xFFFFFFFF)
	NetworkClient.create_chat_group(group_name, ids, func(head: int, payload: PackedByteArray):
		if head != NetworkClient.SM_OK:
			$Status.text = "创建群聊失败"
			return
		var reader := CerealReader.new(payload)
		var peer := reader.read_sd_chat_peer()
		if reader.valid:
			_state.add_chat_peer(peer, true, "你已经加入了群聊，现在就可以聊天了。")
			_open_chat(int(peer.get("cpid", 0)))
	)
