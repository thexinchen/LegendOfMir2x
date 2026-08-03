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
	var page := panel.get_node("PageViewport/Page") as TextureRect
	if page.size != Vector2(page.texture.get_size()):
		_fail("skill page texture was stretched instead of drawn at original size: node=%s texture=%s" % [page.size, page.texture.get_size()])
		return
	var footer := panel.get_node("SelectionLabel") as Label
	if not footer.get_theme_color("font_color").is_equal_approx(Color.WHITE):
		_fail("skill footer did not use original pure-white font color: %s" % footer.get_theme_color("font_color"))
		return
	var slider_input := panel.get_node_or_null("SliderInput") as Control
	if slider_input == null or slider_input.position != Vector2(319, 67) or slider_input.size != Vector2(20, 280) or not panel.has_method("_on_slider_input"):
		_fail("original draggable slider hit region is unavailable")
		return

	var active_layout: PackedInt32Array = resources.skill_layout(active_ids[0])
	panel.call("_select_tab", active_layout[1])
	var page_skill_ids: Array[int] = []
	var max_button_reach := 0.0
	for magic_value in resources.skill_meta.keys():
		var magic_id := int(magic_value)
		var layout: PackedInt32Array = resources.skill_layout(magic_id)
		if layout.size() < 5 or layout[1] != active_layout[1]:
			continue
		var frame: Dictionary = resources.frame("proguse", layout[0])
		if frame.is_empty():
			continue
		page_skill_ids.append(magic_id)
		max_button_reach = maxf(max_button_reach, layout[3] * 65.0 + 13.0 + frame.texture.get_height() + 8.0)
	var background_height := float(page.texture.get_height())
	var page_height := clampf(max_button_reach + 10.0, minf(background_height, 329.0), maxf(background_height, 329.0))
	var expected_scroll_reach := maxf(0.0, page_height - 329.0)
	if not is_equal_approx(float(panel.get("_scroll_reach")), expected_scroll_reach):
		_fail("scroll reach did not use every original skill slot: actual=%s expected=%s" % [panel.get("_scroll_reach"), expected_scroll_reach])
		return
	if panel.get_node("PageViewport/LearnedSkills").get_child_count() != page_skill_ids.size():
		_fail("unlearned skill hover regions are missing: actual=%d expected=%d" % [panel.get_node("PageViewport/LearnedSkills").get_child_count(), page_skill_ids.size()])
		return
	var active_button := _magic_button(panel, active_ids[0])
	var active_icon: TextureRect = null
	if active_button != null:
		active_icon = active_button.get_node_or_null("Icon") as TextureRect
	var active_frame: Dictionary = resources.frame("proguse", active_layout[0])
	if active_icon == null or active_icon.position != Vector2.ZERO or active_icon.size != Vector2(active_frame.texture.get_size()) or active_button.size != active_icon.size + Vector2(8, 8):
		_fail("learned skill icon was not drawn at native size inside the original +8 hit region")
		return
	var level_overlay: Label = null
	for child_value in active_button.get_children():
		var child := child_value as Label
		if child != null and child.text == "1":
			level_overlay = child
			break
	if level_overlay == null or not level_overlay.get_theme_font("font").resource_path.ends_with("/03_MONOWIDE.ttf"):
		_fail("skill level overlay did not use the original font-03 resource")
		return
	panel.set("_scroll", 0.5)
	panel.call("_apply_scroll")
	panel.call("_select_tab", active_layout[1])
	if not is_equal_approx(float(panel.get("_scroll")), 0.5):
		_fail("reselecting current skill tab reset the original scroll position")
		return
	var drag_event := InputEventMouseButton.new()
	drag_event.button_index = MOUSE_BUTTON_LEFT
	drag_event.pressed = true
	drag_event.position = Vector2(10, 140)
	panel.call("_on_slider_input", drag_event)
	if not is_equal_approx(float(panel.get("_scroll")), 0.5):
		_fail("skill slider drag formula mismatch: %s" % panel.get("_scroll"))
		return
	var other_tab := (active_layout[1] + 1) % 8
	var selected_tab_label := footer.text
	panel.call("_show_tab_name", other_tab)
	if not panel.has_method("_hide_tab_name"):
		_fail("skill tab hover cannot restore the selected footer on mouse exit")
		return
	panel.call("_hide_tab_name", other_tab)
	if footer.text != selected_tab_label:
		_fail("skill tab mouse exit did not restore selected element: %s" % footer.text)
		return
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
	active_button = _magic_button(panel, active_ids[0])
	var key_overlays: Array[Label] = []
	for child_value in active_button.get_children():
		var child := child_value as Label
		if child != null and child.text == "X":
			key_overlays.append(child)
	if key_overlays.size() != 2 or key_overlays[0].position != Vector2(4, 4) or key_overlays[1].position != Vector2(2, 2):
		_fail("magic key did not preserve original offset shadow and foreground draws")
		return
	if not key_overlays[0].get_theme_color("font_color").is_equal_approx(Color(0, 0, 0, 224.0 / 255.0)) or not key_overlays[1].get_theme_color("font_color").is_equal_approx(Color(1, 128.0 / 255.0, 0, 224.0 / 255.0)):
		_fail("magic key overlay colors do not match original alpha/color")
		return
	if not key_overlays.all(func(label: Label): return label.get_theme_font("font").resource_path.ends_with("/03_MONOWIDE.ttf")):
		_fail("magic key overlays did not use the original font-03 resource")
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
	if OS.has_environment("MIR2X_SKILL_SCREENSHOT"):
		panel.call("_select_tab", active_layout[1])
		panel.set("_scroll", 0.5)
		panel.call("_apply_scroll")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SKILL_SCREENSHOT"))
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


func _magic_button(panel: Control, magic_id: int) -> TextureButton:
	for child_value in panel.get_node("PageViewport/LearnedSkills").get_children():
		var button := child_value as TextureButton
		if button != null and int(button.get_meta("magic_id", 0)) == magic_id:
			return button
	return null
