extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var ids: Array[int] = []
	for item_id in resources.item_meta:
		if not resources.item_icon(int(item_id)).is_empty():
			ids.append(int(item_id))
			if ids.size() == 13:
				break
	if ids.size() < 13:
		_fail("not enough item icons")
		return
	GameState.secured_items.clear()
	for index in range(ids.size()):
		GameState.secured_items.append({"itemID": ids[index], "seqID": index + 1, "count": index + 2})
	var panel: Control = load("res://scenes/game/panels/secured_items.tscn").instantiate()
	add_child(panel)
	panel.position = Vector2(301, 198)
	await get_tree().process_frame
	if panel.get_node("ItemGrid").get_child_count() != 12 or panel.get_node("Page").text != "第1/2页":
		_fail("first page layout mismatch")
		return
	panel.call("_change_page", 1)
	if panel.get_node("Page").text != "第2/2页":
		_fail("page navigation mismatch")
		return
	GameState.remove_secured_item(ids[12], 13)
	if GameState.secured_items.size() != 12 or panel.get_node("Page").text != "第1/1页":
		_fail("remove response state mismatch")
		return
	await get_tree().process_frame
	if OS.has_environment("MIR2X_SECURED_SCREENSHOT"):
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SECURED_SCREENSHOT"))
	print("SECURED ITEMS PASS: icons=12 pagination and removal")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("SECURED_ITEMS_SMOKE %s" % message)
	get_tree().quit(1)
