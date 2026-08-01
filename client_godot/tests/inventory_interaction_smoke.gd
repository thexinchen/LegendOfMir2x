extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var potion_id := _find_type(resources, "恢复药水")
	var weapon_id := _find_type(resources, "武器")
	if potion_id == 0 or weapon_id == 0 or resources.item_types.size() < 1000:
		_fail("item type metadata incomplete")
		return
	GameState.inventory = [_item(potion_id, 1, 3), _item(weapon_id, 2, 1)]
	GameState.grabbed_item = {}
	var panel: Control = load("res://scenes/game/panels/inventory.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("ItemGrid").position != Vector2(18, 59) or panel.get_node("ItemGrid").size != Vector2(380, 380):
		_fail("inventory grid geometry mismatch")
		return
	if panel.get_node("Title").position.x + panel.get_node("Title").size.x / 2.0 != 238.0:
		_fail("inventory title center mismatch")
		return
	if panel.get_node("SliderTrack").position != Vector2(403, 56) or panel.get_node("SliderTrack").size != Vector2(22, 383):
		_fail("inventory slider hit area mismatch")
		return
	if panel.get_node("Slider").position != Vector2(403, 56) or panel.get_node("Slider").size != Vector2(16, 16):
		_fail("inventory slider thumb geometry mismatch")
		return
	if not panel.get_node("Emblem").visible or panel.get_node("Emblem").texture == null or panel.get_node("Emblem").position != Vector2(23, 14):
		_fail("inventory animated emblem missing")
		return
	var sort_button := panel.get_node("SortButton") as TextureButton
	var close_button := panel.get_node("CloseButton") as TextureButton
	if sort_button.modulate.a != 0.0 or close_button.modulate.a != 0.0 or not sort_button.tooltip_text.is_empty() or not close_button.tooltip_text.is_empty():
		_fail("inventory overlay chrome visible while idle or retained tooltip")
		return
	sort_button.mouse_entered.emit()
	if sort_button.modulate.a != 1.0 or close_button.modulate.a != 0.0:
		_fail("inventory sort hover overlay mismatch")
		return
	sort_button.mouse_exited.emit()
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	sort_button.pressed.emit()
	if AudioService.last_seff_id != AudioService.UI_CLICK_SEFF_ID:
		_fail("inventory texture button did not play original click SEFF")
		return
	panel.call("_sync_bins")
	var bins: Dictionary = panel.get("_bins")
	if bins.size() != 2:
		_fail("inventory bin count mismatch")
		return
	var potion_key := "%d:1" % potion_id
	panel.call("_grab_item", potion_key)
	if GameState.grabbed_item.get("itemID", 0) != potion_id or GameState.inventory.size() != 1:
		_fail("grab operation mismatch")
		return
	panel.call("_place_grabbed", Vector2i(5, 5))
	if not GameState.grabbed_item.is_empty() or GameState.inventory.size() != 2:
		_fail("place operation mismatch")
		return
	var no_range_wheel := InputEventMouseButton.new()
	no_range_wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	no_range_wheel.pressed = true
	panel.call("_on_grid_input", no_range_wheel)
	if panel.get("_scroll_value") != 0.0 or panel.get_node("Slider").position.y != 56.0:
		_fail("inventory wheel moved slider without a scroll range")
		return
	for index in range(120):
		var extra := _item(potion_id, 100 + index, 1)
		GameState.inventory.append(extra)
	panel.call("_repack")
	var slider_press := InputEventMouseButton.new()
	slider_press.button_index = MOUSE_BUTTON_LEFT
	slider_press.pressed = true
	slider_press.position = Vector2(11, 191)
	panel.call("_on_slider_input", slider_press)
	if panel.get_node("Slider").position.y != 240.0 or panel.get("_scroll_row") != roundi(panel.call("_max_scroll_row") * 0.5):
		_fail("inventory slider did not preserve continuous half-way position")
		return
	slider_press.position = Vector2(11, 382)
	panel.call("_on_slider_input", slider_press)
	if panel.get("_scroll_row") != panel.call("_max_scroll_row") or panel.get_node("Slider").position.y != 424.0 or panel.get_node("Slider").modulate != Color.WHITE:
		_fail("inventory slider drag-to-bottom mismatch")
		return
	var slider_release := InputEventMouseButton.new()
	slider_release.button_index = MOUSE_BUTTON_LEFT
	slider_release.pressed = false
	panel.call("_on_slider_input", slider_release)
	if panel.get_node("Slider").modulate != Color(0.5, 0.5, 0.5, 1.0):
		_fail("inventory slider idle tint mismatch")
		return
	var wheel_up := InputEventMouseButton.new()
	wheel_up.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel_up.pressed = true
	var bottom_row := int(panel.get("_scroll_row"))
	panel.call("_on_item_input", wheel_up, potion_key)
	if panel.get("_scroll_row") != bottom_row - 1:
		_fail("inventory wheel over occupied cell did not scroll")
		return
	GameState.inventory_operation = {"invOp": 3, "uid": 9876, "typeList": ["武器"]}
	panel.show()
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if panel.visible or GameState.inventory_operation.is_empty():
		_fail("inventory Escape did not preserve original operation state")
		return
	panel.show()
	close_button.pressed.emit()
	if panel.visible or not GameState.inventory_operation.is_empty():
		_fail("inventory close button did not clear operation state")
		return
	if not _test_start_inv_op_reader():
		return
	print("INVENTORY INTERACTION PASS: types=%d potion=%d weapon=%d bins=%d" % [resources.item_types.size(), potion_id, weapon_id, bins.size()])
	get_tree().quit()


func _test_start_inv_op_reader() -> bool:
	var archive := PackedByteArray([1])
	_s32(archive, 3)
	_u64(archive, 9876)
	_string(archive, "query_repair")
	_string(archive, "commit_repair")
	_u64(archive, 2)
	_string(archive, "武器")
	_string(archive, "衣服")
	archive.append(0)
	var reader := CerealReader.new(archive)
	var operation := reader.read_sd_start_inv_op()
	if not reader.valid or not reader.at_end() or operation.invOp != 3 or operation.uid != 9876 or operation.typeList != ["武器", "衣服"]:
		_fail("SDStartInvOp parse mismatch: %s" % reader.error)
		return false
	return true


func _find_type(resources: RefCounted, type_name: String) -> int:
	for item_id in resources.item_types:
		if resources.item_types[item_id] == type_name:
			return int(item_id)
	return 0


func _item(item_id: int, seq_id: int, count: int) -> Dictionary:
	return {"itemID": item_id, "seqID": seq_id, "count": count, "duration": [0, 0], "extAttrList": {}}


func _string(buf: PackedByteArray, value: String) -> void:
	var encoded := value.to_utf8_buffer()
	_u64(buf, encoded.size())
	buf.append_array(encoded)


func _s32(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 4)
	buf.encode_s32(offset, value)


func _u64(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 8)
	buf.encode_u32(offset, value & 0xFFFFFFFF)
	buf.encode_u32(offset + 4, (value >> 32) & 0xFFFFFFFF)


func _fail(message: String) -> void:
	push_error("INVENTORY_INTERACTION_SMOKE %s" % message)
	get_tree().quit(1)
