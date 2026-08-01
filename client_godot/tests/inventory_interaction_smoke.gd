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
