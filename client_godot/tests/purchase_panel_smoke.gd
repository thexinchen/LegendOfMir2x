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
		var item_icon: Dictionary = resources.item_icon(item_id)
		if item_icon.is_empty():
			continue
		icon_ids.append(item_id)
		if resources.item_type(item_id) == "金币" and gold_id == 0:
			gold_id = item_id
		var texture_size: Vector2 = item_icon.texture.get_size()
		var original_size_icon := texture_size.x <= 38.0 and texture_size.y <= 38.0
		if resources.item_is_packable(item_id) and packable_id == 0 and original_size_icon:
			packable_id = item_id
		elif not resources.item_is_packable(item_id) and unique_id == 0 and original_size_icon:
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

	var goods_ids: Array[int] = [unique_id, packable_id]
	for item_id in icon_ids:
		if item_id not in goods_ids:
			goods_ids.append(item_id)
	GameState.set_npc_sell({"npcUID": 77, "itemList": goods_ids})
	var panel: Control = load("res://scenes/game/panels/purchase.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	var goods := panel.get_node("GoodsList")
	if goods.position != Vector2(19, 15) or goods.get_child_count() != 4 or goods.get_child(1).position.y != 42:
		_fail("goods geometry mismatch")
		return
	var goods_selection := goods.get_child(0).get_child(0) as ColorRect
	if goods_selection == null or goods_selection.color != Color(1, 1, 1, 64.0 / 255.0):
		_fail("goods selection layer diverged from original byte alpha 64: %s" % [goods_selection.color if goods_selection else Color.TRANSPARENT])
		return
	var first_goods_icon := goods.get_child(0).get_child(2) as TextureRect
	var first_goods_texture: Texture2D = resources.item_icon(unique_id).texture
	if first_goods_icon == null or first_goods_icon.size != first_goods_texture.get_size() or first_goods_icon.position != (Vector2(38, 38) - first_goods_icon.size) / 2.0:
		_fail("purchase icon was enlarged instead of preserving the original client size")
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
	var unique_button := _texture_button_at(panel.get_node("Detail"), Vector2(313, 41))
	var unique_icon := unique_button.get_node_or_null("Icon") as TextureRect if unique_button != null else null
	var unique_texture: Texture2D = resources.item_icon(unique_id).texture
	if unique_icon == null or unique_icon.size != unique_texture.get_size() or unique_icon.position != (unique_button.size - unique_icon.size) / 2.0:
		_fail("unique purchase icon was enlarged instead of preserving the original client size")
		return
	if not _has_label_text(panel.get_node("Detail"), "1,234") or _count_type(panel.get_node("Detail"), "ColorRect") < 12:
		_fail("unique price or overlay mismatch")
		return
	var original_overlay_alpha := 96.0 / 255.0
	var hover_overlay := _color_rect_at(panel.get_node("Detail"), Vector2(313, 41))
	if hover_overlay == null or hover_overlay.color != Color(1, 1, 1, original_overlay_alpha):
		_fail("unique hover overlay diverged from original byte alpha 96: %s" % [hover_overlay.color if hover_overlay else Color.TRANSPARENT])
		return
	var page_label := _find_label_text(panel.get_node("Detail"), "第1/2页")
	if page_label == null or page_label.position != Vector2(389, 16):
		_fail("unique page label does not match the original compact text and position")
		return
	get_viewport().warp_mouse(Vector2(520, 100))
	panel.call("_show_item_tooltip", 0, unique_list[0])
	var purchase_tooltip := panel.get_node("ItemTooltip") as Panel
	var purchase_lines := _label_texts(purchase_tooltip)
	var purchase_style := purchase_tooltip.get_theme_stylebox("panel") as StyleBoxFlat
	var purchase_tooltip_label := purchase_tooltip.get_child(0) as Label
	if not purchase_tooltip.visible or purchase_tooltip_label.position != Vector2(10, 10) or purchase_tooltip_label.get_theme_font_size("font_size") != 12 or not purchase_tooltip_label.get_theme_font("font").resource_path.ends_with("/01_Yahei.ttf") or not purchase_lines.any(func(line): return "【售价】1234" in line) or purchase_style.border_width_left != 0 or purchase_style.corner_radius_top_left != 0:
		_fail("unique purchase tooltip content/style mismatch: %s" % purchase_lines)
		return
	panel.set("_detail_selected", 0)
	panel.call("_refresh_detail")
	if not purchase_tooltip.visible or panel.get("_tooltip_index") != 0:
		_fail("purchase tooltip did not survive detail refresh")
		return
	var selected_overlay := _color_rect_at(panel.get_node("Detail"), Vector2(313, 41))
	if selected_overlay == null or selected_overlay.color != Color(0, 0, 1, original_overlay_alpha) or not selected_overlay.visible:
		_fail("unique selected overlay diverged from original byte alpha 96: %s visible=%s" % [selected_overlay.color if selected_overlay else Color.TRANSPARENT, selected_overlay.visible if selected_overlay else false])
		return
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
	var packable_price_label := _find_label_text(panel.get_node("Detail"), "1,288 金币")
	if panel.size.x != 514 or panel.get_node("Detail").get_child_count() < 3 or packable_price_label == null or packable_price_label.get_theme_font_size("font_size") != 13:
		_fail("packable item extension missing")
		return
	var packable_icon := panel.get_node_or_null("Detail/Icon") as TextureRect
	var packable_texture: Texture2D = resources.item_icon(packable_id).texture
	if packable_icon == null or packable_icon.size != packable_texture.get_size() or packable_icon.position != Vector2(303, 16) + (Vector2(38, 38) - packable_icon.size) / 2.0:
		_fail("packable purchase icon was enlarged instead of preserving the original client size")
		return
	if panel.get_node("ItemTooltip").visible:
		_fail("packable extension unexpectedly exposes unique-item tooltip")
		return
	if OS.has_environment("MIR2X_PURCHASE_SCREENSHOT"):
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PURCHASE_SCREENSHOT"))

	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	var packable_name: String = resources.item_name(packable_id)
	main.call("_on_purchase_quantity_requested", 88, packable_id, packable_name)
	var input_panel := main.call("_ensure_extra_panel", "res://scenes/game/panels/input_string.tscn") as Control
	if input_panel.get_node("Title").text != "请输入你要购买%s的数量" % packable_name:
		_fail("purchase quantity prompt diverged from original wording: %s" % input_panel.get_node("Title").text)
		return
	GameState.chat_log.clear()
	main.call("_on_input_committed", "0")
	if GameState.chat_log.size() != 1 or GameState.chat_log[0].get("text", "") != "无效输入:0":
		_fail("invalid purchase count feedback diverged from original wording: %s" % GameState.chat_log)
		return
	main.call("_show_purchase", {"npcUID": 91, "itemList": [packable_id]})
	var main_purchase := main.call("_ensure_extra_panel", "res://scenes/game/panels/purchase.tscn") as Control
	if not main_purchase.visible or main_purchase.position != Vector2.ZERO:
		_fail("purchase open choreography mismatch")
		return
	var inventory_panel := main.get_node("InventoryPanel") as Control
	var inventory_visible := inventory_panel.visible
	var inventory_hotkey := InputEventKey.new()
	inventory_hotkey.keycode = KEY_B
	inventory_hotkey.pressed = true
	main.call("_unhandled_input", inventory_hotkey)
	if inventory_panel.visible != inventory_visible:
		_fail("visible purchase panel leaked the inventory hotkey to the world UI")
		return
	var npc_panel := main.call("_ensure_extra_panel", "res://scenes/game/panels/npc_chat.tscn") as Control
	npc_panel.show()
	main.call("_show_purchase", {"npcUID": 92, "itemList": [packable_id]})
	if main_purchase.position != Vector2(0, npc_panel.size.y):
		_fail("purchase panel did not stack below NPC chat")
		return
	GameState.chat_log.clear()
	GameState.npc_sell_detail = {"npcUID": 92, "list": [
		{"item": {"itemID": unique_id, "seqID": 33}, "costList": [{"itemID": gold_id, "count": 1234}]},
	]}
	GameState.state_changed.emit()
	main.call("_on_server_message", NetworkClient.SM_BUYSUCCEED, _buy_succeed_payload(92, unique_id, 33))
	if not GameState.npc_sell_detail.get("list", []).is_empty() or main_purchase.size != Vector2(290, 224) or not GameState.chat_log.is_empty():
		_fail("buy confirmation did not silently remove and refresh the unique offer")
		return
	main.hide()
	print("PURCHASE PANEL PASS: reset, geometry, slider, paging, prices, layouts and NPC choreography")
	get_tree().quit()


func _has_label_text(parent: Node, wanted: String) -> bool:
	for child in parent.get_children():
		if child is Label and child.text == wanted:
			return true
	return false


func _find_label_text(parent: Node, wanted: String) -> Label:
	for child in parent.get_children():
		if child is Label and child.text == wanted:
			return child
	return null


func _label_texts(parent: Node) -> Array[String]:
	var result: Array[String] = []
	for child in parent.get_children():
		if child is Label:
			result.append(child.text)
	return result


func _count_type(parent: Node, type_name: String) -> int:
	var count := 0
	for child in parent.get_children():
		if child.get_class() == type_name:
			count += 1
	return count


func _texture_button_at(parent: Node, position: Vector2) -> TextureButton:
	for child in parent.get_children():
		if child is TextureButton and child.position == position:
			return child
	return null


func _color_rect_at(parent: Node, position: Vector2) -> ColorRect:
	for child in parent.get_children():
		if child is ColorRect and child.position == position:
			return child
	return null


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


func _buy_succeed_payload(npc_uid: int, item_id: int, seq_id: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(16)
	payload.encode_u64(0, npc_uid)
	payload.encode_u32(8, item_id)
	payload.encode_u32(12, seq_id)
	return payload


func _fail(message: String) -> void:
	push_error("PURCHASE_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
