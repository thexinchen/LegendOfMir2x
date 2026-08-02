extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var packable_id := 0
	var other_ids: Array[int] = []
	for item_id in resources.item_meta:
		var typed_id := int(item_id)
		if resources.item_icon(typed_id).is_empty():
			continue
		if packable_id == 0 and resources.item_is_packable(typed_id):
			packable_id = typed_id
		else:
			other_ids.append(typed_id)
		if packable_id != 0 and other_ids.size() >= 12:
			break
	if packable_id == 0 or other_ids.size() < 12:
		_fail("not enough item icons")
		return
	var ids: Array[int] = [packable_id]
	ids.append_array(other_ids.slice(0, 12))
	var secured_items: Array = []
	for index in range(ids.size()):
		secured_items.append({"itemID": ids[index], "seqID": index + 1, "count": 1234 if index == 0 else index + 2})
	GameState.set_secured_items(secured_items)
	var panel: Control = load("res://scenes/game/panels/secured_items.tscn").instantiate()
	add_child(panel)
	panel.position = Vector2(301, 198)
	await get_tree().process_frame
	if panel.get_node("ItemGrid").get_child_count() != 12 or panel.get_node("Page").text != "第1/2页":
		_fail("first page layout mismatch")
		return
	panel.call("_select_index", 3)
	panel.call("_change_page", 1)
	if panel.get_node("Page").text != "第2/2页" or panel.get("_selected_index") != 3:
		_fail("page navigation mismatch")
		return
	panel.call("_change_page", -1)
	var first_cell := panel.get_node("ItemGrid").get_child(0)
	var count_label := first_cell.get_child(0) as Label
	if count_label == null or count_label.text != "1,234" or first_cell.get_node_or_null("Hover") == null:
		_fail("count formatting/hover overlay mismatch: children=%s count=%s" % [first_cell.get_children().map(func(child): return child.name), count_label.text if count_label else "null"])
		return
	get_viewport().warp_mouse(Vector2(520, 100))
	panel.call("_show_item_tooltip", 0, secured_items[0])
	var secured_tooltip := panel.get_node("ItemTooltip") as Panel
	var secured_lines := _label_texts(secured_tooltip)
	var secured_style := secured_tooltip.get_theme_stylebox("panel") as StyleBoxFlat
	var secured_name := secured_tooltip.get_child(0) as Label
	var secured_description := secured_tooltip.get_child(1) as Label
	if not secured_tooltip.visible or secured_tooltip.size != Vector2(240, 60) or secured_lines.size() != 2 or secured_name.position != Vector2(20, 12) or secured_description.position != Vector2(20, 31) or secured_name.get_theme_font_size("font_size") != 12 or not secured_name.get_theme_font("font").resource_path.ends_with("/01_Yahei.ttf") or "数量" in "".join(secured_lines) or resources.item_type(packable_id) in "".join(secured_lines) or secured_style.border_width_left != 0:
		_fail("secured tooltip content/style mismatch: %s" % secured_lines)
		return
	if OS.has_environment("MIR2X_SECURED_TOOLTIP_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SECURED_TOOLTIP_SCREENSHOT"))
	GameState.set_secured_items(secured_items)
	if panel.get("_page") != 0 or panel.get("_selected_index") != -1:
		_fail("full list replacement did not reset page/selection")
		return
	GameState.remove_secured_item(ids[12], 13)
	if GameState.secured_items.size() != 12 or panel.get_node("Page").text != "第1/1页":
		_fail("remove response state mismatch")
		return
	var main := load("res://scenes/game/main.tscn").instantiate() as Control
	add_child(main)
	await get_tree().process_frame
	main.call("_show_secured_items", secured_items)
	var main_secured := main.call("_ensure_extra_panel", "res://scenes/game/panels/secured_items.tscn") as Control
	if not main_secured.visible or main_secured.position != Vector2.ZERO or not main.get_node("InventoryPanel").visible:
		_fail("secured/inventory open choreography mismatch")
		return
	var npc_panel := main.call("_ensure_extra_panel", "res://scenes/game/panels/npc_chat.tscn") as Control
	npc_panel.show()
	main.call("_show_secured_items", secured_items)
	if main_secured.position != Vector2(0, npc_panel.size.y):
		_fail("secured panel did not stack below NPC chat: %s" % main_secured.position)
		return
	main.hide()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_SECURED_SCREENSHOT"):
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SECURED_SCREENSHOT"))
	print("SECURED ITEMS PASS: icons=12 pagination and removal")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("SECURED_ITEMS_SMOKE %s" % message)
	get_tree().quit(1)


func _label_texts(parent: Node) -> Array[String]:
	var result: Array[String] = []
	for child in parent.get_children():
		if child is Label:
			result.append(child.text)
	return result
