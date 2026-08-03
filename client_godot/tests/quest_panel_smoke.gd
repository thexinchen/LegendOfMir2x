extends Node

const SYS_QSTFSM := "_RSVD_NAME_QST_FSM_4194347313"


func _ready() -> void:
	GameState.set_quest_list({"初入江湖": {SYS_QSTFSM: "拜访村长", "寻找药草": "已经找到一株药草"}})
	var panel := load("res://scenes/game/panels/quest.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	var slider := panel.get_node("Slider") as TextureRect
	if slider.position != Vector2(321, 148) or slider.size != Vector2(23, 26) or slider.modulate.r > 0.6:
		_fail("native quest slider mismatch: position=%s size=%s tint=%s" % [slider.position, slider.size, slider.modulate])
		return
	if _label_count(panel) != 1 or not panel.get("_folded").get("初入江湖", false):
		_fail("quest did not start folded")
		return
	panel.call("_toggle_quest", "初入江湖")
	if _label_count(panel) != 4 or panel.get("_folded").get("初入江湖", true):
		_fail("expanded quest line composition mismatch: %d" % _label_count(panel))
		return
	var long_desp := "任务内容需要自动换行。".repeat(120)
	GameState.update_quest_description("初入江湖", SYS_QSTFSM, long_desp, SYS_QSTFSM)
	var wheel := InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	wheel.pressed = true
	panel.call("_on_content_input", wheel)
	if panel.get("_scroll_value") <= 0.0 or slider.position.y <= 148.0:
		_fail("wheel scrolling did not move content and knob")
		return
	var reset_serial: int = GameState.quest_reset_serial
	GameState.set_quest_list({"重置任务": {SYS_QSTFSM: "重新开始"}})
	if GameState.quest_reset_serial != reset_serial + 1 or panel.get("_scroll_value") != 0.0 or not panel.get("_folded").get("重置任务", false):
		_fail("full quest list did not reset folding/scroll")
		return
	GameState.set_quest_list({"空状态任务": {"支线": "保留标题"}})
	GameState.update_quest_description("空状态任务", "支线", null, SYS_QSTFSM)
	if not GameState.quests.has("空状态任务") or not (GameState.quests["空状态任务"] as Dictionary).is_empty():
		_fail("child FSM removal did not preserve the original empty quest heading")
		return
	if _label_count(panel) != 1 or not panel.get("_folded").get("空状态任务", false):
		_fail("empty quest heading did not remain visible and folded")
		return
	GameState.set_quest_list({"重置任务": {SYS_QSTFSM: "重新开始"}})
	GameState.update_quest_description("重置任务", "支线", "保留项", SYS_QSTFSM)
	GameState.update_quest_description("重置任务", SYS_QSTFSM, null, SYS_QSTFSM)
	if GameState.quests.has("重置任务"):
		_fail("main FSM removal did not erase the whole quest")
		return
	var control := load("res://scenes/game/control_panel.tscn").instantiate() as Control
	add_child(control)
	await get_tree().process_frame
	control.call("start_button_blink", "Quest")
	if control.get("_button_blinks").get("Quest", -1) != 0:
		_fail("default quest button blink was not indefinite")
		return
	control.call("_on_board_button_pressed", control.get_node("Body/BoardButtons/Quest"))
	if control.get("_button_blinks").has("Quest") or control.get_node("Body/BoardButtons/Quest").modulate.a != 1.0:
		_fail("quest button click did not stop blink")
		return
	control.call("start_button_blink", "Quest", 10000)
	if int(control.get("_button_blinks").get("Quest", 0)) <= Time.get_ticks_msec():
		_fail("finite quest button blink did not get an expiry")
		return
	control.get("_button_blinks")["Quest"] = Time.get_ticks_msec() - 1
	control.call("_update_button_blinks")
	if control.get("_button_blinks").has("Quest"):
		_fail("finite quest button blink did not expire")
		return
	if OS.has_environment("MIR2X_QUEST_SCREENSHOT"):
		control.hide()
		GameState.set_quest_list({"初入江湖": {SYS_QSTFSM: "拜访村长并了解村庄最近发生的事情。", "寻找药草": "已经找到一株药草，还需要两株。"}})
		panel.call("_toggle_quest", "初入江湖")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_QUEST_SCREENSHOT"))
	print("QUEST PANEL PASS: folding, wrapping, scroll, update semantics and HUD blink")
	get_tree().quit()


func _label_count(panel: Control) -> int:
	var count := 0
	for child in panel.get_node("ContentViewport/Lines").get_children():
		if child is Label:
			count += 1
	return count


func _fail(message: String) -> void:
	push_error("QUEST_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
