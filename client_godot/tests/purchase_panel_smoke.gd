extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var unique_id := 0
	var packable_id := 0
	for item_value in resources.item_names.keys():
		var item_id: int = item_value
		if resources.item_icon(item_id).is_empty():
			continue
		if resources.item_is_packable(item_id) and packable_id == 0:
			packable_id = item_id
		elif not resources.item_is_packable(item_id) and unique_id == 0:
			unique_id = item_id
		if unique_id and packable_id:
			break
	if unique_id == 0 or packable_id == 0:
		_fail("missing packable or unique item metadata")
		return

	var decoded := _decode_sell_archive(unique_id)
	if decoded.get("npcUID", 0) != 77 or decoded.get("list", []).size() != 1 or decoded.list[0].costList[0].count != 123:
		_fail("SDSellItemList decode mismatch: %s" % decoded)
		return

	GameState.npc_sell = {"npcUID": 77, "itemList": [unique_id, packable_id]}
	var panel: Control = load("res://scenes/game/panels/purchase.tscn").instantiate()
	add_child(panel)
	panel.call("_select_item", unique_id)
	GameState.npc_sell_detail = {"npcUID": 77, "list": [
		{"item": {"itemID": unique_id, "seqID": 11}, "costList": [{"itemID": 1, "count": 123}]},
		{"item": {"itemID": unique_id, "seqID": 12}, "costList": [{"itemID": 1, "count": 456}]},
	]}
	GameState.state_changed.emit()
	if panel.size.x != 488 or panel.get_node("Detail").get_child_count() < 5:
		_fail("unique item extension missing")
		return

	panel.call("_select_item", packable_id)
	GameState.npc_sell_detail = {"npcUID": 77, "list": [
		{"item": {"itemID": packable_id, "seqID": 0}, "costList": [{"itemID": 1, "count": 88}]},
	]}
	GameState.state_changed.emit()
	if panel.size.x != 514 or panel.get_node("Detail").get_child_count() < 3:
		_fail("packable item extension missing")
		return
	if OS.has_environment("MIR2X_PURCHASE_SCREENSHOT"):
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PURCHASE_SCREENSHOT"))
	print("PURCHASE PANEL PASS: names, icons, archive, unique and packable layouts")
	get_tree().quit()


func _decode_sell_archive(item_id: int) -> Dictionary:
	var bytes := PackedByteArray([1])
	_append_u64(bytes, 77)
	_append_u64(bytes, 1)
	_append_u32(bytes, item_id)
	_append_u32(bytes, 9)
	_append_u64(bytes, 1)
	_append_u64(bytes, 0)
	_append_u64(bytes, 0)
	_append_u64(bytes, 0)
	_append_u64(bytes, 1)
	_append_u32(bytes, 1)
	_append_u64(bytes, 123)
	bytes.append(0)
	var reader: RefCounted = CerealReader.new(bytes)
	return reader.read_sd_sell_item_list()


func _append_u32(bytes: PackedByteArray, value: int) -> void:
	for shift in [0, 8, 16, 24]:
		bytes.append((value >> shift) & 0xFF)


func _append_u64(bytes: PackedByteArray, value: int) -> void:
	_append_u32(bytes, value & 0xFFFFFFFF)
	_append_u32(bytes, value >> 32)


func _fail(message: String) -> void:
	push_error("PURCHASE_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
