extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var active_ids: Array[int] = []
	var passive_id := 0
	for magic_value in resources.skill_meta.keys():
		var magic_id := int(magic_value)
		var layout: PackedInt32Array = resources.skill_layout(magic_id)
		if layout.size() < 5 or not resources.magic_names.has(magic_id):
			continue
		if (layout[4] & 1) != 0:
			passive_id = magic_id
		elif active_ids.size() < 2:
			active_ids.append(magic_id)
		if passive_id != 0 and active_ids.size() == 2:
			break
	if passive_id == 0 or active_ids.size() < 2:
		_fail("active/passive magic fixtures unavailable")
		return

	GameState.learned_magic = [
		{"magicID": active_ids[0], "exp": 0},
		{"magicID": active_ids[1], "exp": 0},
		{"magicID": passive_id, "exp": 0},
	]
	GameState.magic_keys = {active_ids[1]: 120}
	GameState.chat_log.clear()
	var panel := load("res://scenes/game/panels/skill.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame

	var active_layout: PackedInt32Array = resources.skill_layout(active_ids[0])
	panel.call("_select_tab", active_layout[1])
	var base_label: String = panel.get_node("SelectionLabel").text
	panel.call("_show_magic", active_ids[0])
	var expected_label := "%s%s" % [base_label, resources.magic_names[active_ids[0]]]
	if panel.get_node("SelectionLabel").text != expected_label:
		_fail("hover footer did not use original element and magic name: %s" % panel.get_node("SelectionLabel").text)
		return
	for button_value in panel.get_node("PageViewport/LearnedSkills").get_children():
		var button := button_value as TextureButton
		if button != null and not button.tooltip_text.is_empty():
			_fail("learned magic retained invented numeric tooltip")
			return

	var bind_event := InputEventKey.new()
	bind_event.keycode = KEY_X
	bind_event.unicode = 120
	bind_event.pressed = true
	panel.call("_unhandled_key_input", bind_event)
	if GameState.magic_keys != {active_ids[0]: 120}:
		_fail("active magic key did not remain unique: %s" % GameState.magic_keys)
		return

	panel.call("_show_magic", passive_id)
	var passive_event := InputEventKey.new()
	passive_event.keycode = KEY_Y
	passive_event.unicode = 121
	passive_event.pressed = true
	panel.call("_unhandled_key_input", passive_event)
	var expected_message := "无法为被动技能设置快捷键：%s" % resources.magic_names[passive_id]
	if GameState.magic_keys.has(passive_id) or GameState.chat_log.size() != 1 or GameState.chat_log[0].type != 1 or GameState.chat_log[0].text != expected_message:
		_fail("passive binding feedback mismatch: keys=%s log=%s" % [GameState.magic_keys, GameState.chat_log])
		return

	panel.call("_hide_magic", passive_id)
	panel.show()
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if panel.visible:
		_fail("Escape did not close skill panel without a hovered magic")
		return
	print("SKILL PANEL PASS: original hover text, key binding, passive feedback and Escape")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("SKILL_PANEL_SMOKE %s" % message)
	get_tree().quit(1)
