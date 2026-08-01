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
	var gold_id := 0
	var icon_ids: Array[int] = []
	for item_value in resources.item_names.keys():
		var item_id: int = item_value
		if resources.item_icon(item_id).is_empty():
			continue
		icon_ids.append(item_id)
		if resources.item_type(item_id) == "金币" and gold_id == 0:
			gold_id = item_id
		if resources.item_is_packable(item_id) and packable_id == 0:
			packable_id = item_id
		elif not resources.item_is_packable(item_id) and unique_id == 0:
			unique_id = item_id
		if unique_id and packable_id and gold_id and icon_ids.size() >= 6:
			break
	if unique_id == 0 or packable_id == 0 or gold_id == 0 or icon_ids.size() < 6:
		_fail("missing packable, unique, gold, or icon metadata")
		return

	var decoded := _decode_sell_archive(unique_id)
	if decoded.get("npcUID", 0) != 77 or decoded.get("list", []).size() != 1 or decoded.list[0].costList[0].count != 123:
		_fail("SDSellItemList decode mismatch: %s" % decoded)
		return

	GameState.set_npc_sell({"npcUID": 77, "itemList": icon_ids})
	var panel: Control = load("res://scenes/game/panels/purchase.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	var goods := panel.get_node("GoodsList")
	if goods.position != Vector2(19, 15) or goods.get_child_count() != 4 or goods.get_child(1).position.y != 42:
		_fail("goods geometry mismatch")
		return
	var slider_event := InputEventMouseButton.new()
	slider_event.button_index = MOUSE_BUTTON_LEFT
	slider_event.pressed = true
	slider_event.position = Vector2(10, 132)
	panel.call("_on_slider_input", slider_event)
	if panel.get("_scroll") != 1.0 or panel.get_node("Slider").position.y != 143:
		_fail("slider drag mismatch")
		return

	panel.call("_select_item", 0)
	var unique_list: Array = []
	for index in range(13):
		unique_list.append({
			"item": {"itemID": unique_id, "seqID": index + 11},
			"costList": [{"itemID": packable_id, "count": 999}, {"itemID": gold_id, "count": 1234 + index}],
		})
	GameState.npc_sell_detail = {"npcUID": 77, "list": unique_list}
	GameState.state_changed.emit()
	if panel.size.x != 488 or panel.get_node("Detail").get_child_count() < 30 or panel.call("_gold_price", unique_list[0]) != 1234:
		_fail("unique item extension missing")
		return
	if not _has_label_text(panel.get_node("Detail"), "1,234") or _count_type(panel.get_node("Detail"), "ColorRect") < 12:
		_fail("unique price or overlay mismatch")
		return
	panel.set("_detail_selected", 0)
	panel.call("_refresh_detail")
	if OS.has_environment("MIR2X_PURCHASE_UNIQUE_SCREENSHOT"):
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PURCHASE_UNIQUE_SCREENSHOT"))
	var page_event := InputEventMouseButton.new()
	page_event.button_index = MOUSE_BUTTON_WHEEL_DOWN
	page_event.pressed = true
	panel.call("_on_detail_grid_input", page_event)
	if panel.get("_detail_page") != 1:
		_fail("detail wheel paging mismatch")
		return

	GameState.set_npc_sell({"npcUID": 88, "itemList": [packable_id]})
	if panel.get("_scroll") != 0.0 or panel.get("_selected_index") != 0 or panel.get("_detail_page") != 0 or not GameState.npc_sell_detail.is_empty():
		_fail("new shop did not reset transient state")
		return

	GameState.npc_sell_detail = {"npcUID": 88, "list": [
		{"item": {"itemID": packable_id, "seqID": 0}, "costList": [{"itemID": gold_id, "count": 1288}]},
	]}
	GameState.state_changed.emit()
	if panel.size.x != 514 or panel.get_node("Detail").get_child_count() < 3 or not _has_label_text(panel.get_node("Detail"), "1,288 金币"):
		_fail("packable item extension missing")
		return
	if OS.has_environment("MIR2X_PURCHASE_SCREENSHOT"):
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PURCHASE_SCREENSHOT"))

	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	main.call("_show_purchase", {"npcUID": 91, "itemList": [packable_id]})
	var main_purchase := main.call("_ensure_extra_panel", "res://scenes/game/panels/purchase.tscn") as Control
	if not main_purchase.visible or main_purchase.position != Vector2.ZERO:
		_fail("purchase open choreography mismatch")
		return
	var npc_panel := main.call("_ensure_extra_panel", "res://scenes/game/panels/npc_chat.tscn") as Control
	npc_panel.show()
	main.call("_show_purchase", {"npcUID": 92, "itemList": [packable_id]})
	if main_purchase.position != Vector2(0, npc_panel.size.y):
		_fail("purchase panel did not stack below NPC chat")
		return
	main.hide()
	print("PURCHASE PANEL PASS: reset, geometry, slider, paging, prices, layouts and NPC choreography")
	get_tree().quit()


func _has_label_text(parent: Node, wanted: String) -> bool:
	for child in parent.get_children():
		if child is Label and child.text == wanted:
			return true
	return false


func _count_type(parent: Node, type_name: String) -> int:
	var count := 0
	for child in parent.get_children():
		if child.get_class() == type_name:
			count += 1
	return count


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
