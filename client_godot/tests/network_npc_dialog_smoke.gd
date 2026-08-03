extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const NPC_PANEL_PATH := "res://scenes/game/panels/npc_chat.tscn"
const PURCHASE_PANEL_PATH := "res://scenes/game/panels/purchase.tscn"
const SYS_ENTER := "_RSVD_NAME_ENTER_90360178872"
const TARGET_NPC_ID := 20

var _main: Control
var _target_uid := 0
var _enter_sent := false
var _purchase_sent := false
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(20.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	if not _enter_sent:
		_target_uid = _find_target_npc()
		if _target_uid == 0:
			return
		if NetworkClient.send_npc_event(_target_uid, "", SYS_ENTER) != OK:
			_fail("failed to send the real NPC enter event", 7)
			return
		_enter_sent = true
		return
	var dialog: Dictionary = GameState.npc_dialog
	if int(dialog.get("npcUID", 0)) != _target_uid:
		return
	var xml := str(dialog.get("xmlLayout", ""))
	var npc_panel := _main.get("_extra_panel_nodes").get(NPC_PANEL_PATH) as Control
	if npc_panel == null or not npc_panel.visible:
		return
	if not _purchase_sent:
		if not xml.contains("购买") or not xml.contains("出售") or not xml.contains("修理"):
			_fail("real NPC entry layout is missing the original service links: %s" % xml, 8)
			return
		var purchase_id := _first_event_id(xml)
		if purchase_id.is_empty() or purchase_id == "npc_goto_1":
			_fail("server did not provide an encrypted purchase event id: %s" % purchase_id, 9)
			return
		npc_panel.call("_on_meta_clicked", JSON.stringify({
			"id": purchase_id,
			"path": "",
			"args": null,
			"close": false,
		}))
		_purchase_sent = true
		return
	if not xml.contains("你想要哪种？戒指还是手镯？"):
		return
	var purchase_panel := _main.get("_extra_panel_nodes").get(PURCHASE_PANEL_PATH) as Control
	var items: Array = GameState.npc_sell.get("itemList", [])
	if purchase_panel == null or not purchase_panel.visible or items.is_empty():
		return
	if purchase_panel.position != Vector2(0.0, npc_panel.size.y):
		_fail("purchase panel did not attach below the visible NPC dialog: npc=%s purchase=%s" % [npc_panel.size, purchase_panel.position], 10)
		return
	if OS.has_environment("MIR2X_NETWORK_NPC_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_NPC_SCREENSHOT"))
		if error != OK:
			_fail("failed to save the real NPC dialog screenshot: %s" % error, 11)
			return
	print("NETWORK NPC DIALOG PASS: uid=%d items=%d event_path=%s" % [_target_uid, items.size(), dialog.get("eventPath", "")])
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to enter game", 3)
		NetworkClient.SM_ONLINEOK:
			var online := Protocol.decode_sm_online_ok(payload)
			var action: Dictionary = online.get("action", {})
			GameState.set_player_online({
				"uid": online.get("uid", 0),
				"name": online.get("name", ""),
				"gender": online.get("gender", 0),
				"job": online.get("job", 0),
				"map_uid": online.get("mapUID", 0),
				"x": action.get("x", 0),
				"y": action.get("y", 0),
				"direction": action.get("direction", 0),
			})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _find_target_npc() -> int:
	for uid_value in GameState.creatures:
		var creature: Dictionary = GameState.creatures[uid_value]
		if int(creature.get("type", 0)) == 3 and int(creature.get("npc_id", 0)) == TARGET_NPC_ID:
			return int(uid_value)
	return 0


func _first_event_id(xml: String) -> String:
	var parser := XMLParser.new()
	if parser.open_buffer(xml.to_utf8_buffer()) != OK:
		return ""
	while parser.read() == OK:
		if parser.get_node_type() != XMLParser.NODE_ELEMENT or parser.get_node_name().to_lower() != "event":
			continue
		for index in parser.get_attribute_count():
			if parser.get_attribute_name(index).to_lower() == "id":
				return parser.get_attribute_value(index)
	return ""


func _on_timeout() -> void:
	_fail("timed out target=%d pos=(%d,%d) enter=%s purchase=%s dialog=%s sell=%s" % [_target_uid, GameState.player_x, GameState.player_y, _enter_sent, _purchase_sent, GameState.npc_dialog, GameState.npc_sell], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_NPC_DIALOG_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
