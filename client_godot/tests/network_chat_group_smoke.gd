extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const FRIEND_PANEL_PATH := "res://scenes/game/panels/friend_chat.tscn"
const INPUT_PANEL_PATH := "res://scenes/game/panels/input_string.tscn"
const GROUP_NAME := "Godot群聊闭环"
const GROUP_MESSAGE := "Godot group delivery"

var _role := "member"
var _main: Control
var _online := false
var _remote_uid := 0
var _creation_started := false
var _group_validated := false
var _message_started := false
var _reload := false
var _query_started := false
var _finished := false


func _ready() -> void:
	_role = OS.get_environment("MIR2X_CHAT_GROUP_ROLE")
	_reload = OS.get_environment("MIR2X_CHAT_GROUP_RELOAD") == "1"
	if _role not in ["creator", "member"]:
		_fail("invalid role: %s" % _role, 2)
		return
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(25.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or not _online or _main == null:
		return
	var group := _find_group()
	if not group.is_empty() and not _group_validated:
		_validate_group(group)
	if _group_validated:
		if _reload:
			_start_reload_query()
		elif _role == "creator" and not _message_started:
			_send_group_message()
		elif _has_group_message():
			_pass(group)
		return
	if _role != "creator" or _creation_started:
		return
	var remote := _find_remote_player("亚当")
	if remote.is_empty():
		return
	_remote_uid = int(remote.get("uid", 0))
	var remote_cpid := (2 << 32) | (_remote_uid & 0xFFFFFFFF)
	if not _is_friend(remote_cpid):
		return
	_creation_started = true
	_create_group(remote_cpid)


func _create_group(remote_cpid: int) -> void:
	var panel := _main.call("_ensure_extra_panel", FRIEND_PANEL_PATH) as Control
	panel.show()
	panel.call("_open_group_page")
	await get_tree().process_frame
	var rows := panel.get_node("Page/ListScroll/Rows")
	if rows.get_child_count() != 1:
		_fail("group page did not show the one-way friend fixture", 7)
		return
	(rows.get_child(0) as Button).pressed.emit()
	await get_tree().process_frame
	if not (panel.get("_selected_group") as Dictionary).has(remote_cpid):
		_fail("actual group member row did not select the friend", 8)
		return
	(panel.get_node("Toolbar/GroupConfirm") as TextureButton).pressed.emit()
	await get_tree().process_frame
	var input_panel := _main.call("_ensure_extra_panel", INPUT_PANEL_PATH) as Control
	var value_input := input_panel.get_node("ValueClip/Value") as LineEdit
	if not input_panel.visible or (input_panel.get_node("Title") as Label).text != "请输入你要建立的群名称" or value_input.secret:
		_fail("group-name input dialog diverged from C++", 9)
		return
	value_input.text = GROUP_NAME
	(input_panel.get_node("ConfirmButton") as TextureButton).pressed.emit()


func _validate_group(group: Dictionary) -> void:
	var member_ids: Array[int] = []
	for member_value in group.get("members", []):
		member_ids.append(int(member_value.get("dbid", 0)))
	member_ids.sort()
	if member_ids != [2, 3]:
		_fail("group member list mismatch: %s" % [member_ids], 10)
		return
	if _role == "creator":
		var panel := _main.call("_ensure_extra_panel", FRIEND_PANEL_PATH) as Control
		if int(panel.get("_selected_cpid")) != int(group.get("cpid", 0)) or int(panel.get("_page")) != 1:
			_fail("creator did not enter the newly created Godot group chat", 11)
			return
	_group_validated = true


func _send_group_message() -> void:
	_message_started = true
	var panel := _main.call("_ensure_extra_panel", FRIEND_PANEL_PATH) as Control
	var input := panel.get_node("Page/ChatPage/Composer/Input") as TextEdit
	input.text = GROUP_MESSAGE
	var event := InputEventKey.new()
	event.keycode = KEY_ENTER
	event.pressed = true
	input.gui_input.emit(event)


func _has_group_message() -> bool:
	for message_value in GameState.chat_messages.values():
		if GameState.chat_message_text(message_value) == GROUP_MESSAGE:
			return true
	return false


func _start_reload_query() -> void:
	if _query_started:
		return
	_query_started = true
	if NetworkClient.query_chat_peers(GROUP_NAME, _on_reload_query) != OK:
		_fail("failed to query the persisted group", 12)


func _on_reload_query(head_code: int, payload: PackedByteArray) -> void:
	if head_code != NetworkClient.SM_OK:
		_fail("persisted group query rejected", 13)
		return
	var reader := CerealReader.new(payload)
	var peers := reader.read_sd_chat_peer_list()
	if not reader.valid or not reader.at_end():
		_fail("persisted group query payload invalid: %s" % reader.error, 14)
		return
	for peer_value in peers:
		if str(peer_value.get("name", "")) != GROUP_NAME:
			continue
		var member_ids: Array[int] = []
		for member_value in peer_value.get("members", []):
			member_ids.append(int(member_value.get("dbid", 0)))
		member_ids.sort()
		if member_ids == [2, 3]:
			_pass(peer_value)
			return
	_fail("persisted group query omitted complete members", 15)


func _pass(group: Dictionary) -> void:
	var member_ids: Array[int] = []
	for member_value in group.get("members", []):
		member_ids.append(int(member_value.get("dbid", 0)))
	member_ids.sort()
	print("NETWORK CHAT GROUP %s PASS: cpid=%d members=%s reload=%s" % [_role.to_upper(), int(group.get("cpid", 0)), member_ids, _reload])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _find_group() -> Dictionary:
	for peer_value in GameState.chat_friends:
		var peer: Dictionary = peer_value
		if int(peer.get("type", 0)) == 3 and str(peer.get("name", "")) == GROUP_NAME:
			return peer
	return {}


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
	var account := "good" if _role == "creator" else "test"
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
	_fail("timed out role=%s online=%s remote=%d started=%s friends=%s" % [_role, _online, _remote_uid, _creation_started, GameState.chat_friends], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_CHAT_GROUP_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
