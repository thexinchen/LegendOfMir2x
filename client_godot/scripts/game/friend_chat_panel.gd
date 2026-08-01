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
var _selected_group: Dictionary = {}
var _refer_id: Variant = null
var _resources: RefCounted = ActorResourceScript.new()


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
	$Page/ChatPage/Composer/Send.pressed.connect(_send)
	$Page/ChatPage/Composer/Input.gui_input.connect(_chat_input)
	$Page/GroupPage/Confirm.pressed.connect(_create_group)
	_register_special_peers()
	_refresh()


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
		child.queue_free()


func _fill_preview() -> void:
	var rows := $Page/ListScroll/Rows
	_clear(rows)
	for conversation in _state.chat_conversations:
		var cpid := int(conversation.get("cpid", 0))
		var peer: Dictionary = _state.chat_peers.get(cpid, {})
		var title: String = peer.get("name", "未知用户 %d" % (cpid & 0xFFFFFFFF))
		var unread := int(conversation.get("unread", 0))
		var suffix := "  (%d)" % unread if unread > 0 else ""
		_add_row(rows, title + suffix, conversation.get("preview", ""), func(): _open_chat(cpid))


func _fill_friends() -> void:
	var rows := $Page/ListScroll/Rows
	_clear(rows)
	_add_row(rows, "系统助手", "系统通知与好友验证", func(): _open_chat(SYSTEM_CPID))
	var self_cpid: int = _state.self_chat_cpid()
	_add_row(rows, "自己  %s" % _state.player_name, "", func(): _open_chat(self_cpid))
	for peer in _state.chat_friends:
		var cpid := int(peer.get("cpid", 0))
		_add_row(rows, peer.get("name", "未知好友"), _peer_description(peer), func(): _open_chat(cpid))


func _add_row(parent: Node, title: String, subtitle: String, action: Callable, trailing: String = "") -> void:
	var button := Button.new()
	button.custom_minimum_size = Vector2(0, 52)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.add_theme_font_size_override("font_size", 13)
	button.text = "  %s%s\n      %s" % [title, trailing, subtitle]
	button.pressed.connect(action)
	parent.add_child(button)


func _peer_description(peer: Dictionary) -> String:
	if int(peer.get("type", 0)) == 3:
		return "群聊 · %d 人" % peer.get("members", []).size()
	var jobs := ["战士", "法师", "道士"]
	var job := int(peer.get("job", -1))
	return jobs[job] if job >= 0 and job < jobs.size() else "玩家"


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
	await get_tree().process_frame
	$Page/ChatPage/Messages.scroll_vertical = int($Page/ChatPage/Messages.get_v_scroll_bar().max_value)


func _add_message_bubble(parent: Node, peer: Dictionary, sender_name: String, text: String, mine: bool, message: Dictionary) -> void:
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
	style.bg_color = Color(0.08, 0.38, 0.08, 0.88) if mine else Color(0.43, 0.10, 0.08, 0.88)
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
	var error := NetworkClient.send_chat_message(_selected_cpid, text, _refer_id, func(head: int, payload: PackedByteArray):
		if head != NetworkClient.SM_OK:
			$Status.text = "消息发送失败"
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
		$Status.text = "尚未连接服务器"


func _search(text: String) -> void:
	_search_serial += 1
	var serial := _search_serial
	var results := $Page/SearchPage/Results
	_clear(results)
	if text.strip_edges().is_empty():
		return
	NetworkClient.query_chat_peers(text.strip_edges(), func(head: int, payload: PackedByteArray):
		if serial != _search_serial or head != NetworkClient.SM_OK:
			return
		var reader := CerealReader.new(payload)
		var peers := reader.read_sd_chat_peer_list()
		if not reader.valid:
			return
		_clear(results)
		for peer in peers:
			_state.add_chat_peer(peer)
			var cpid := int(peer.get("cpid", 0))
			_add_row(results, "%s（%d）" % [peer.get("name", "未知"), peer.get("id", 0)], "点击发送好友申请", func(): _request_friend(cpid))
	)


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
				_state.add_chat_peer(peer, true)
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
			_state.add_chat_peer(peer, true)
			_open_chat(int(peer.get("cpid", 0)))
	)
