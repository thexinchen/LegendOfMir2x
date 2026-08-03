extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const ITEM_ID := 6
const INITIAL_SEQ_ID := 6
const ITEM_COUNT := 10

enum Stage { WAIT_READY, WAIT_GRABBED, WAIT_EQUIPPED, WAIT_RETURNED, WAIT_PLACED }

var _main: Control
var _stage := Stage.WAIT_READY
var _inventory_received := false
var _belt_received := false
var _finished := false


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(30.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	var quick_bar := _main.get_node("QuickBar") as Control
	match _stage:
		Stage.WAIT_READY:
			if not _inventory_received or not _belt_received or not GameState.grabbed_item.is_empty() or not _belt_item(0).is_empty():
				return
			inventory_panel.show()
			if not _emit_item_click(inventory_panel, "%d:%d" % [ITEM_ID, INITIAL_SEQ_ID]):
				return
			_stage = Stage.WAIT_GRABBED
		Stage.WAIT_GRABBED:
			if int(GameState.grabbed_item.get("itemID", 0)) != ITEM_ID:
				return
			if int(quick_bar.call("activate_slot", 0, MOUSE_BUTTON_LEFT)) == 0:
				_fail("visible quick slot rejected the grabbed belt item", 7)
				return
			_stage = Stage.WAIT_EQUIPPED
		Stage.WAIT_EQUIPPED:
			if int(_belt_item(0).get("itemID", 0)) != ITEM_ID or not GameState.grabbed_item.is_empty():
				return
			inventory_panel.hide()
			quick_bar.show()
			if OS.has_environment("MIR2X_NETWORK_BELT_SCREENSHOT"):
				await RenderingServer.frame_post_draw
				var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_BELT_SCREENSHOT"))
				if error != OK:
					_fail("failed to save belt screenshot: %s" % error, 8)
					return
			quick_bar.call("activate_slot", 0, MOUSE_BUTTON_LEFT)
			_stage = Stage.WAIT_RETURNED
		Stage.WAIT_RETURNED:
			if not _belt_item(0).is_empty() or int(GameState.grabbed_item.get("itemID", 0)) != ITEM_ID:
				return
			inventory_panel.show()
			inventory_panel.call("_place_grabbed", Vector2i(5, 5))
			_stage = Stage.WAIT_PLACED
		Stage.WAIT_PLACED:
			if not GameState.grabbed_item.is_empty():
				return
			var restored := _inventory_item()
			if int(restored.get("count", 0)) != ITEM_COUNT:
				_fail("returned belt item did not restore its full stack: %s" % restored, 9)
				return
			print("NETWORK BELT PASS: item=%d:%d count=%d inventory->slot0->cursor->inventory returned_seq=%d" % [ITEM_ID, INITIAL_SEQ_ID, ITEM_COUNT, int(restored.get("seqID", 0))])
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()


func _emit_item_click(panel: Control, key: String) -> bool:
	var bin: Dictionary = panel.get("_bins").get(key, {})
	if bin.is_empty():
		return false
	var expected := Vector2(int(bin.x) * 38, (int(bin.y) - int(panel.get("_scroll_row"))) * 38)
	for child in panel.get_node("ItemGrid").get_children():
		if child is TextureButton and child.position == expected:
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_LEFT
			event.pressed = true
			child.gui_input.emit(event)
			return true
	return false


func _belt_item(slot: int) -> Dictionary:
	if slot < 0 or slot >= GameState.belt.size() or not GameState.belt[slot] is Dictionary:
		return {}
	var item: Dictionary = GameState.belt[slot]
	return {} if int(item.get("itemID", 0)) == 0 else item


func _inventory_item() -> Dictionary:
	for value in GameState.inventory:
		var item: Dictionary = value
		if int(item.get("itemID", 0)) == ITEM_ID:
			return item
	return {}


func _on_connection_changed(connected: bool, _message: String) -> void:
	if connected and NetworkClient.login("id_4", "123456") != OK:
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
			GameState.set_player_online({"uid": online.get("uid", 0), "name": online.get("name", ""), "gender": online.get("gender", 0), "job": online.get("job", 0), "map_uid": online.get("mapUID", 0), "x": action.get("x", 0), "y": action.get("y", 0), "direction": action.get("direction", 0)})
			_main = load("res://scenes/game/main.tscn").instantiate() as Control
			add_child(_main)
		NetworkClient.SM_INVENTORY:
			_inventory_received = true
		NetworkClient.SM_BELT:
			_belt_received = true
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d item=%s belt=%s grabbed=%s" % [_stage, _inventory_item(), _belt_item(0), GameState.grabbed_item], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_BELT_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
