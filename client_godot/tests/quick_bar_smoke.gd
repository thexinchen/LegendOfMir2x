extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const QuickBarScript = preload("res://scripts/game/quick_bar.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var potion_id := _find_type(resources, "恢复药水")
	var scroll_id := _find_type(resources, "传送卷轴")
	var non_belt_id := _find_non_belt(resources)
	if potion_id == 0 or scroll_id == 0 or non_belt_id == 0:
		_fail("required item types unavailable: %d %d %d" % [potion_id, scroll_id, non_belt_id])
		return

	GameState.inventory.clear()
	GameState.grabbed_item = {}
	GameState.belt.resize(6)
	GameState.belt.fill(null)
	GameState.belt[0] = {"itemID": potion_id, "seqID": 101, "count": 3}
	var main: Control = load("res://scenes/game/main.tscn").instantiate()
	add_child(main)
	await get_tree().process_frame
	var quick_bar: Control = main.get_node("%QuickBar")
	quick_bar.show()
	GameState.state_changed.emit()
	await get_tree().process_frame

	if quick_bar.position != Vector2(0, 400):
		_fail("original quick-bar placement mismatch: %s" % quick_bar.position)
		return
	var slot0 := quick_bar.get_node("Slots/Slot0")
	if not slot0.has_node("Icon") or not slot0.has_node("Count") or slot0.get_node("Count").text != "3":
		_fail("belt icon/count did not render")
		return
	quick_bar.call("_set_hovered_slot", 0)
	if not quick_bar.get_node("Hover").visible or quick_bar.get_node("Hover").position != Vector2(17, 6):
		_fail("original slot hover overlay mismatch")
		return
	if quick_bar.call("activate_slot", 0, MOUSE_BUTTON_RIGHT) != QuickBarScript.ACTION_CONSUME:
		_fail("right-click consume route mismatch")
		return
	var key_actions: Array = []
	quick_bar.slot_action_requested.connect(func(action: int, slot: int): key_actions.append([action, slot]))
	var key_event := InputEventKey.new()
	key_event.keycode = KEY_1
	key_event.pressed = true
	main.call("_unhandled_input", key_event)
	if key_actions.is_empty() or key_actions.back() != [QuickBarScript.ACTION_CONSUME, 0]:
		_fail("number-key consume route mismatch: %s" % [key_actions])
		return
	GameState.grabbed_item = {"itemID": scroll_id, "seqID": 202, "count": 1}
	if quick_bar.call("activate_slot", 1, MOUSE_BUTTON_LEFT) != QuickBarScript.ACTION_EQUIP:
		_fail("beltable grabbed item did not request equip")
		return
	GameState.grabbed_item = {"itemID": non_belt_id, "seqID": 303, "count": 1}
	if quick_bar.call("activate_slot", 1, MOUSE_BUTTON_LEFT) != QuickBarScript.ACTION_RETURN or not GameState.grabbed_item.is_empty() or GameState.inventory.is_empty():
		_fail("non-belt item did not return to inventory")
		return
	GameState.grabbed_item = {}
	if quick_bar.call("activate_slot", 0, MOUSE_BUTTON_LEFT) != QuickBarScript.ACTION_GRAB:
		_fail("occupied belt slot did not request grab")
		return

	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = Vector2(20, 10)
	quick_bar.call("_on_bar_input", press)
	var motion := InputEventMouseMotion.new()
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	motion.position = Vector2(70, 0)
	motion.relative = Vector2(50, -10)
	quick_bar.call("_on_bar_input", motion)
	if quick_bar.position != Vector2(50, 390):
		_fail("quick-bar drag mismatch: %s" % quick_bar.position)
		return
	motion.position = Vector2(2000, 2000)
	motion.relative = Vector2(2000, 2000)
	quick_bar.call("_on_bar_input", motion)
	if quick_bar.position.x > 518 or quick_bar.position.y > 557:
		_fail("quick-bar drag was not clamped: %s" % quick_bar.position)
		return

	GameState.grabbed_item = {"itemID": potion_id, "seqID": 404, "count": 1}
	GameState.state_changed.emit()
	await get_tree().process_frame
	var grabbed_icon: TextureRect = main.get_node("GrabbedItemIcon")
	if not grabbed_icon.visible or grabbed_icon.texture == null or grabbed_icon.size.x <= 0 or grabbed_icon.size.y <= 0:
		_fail("grabbed item cursor icon did not get a drawable texture/size")
		return
	quick_bar.position = Vector2(0, 400)
	if OS.has_environment("MIR2X_QUICK_BAR_SCREENSHOT"):
		quick_bar.call("_set_hovered_slot", 0)
		main.set_process(false)
		main.get_node("GrabbedItemIcon").position = Vector2(382, 232)
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_QUICK_BAR_SCREENSHOT"))
	print("QUICK BAR PASS: six slots, icons, count, equip/grab/consume, non-belt return, drag and cursor feedback")
	get_tree().quit()


func _find_type(resources: RefCounted, wanted: String) -> int:
	for item_id in resources.item_types:
		if resources.item_type(item_id) == wanted:
			return item_id
	return 0


func _find_non_belt(resources: RefCounted) -> int:
	for item_id in resources.item_types:
		if resources.item_type(item_id) not in ["恢复药水", "传送卷轴"] and not resources.item_icon(item_id).is_empty():
			return item_id
	return 0


func _fail(message: String) -> void:
	push_error("QUICK_BAR_SMOKE %s" % message)
	get_tree().quit(1)
