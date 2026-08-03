extends Node

const Protocol = preload("res://scripts/network/protocol.gd")

const FIRST_ITEM_ID := 826
const REPLACEMENT_ITEM_ID := 824
const WEAPON_LOCATION := 3
const WEAPON_SLOT_POSITION := Vector2(22, 22)

enum Stage { WAIT_READY, WAIT_FIRST_EQUIPPED, WAIT_REPLACED, WAIT_RETURNED, WAIT_GRID_VISIBLE, WAIT_PLACED }

var _main: Control
var _stage := Stage.WAIT_READY
var _inventory_received := false
var _finished := false
var _first_seq_id := 0
var _replacement_seq_id := 0
var _returned_replacement_seq_id := 0


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(30.0).timeout.connect(_on_timeout)


func _process(_delta: float) -> void:
	if _finished or _main == null:
		return
	var inventory_panel := _main.get_node("InventoryPanel") as Control
	var player_state_panel := _main.get_node("PlayerStatePanel") as Control
	match _stage:
		Stage.WAIT_READY:
			var first_item := _inventory_item(FIRST_ITEM_ID)
			var replacement_item := _inventory_item(REPLACEMENT_ITEM_ID)
			if not _inventory_received or first_item.is_empty() or replacement_item.is_empty() or _inventory_weapons().size() != 2 or not GameState.grabbed_item.is_empty() or not _wear_item().is_empty():
				return
			_first_seq_id = int(first_item.get("seqID", 0))
			_replacement_seq_id = int(replacement_item.get("seqID", 0))
			inventory_panel.show()
			if not _emit_inventory_item_click(inventory_panel, FIRST_ITEM_ID, _first_seq_id):
				return
			_stage = Stage.WAIT_FIRST_EQUIPPED
		Stage.WAIT_FIRST_EQUIPPED:
			if int(_wear_item().get("itemID", 0)) != FIRST_ITEM_ID or int(_wear_item().get("seqID", 0)) != _first_seq_id or not _inventory_item(FIRST_ITEM_ID).is_empty() or _inventory_item(REPLACEMENT_ITEM_ID).is_empty():
				return
			if not _emit_inventory_item_click(inventory_panel, REPLACEMENT_ITEM_ID, _replacement_seq_id):
				return
			_stage = Stage.WAIT_REPLACED
		Stage.WAIT_REPLACED:
			var displaced := _inventory_item(FIRST_ITEM_ID)
			if int(_wear_item().get("itemID", 0)) != REPLACEMENT_ITEM_ID or int(_wear_item().get("seqID", 0)) != _replacement_seq_id or displaced.is_empty() or _inventory_weapons().size() != 1:
				return
			if int(displaced.get("seqID", 0)) != _first_seq_id:
				_fail("occupied replacement did not preserve the displaced weapon: %s" % [displaced], 7)
				return
			inventory_panel.hide()
			player_state_panel.show()
			var weapon_slot := _weapon_slot(player_state_panel)
			if weapon_slot == null:
				return
			if OS.has_environment("MIR2X_NETWORK_WEAR_REPLACE_SCREENSHOT"):
				await RenderingServer.frame_post_draw
				var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NETWORK_WEAR_REPLACE_SCREENSHOT"))
				if error != OK:
					_fail("failed to save occupied replacement screenshot: %s" % error, 8)
					return
			weapon_slot.pressed.emit()
			_stage = Stage.WAIT_RETURNED
		Stage.WAIT_RETURNED:
			if not _wear_item().is_empty() or int(GameState.grabbed_item.get("itemID", 0)) != REPLACEMENT_ITEM_ID or int(GameState.grabbed_item.get("seqID", 0)) <= 0 or _inventory_weapons().size() != 1:
				return
			_returned_replacement_seq_id = int(GameState.grabbed_item.get("seqID", 0))
			player_state_panel.hide()
			inventory_panel.show()
			_stage = Stage.WAIT_GRID_VISIBLE
		Stage.WAIT_GRID_VISIBLE:
			var item_grid := inventory_panel.get_node("ItemGrid") as Control
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_LEFT
			event.pressed = true
			event.position = Vector2(5 * 38 + 1, 5 * 38 + 1)
			item_grid.gui_input.emit(event)
			_stage = Stage.WAIT_PLACED
		Stage.WAIT_PLACED:
			var restored := _inventory_weapons()
			if not GameState.grabbed_item.is_empty() or restored.size() != 2:
				return
			for item in restored:
				if int(item.get("count", 0)) != 1:
					_fail("restored weapon has invalid count: %s" % [item], 9)
					return
			var first_restored := _inventory_item(FIRST_ITEM_ID)
			var replacement_restored := _inventory_item(REPLACEMENT_ITEM_ID)
			if int(first_restored.get("seqID", 0)) != _first_seq_id or int(replacement_restored.get("seqID", 0)) != _returned_replacement_seq_id or not _wear_item().is_empty():
				_fail("weapon replacement closure lost or changed an item: %s" % [restored], 10)
				return
			print("NETWORK WEAR REPLACE PASS: first=%d:%d replacement=%d:%d->%d" % [FIRST_ITEM_ID, _first_seq_id, REPLACEMENT_ITEM_ID, _replacement_seq_id, _returned_replacement_seq_id])
			_finished = true
			NetworkClient.disconnect_from_server()
			get_tree().quit()


func _emit_inventory_item_click(panel: Control, item_id: int, seq_id: int) -> bool:
	var key := "%d:%d" % [item_id, seq_id]
	var bin: Dictionary = panel.get("_bins").get(key, {})
	if bin.is_empty():
		return false
	var expected := Vector2(int(bin.x) * 38, (int(bin.y) - int(panel.get("_scroll_row"))) * 38)
	for child in panel.get_node("ItemGrid").get_children():
		if child is TextureButton and child.position == expected:
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_RIGHT
			event.pressed = true
			child.gui_input.emit(event)
			return true
	return false


func _weapon_slot(panel: Control) -> TextureButton:
	for child in panel.get_node("EquipmentSlots").get_children():
		if child is TextureButton and child.position == WEAPON_SLOT_POSITION:
			return child
	return null


func _wear_item() -> Dictionary:
	var item: Dictionary = GameState.wear.get(WEAPON_LOCATION, {})
	return {} if int(item.get("itemID", 0)) == 0 else item


func _inventory_weapons() -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for value in GameState.inventory:
		var item: Dictionary = value
		if int(item.get("itemID", 0)) in [FIRST_ITEM_ID, REPLACEMENT_ITEM_ID]:
			result.append(item)
	return result


func _inventory_item(item_id: int) -> Dictionary:
	for item in _inventory_weapons():
		if int(item.get("itemID", 0)) == item_id:
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
		NetworkClient.SM_LOGINERROR, NetworkClient.SM_ONLINEERROR:
			_fail("server rejected login or online", 4)


func _on_timeout() -> void:
	_fail("timed out stage=%d inventory=%s wear=%s grabbed=%s" % [_stage, _inventory_weapons(), _wear_item(), GameState.grabbed_item], 6)


func _fail(message: String, code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_WEAR_REPLACE_SMOKE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(code)
