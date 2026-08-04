extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const FRIEND_PANEL_PATH := "res://scenes/game/panels/friend_chat.tscn"
const SYSTEM_CPID := (1 << 32) | 0xFFFFFF01
const REQUEST_EVENT := "_RSVD_NAME_AFRESP_8368138412597"
const MARKER := "好友申请四按钮验证"

var _role := "target"
var _main: Control
var _online := false
var _remote_uid := 0
var _request_started := false
var _marker_seen := false
var _message_floor := 0
var _click_started := false
var _finished := false


func _ready() -> void:
	_role = OS.get_environment("MIR2X_FRIEND_REQUEST_ROLE")
	if _role not in ["requester", "target"]:
		_fail("invalid role: %s" % _role, 2)
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(25.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or not _online or _main == null:
		return
	var remote := _find_remote_player("亚当" if _role == "requester" else "夏娃")
	if remote.is_empty():
		return
	_remote_uid = int(remote.get("uid", 0))
	var remote_peer := {
		"id": _remote_uid & 0xFFFFFFFF,
		"cpid": (2 << 32) | (_remote_uid & 0xFFFFFFFF),
		"type": 2,
		"name": remote.get("name", ""),
		"gender": bool(remote.get("gender", false)),
		"job": int(remote.get("job", 0)),
	}
	GameState.add_chat_peer(remote_peer)
	if _role == "requester":
		_run_requester(remote_peer)
	else:
		_run_target(remote_peer)


func _run_requester(remote_peer: Dictionary) -> void:
	if not _request_started:
		_request_started = true
		_start_request(remote_peer)
		return
	if _is_friend(int(remote_peer.cpid)):
		print("NETWORK FRIEND REQUESTER PASS: accepted=%d" % int(remote_peer.cpid))
		_finish()


func _start_request(remote_peer: Dictionary) -> void:
	if NetworkClient.send_player_say(MARKER) != OK:
		_fail("failed to send marker", 7)
		return
	await get_tree().create_timer(0.8).timeout
	var panel := _main.call("_ensure_extra_panel", FRIEND_PANEL_PATH) as Control
	panel.call("_request_friend", int(remote_peer.cpid), false)


func _run_target(remote_peer: Dictionary) -> void:
	if not _marker_seen:
		for message_value in GameState.player_say_messages.get(_remote_uid, []):
			if str(message_value.get("text", "")) == MARKER:
				_marker_seen = true
				for message_id_value in GameState.chat_messages:
					_message_floor = maxi(_message_floor, int(message_id_value))
				break
		return
	if _click_started:
		return
	for message_id_value in GameState.chat_messages:
		var message_id := int(message_id_value)
		if message_id <= _message_floor:
			continue
		var message: Dictionary = GameState.chat_messages[message_id]
		var xml := GameState.chat_message_xml(message)
		if REQUEST_EVENT in xml and ('cpid="%d"' % int(remote_peer.cpid)) in xml:
			_click_started = true
			_click_plain_accept(message_id, remote_peer)
			return


func _click_plain_accept(message_id: int, remote_peer: Dictionary) -> void:
	var panel := _main.call("_ensure_extra_panel", FRIEND_PANEL_PATH) as Control
	panel.call("_open_chat", SYSTEM_CPID)
	await get_tree().process_frame
	await get_tree().process_frame
	var conversation: Dictionary = {}
	for value in GameState.chat_conversations:
		if int(value.get("cpid", 0)) == SYSTEM_CPID:
			conversation = value
			break
	var message_index := -1
	var messages: Array = conversation.get("messages", [])
	for index in messages.size():
		if int(messages[index].get("seq", {}).get("id", 0)) == message_id:
			message_index = index
			break
	var rows := panel.get_node("Page/ChatPage/Messages/MessageRows")
	if message_index < 0 or message_index >= rows.get_child_count():
		_fail("request message row unavailable", 8)
		return
	var message_row := rows.get_child(message_index) as HBoxContainer
	var bubble := message_row.get_child(1) as PanelContainer
	var content := bubble.get_child(0) as VBoxContainer
	var actions := content.get_child(content.get_child_count() - 1) as VBoxContainer
	var labels: Array[String] = []
	for child in actions.get_children():
		labels.append((child as Button).text)
	if labels != ["同意", "同意并添加对方为好友", "拒绝", "拒绝并将对方加入黑名单"]:
		_fail("real request actions mismatch: %s" % [labels], 9)
		return
	(actions.get_child(0) as Button).pressed.emit()
	await get_tree().create_timer(1.5).timeout
	if _is_friend(int(remote_peer.cpid)):
		_fail("plain accept incorrectly added the requester back", 10)
		return
	print("NETWORK FRIEND TARGET PASS: actions=4 plain-accept no-add-back")
	_finish()


func _is_friend(cpid: int) -> bool:
	for peer_value in GameState.chat_friends:
		if int(peer_value.get("cpid", 0)) == cpid:
			return true
	return false


func _find_remote_player(expected_name: String) -> Dictionary:
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 2 and int(uid_value) != GameState.player_uid and str(creature.get("name", "")) == expected_name:
			return creature
	return {}


func _on_connection_changed(connected: bool, _message: String) -> void:
	if not connected:
		return
	var account := "good" if _role == "requester" else "test"
	if NetworkClient.login(account, "123456") != OK:
		_fail("failed to send login for %s" % account, 3)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 4)
		NetworkClient.SM_ONLINEOK:
			var online := Protocol.decode_sm_online_ok(payload)
			var action: Dictionary = online.get("action", {})
			GameState.set_player_online({
				"uid": online.get("uid", 0), "name": online.get("name", ""),
				"gender": online.get("gender", 0), "job": online.get("job", 0),
				"map_uid": online.get("mapUID", 0), "x": action.get("x", 0),
				"y": action.get("y", 0), "direction": action.get("direction", 0),
			})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
			_online = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 5)


func _on_timeout() -> void:
	_fail("timed out role=%s online=%s remote=%d marker=%s floor=%d messages=%d" % [_role, _online, _remote_uid, _marker_seen, _message_floor, GameState.chat_messages.size()], 11)


func _finish() -> void:
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_FRIEND_REQUEST_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
