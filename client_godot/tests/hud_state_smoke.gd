extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")
const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")
const WorldResourceScript = preload("res://scripts/game/world_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default() or resources.buff_meta.is_empty():
		_fail("HUD metadata unavailable")
		return
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.player_hp = 75
	GameState.player_hp_max = 100
	GameState.player_mp = 50
	GameState.player_mp_max = 100
	GameState.update_exp(1100)
	var weighted_item := 0
	for item_id in resources.item_attributes:
		if resources.item_weight(item_id) > 0:
			weighted_item = item_id
			break
	GameState.inventory = [{"itemID": weighted_item, "seqID": 1, "count": 1, "extAttrList": {}}]
	GameState.wear = {}
	GameState.buff_list = [resources.buff_meta.keys()[0]]
	GameState.chat_log = []
	for index in range(GameState.CHAT_LOG_MAX + 1):
		GameState.add_chat_log("历史消息 %03d" % index)
	if GameState.chat_log.size() != 200 or GameState.chat_log[0].text != "历史消息 001":
		_fail("chat history did not retain the original 200-line capacity")
		return
	GameState.chat_log = []
	GameState.add_chat_log("普通消息", 0)
	GameState.add_chat_log("获得物品", 1)
	GameState.add_chat_log("广播消息", 2)
	GameState.add_chat_log("错误消息", 3)
	var expected_log_colors := [
		Color8(255, 255, 255, 255),
		Color8(0, 255, 0, 255),
		Color8(64, 128, 255, 255),
		Color8(255, 64, 64, 255),
	]
	for index in expected_log_colors.size():
		if GameState.chat_log[index].color != expected_log_colors[index]:
			_fail("chat log color %d diverged from original 8-bit value: %s" % [index, GameState.chat_log[index].color])
			return
	for index in range(8):
		GameState.add_chat_log("滚动消息 %02d" % index, index % 4)
	GameState.add_chat_log("无效的请求。", 0, Color(0.0, 0.5, 0.0, 1.0))
	var panel: Control = load("res://scenes/game/control_panel.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	var face := panel.get_node("%Face") as TextureRect
	if not _face_uses_original_crop(face):
		_fail("player face did not preserve the original right-edge crop")
		return
	if panel.get_node("%BuffContainer").get_child_count() != 1:
		_fail("compact self buff missing")
		return
	var monster_id: int = resources.monster_meta.keys()[0]
	panel.call("_apply_focus_hud", {"uid": 101, "type": 1, "monster_id": monster_id, "hp": 25, "hp_max": 100, "buffs": GameState.buff_list})
	if absf(panel.get_node("%FaceHealth").size.x - 20.5) > 0.01 or not _face_uses_original_crop(face):
		_fail("focused monster portrait or health mismatch")
		return
	if panel.get_node("%BuffContainer").get_child_count() != 1:
		_fail("focused monster buff missing")
		return
	var renderer: Control = WorldRendererScript.new()
	renderer.game_state = GameState
	GameState.team_leader = GameState.player_uid
	GameState.team_members = [{"uid": GameState.player_uid}]
	if not renderer.has_method("_team_leader_marker"):
		_fail("local team-leader marker is missing")
		return
	var leader_marker: Dictionary = renderer.call("_team_leader_marker", GameState.player_uid, 100, 200)
	if leader_marker.get("text", "") != "我是队长" or leader_marker.get("font_size", 0) != 15 or leader_marker.get("color", Color()) != Color.YELLOW or leader_marker.get("baseline", Vector2.ZERO) != Vector2(100, 200 + leader_marker.font.get_ascent(15)):
		_fail("local team-leader marker did not match original font-11/15px yellow grid anchor: %s" % leader_marker)
		return
	if not (renderer.call("_team_leader_marker", GameState.player_uid + 1, 100, 200) as Dictionary).is_empty():
		_fail("remote hero incorrectly received the MyHero-only leader marker")
		return
	renderer._actor_target_rects = {
		GameState.player_uid: {"rect": Rect2(10, 10, 40, 40), "map_y": 99},
		101: {"rect": Rect2(10, 10, 40, 40), "map_y": 100},
		102: {"rect": Rect2(10, 10, 40, 40), "map_y": 101},
	}
	if renderer.focus_uid_at_screen(Vector2(20, 20)) != 102:
		_fail("focus overlap did not select greatest map Y")
		return
	renderer._actor_target_rects.erase(102)
	if renderer.focus_uid_at_screen(Vector2(20, 20)) != 101:
		_fail("focus query did not exclude self")
		return
	renderer.free()
	if absf(panel.get_node("%Experience").value - GameState.level_ratio() * 100.0) > 0.01:
		_fail("experience meter mismatch: panel=%s expected=%s" % [panel.get_node("%Experience").value, GameState.level_ratio() * 100.0])
		return
	var combat := CombatCalculatorScript.calculate(GameState, resources)
	var expected_load := float(resources.item_weight(weighted_item)) / float(combat.load[2]) * 100.0
	if absf(panel.get_node("%Load").value - expected_load) > 0.01:
		_fail("load meter mismatch: panel=%s expected=%s" % [panel.get_node("%Load").value, expected_load])
		return
	var chat_background: ColorRect = panel.get_node("%ChatBackground")
	var chat_log: RichTextLabel = panel.get_node("%ChatLog")
	var command: LineEdit = panel.get_node("%Command")
	if chat_background.color != Color.BLACK:
		_fail("chat background mismatch: %s" % chat_background.color)
		return
	var parsed_chat := chat_log.get_parsed_text()
	if parsed_chat.find("普通消息") < 0 or parsed_chat.find("获得物品") < 0 or parsed_chat.find("广播消息") < 0 or parsed_chat.find("错误消息") < 0:
		_fail("chat messages mismatch: %s" % parsed_chat)
		return
	if GameState.chat_log[-1].background_color != Color(0.0, 0.5, 0.0, 1.0) or parsed_chat.find("无效的请求。") < 0:
		_fail("friend notice background metadata or rendered text mismatch")
		return
	if command.get_theme_font_size("font_size") != 15:
		_fail("command font size mismatch")
		return
	if chat_log.get_theme_font_size("normal_font_size") != 15 or chat_log.get_theme_font("normal_font") == null:
		_fail("chat font mismatch")
		return
	var level_label := panel.get_node("%Level") as Label
	var ac_value := panel.get_node("%ACValue") as Label
	var dc_value := panel.get_node("%DCValue") as Label
	if level_label.get_theme_font("font").resource_path != "res://assets/font/00_SIMSUN.ttf" or level_label.get_theme_font_size("font_size") != 11:
		_fail("level font mismatch: %s size=%s" % [level_label.get_theme_font("font").resource_path, level_label.get_theme_font_size("font_size")])
		return
	if ac_value.get_theme_font("font").resource_path != "res://assets/font/0B_WenQuanYi_Bitmap_Song_15_px.ttf" or dc_value.get_theme_font("font").resource_path != "res://assets/font/0B_WenQuanYi_Bitmap_Song_15_px.ttf":
		_fail("AC/DC font mismatch: %s / %s" % [ac_value.get_theme_font("font").resource_path, dc_value.get_theme_font("font").resource_path])
		return
	if ac_value.get_theme_font_size("font_size") != 15 or dc_value.get_theme_font_size("font_size") != 15:
		_fail("AC/DC font size mismatch")
		return
	if level_label.position != Vector2(395, 19) or level_label.size != Vector2(21, 12) or ac_value.position != Vector2(669, 124) or ac_value.size != Vector2(46, 25) or dc_value.position != Vector2(752, 124) or dc_value.size != Vector2(47, 25):
		_fail("persistent HUD text geometry mismatch")
		return
	var arc_current := panel.get_node("%ArcCurrent") as TextureRect
	var arc_next := panel.get_node("%ArcNext") as TextureRect
	if arc_current.position != Vector2(46, 8) or arc_current.size != Vector2(36, 20) or arc_next.position != Vector2(46, 8) or arc_next.size != Vector2(36, 20):
		_fail("title arc geometry mismatch")
		return
	panel.call("_update_title_arc", 0.0)
	if arc_current.texture == null or arc_next.texture == null or arc_current.modulate.a != 1.0 or arc_next.modulate.a != 0.0:
		_fail("title arc initial frame mismatch")
		return
	panel.call("_update_title_arc", 1250.0)
	if not is_equal_approx(arc_current.modulate.a, 191.0 / 255.0) or not is_equal_approx(arc_next.modulate.a, 64.0 / 255.0):
		_fail("title arc cross-fade mismatch: %s / %s" % [arc_current.modulate.a, arc_next.modulate.a])
		return
	var screenshot_path := OS.get_environment("MIR2X_FRIEND_FEEDBACK_SCREENSHOT")
	if screenshot_path.is_empty():
		screenshot_path = OS.get_environment("MIR2X_HUD_ARC_SCREENSHOT")
	if screenshot_path.is_empty():
		screenshot_path = OS.get_environment("MIR2X_HUD_TEXT_SCREENSHOT")
	if not screenshot_path.is_empty():
		panel.set_process(false)
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		var save_error := get_viewport().get_texture().get_image().save_png(screenshot_path)
		if save_error != OK:
			_fail("failed to save HUD text screenshot: %s" % save_error)
			return
		print("HUD TEXT VISUAL PASS: %s" % screenshot_path)
		get_tree().quit()
		return
	if chat_log.get_v_scroll_bar().modulate.a != 0.0:
		_fail("default chat scrollbar is visible")
		return
	var chat_slider := panel.get_node("Body/ChatSlider") as TextureRect
	var chat_slider_hit_area := panel.get_node("Body/ChatSliderHitArea") as Control
	var chat_scroll_bar := chat_log.get_v_scroll_bar()
	if chat_slider.size != Vector2(17, 19) or chat_slider.position != Vector2(619.5, 110) or chat_slider.modulate != Color(0.5, 0.5, 0.5, 1.0) or chat_slider_hit_area.position != Vector2(615, 49) or chat_slider_hit_area.size != Vector2(18, 80):
		_fail("compact original chat slider geometry mismatch")
		return
	var scroll_reach := chat_scroll_bar.max_value - chat_scroll_bar.page
	if scroll_reach <= 45.0:
		_fail("chat slider fixture has no useful scroll range")
		return
	var wheel_up := InputEventMouseButton.new()
	wheel_up.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel_up.pressed = true
	wheel_up.factor = 1.0
	var bottom_value := chat_scroll_bar.value
	panel.call("_on_chat_input", wheel_up)
	if not is_equal_approx(chat_scroll_bar.value, bottom_value - 45.0):
		_fail("chat wheel did not move by the original three-line step")
		return
	var slider_press := InputEventMouseButton.new()
	slider_press.button_index = MOUSE_BUTTON_LEFT
	slider_press.pressed = true
	slider_press.position = Vector2(9, 10)
	panel.call("_on_chat_slider_input", slider_press)
	if chat_scroll_bar.value != 0.0 or chat_slider.position.y != 51.0 or chat_slider.modulate != Color.WHITE:
		_fail("compact chat slider drag-to-top mismatch")
		return
	var slider_release := InputEventMouseButton.new()
	slider_release.button_index = MOUSE_BUTTON_LEFT
	slider_release.pressed = false
	panel.call("_on_chat_slider_input", slider_release)
	if chat_slider.modulate != Color(0.5, 0.5, 0.5, 1.0):
		_fail("chat slider idle tint mismatch")
		return
	GameState.add_chat_log("新消息回到底部", 1)
	await get_tree().process_frame
	await get_tree().process_frame
	if not is_equal_approx(chat_scroll_bar.value, chat_scroll_bar.max_value - chat_scroll_bar.page) or chat_slider.position.y != 110.0:
		_fail("new chat message did not restore original bottom-follow state")
		return
	panel.call("_on_expand_pressed")
	if panel.get_node("%Face").visible or panel.get_node("%FaceHealth").visible or panel.get_node("%BuffContainer").visible:
		_fail("compact focus HUD remains visible while chat is expanded")
		return
	if panel.get_node("Body/ExpandButton").position.y != -266.0 or not panel.get_node("Body/EmojiButton").visible or not panel.get_node("Body/MuteButton").visible:
		_fail("expanded HUD switch or hover-only controls mismatch")
		return
	if chat_slider.position != Vector2(619.5, 108) or chat_slider_hit_area.position != Vector2(615, -218) or chat_slider_hit_area.size != Vector2(18, 345):
		_fail("expanded original chat slider geometry mismatch")
		return
	var emoji_button := panel.get_node("Body/EmojiButton") as TextureButton
	var mute_button := panel.get_node("Body/MuteButton") as TextureButton
	if emoji_button.modulate.a != 0.0 or mute_button.modulate.a != 0.0:
		_fail("expanded hover-only controls were visible while idle")
		return
	emoji_button.mouse_entered.emit()
	if emoji_button.modulate.a != 1.0 or mute_button.modulate.a != 0.0:
		_fail("expanded hover feedback affected the wrong control")
		return
	emoji_button.mouse_exited.emit()
	if panel.get_node("Body/ExpandButton").texture_normal != null or panel.get_node("Body/ExpandButton").texture_hover == null or panel.get_node("Body/ExpandButton").texture_pressed == null:
		_fail("expand control did not use original hover/down-only textures")
		return
	panel.call("_on_expand_pressed")
	if not panel.get_node("%Face").visible or not panel.get_node("%FaceHealth").visible or not panel.get_node("%BuffContainer").visible:
		_fail("compact focus HUD did not return after collapsing chat")
		return
	if panel.get_node("Body/ExpandButton").position.y != 22.0 or panel.get_node("Body/EmojiButton").visible or panel.get_node("Body/MuteButton").visible:
		_fail("collapsed HUD retained expanded controls")
		return
	panel.call("_on_minimize_pressed")
	if panel.get_node("%Body").visible or panel.get_node("Title").position.y != 121.0 or panel.get_node("%Level").visible or panel.get_node("MinimizeButton").visible:
		_fail("minimized HUD does not match the C++ title-only layout")
		return
	var title_double_click := InputEventMouseButton.new()
	title_double_click.button_index = MOUSE_BUTTON_LEFT
	title_double_click.pressed = true
	title_double_click.double_click = true
	panel.call("_on_title_gui_input", title_double_click)
	if not panel.get_node("%Body").visible or panel.get_node("Title").position.y != 0.0 or not panel.get_node("%Level").visible or not panel.get_node("MinimizeButton").visible:
		_fail("title double-click did not restore the compact HUD")
		return
	panel.call("_on_title_gui_input", title_double_click)
	if panel.get_node("%Body").visible:
		_fail("title double-click did not minimize the compact HUD")
		return
	panel.call("_on_minimize_pressed")
	panel.call("_on_ac_pressed")
	panel.call("_on_dc_pressed")
	if panel.get_node("%ACValue").text != "%d-%d" % [combat.mac[0], combat.mac[1]] or panel.get_node("%DCValue").text != "%d-%d" % [combat.mc[0], combat.mc[1]]:
		_fail("AC/MA or DC/MC toggle mismatch")
		return
	GameState.chat_log.clear()
	panel.call("_on_exchange_pressed")
	if GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "exchange doesn't implemented yet":
		_fail("exchange control did not provide the original feedback")
		return
	GameState.chat_log.clear()
	command.text = "   local echo  "
	panel.call("focus_command")
	await get_tree().process_frame
	panel.call("_on_command_submitted", command.text)
	if not command.text.is_empty() or command.has_focus():
		_fail("command submission did not clear and release the input")
		return
	if GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "local echo  ":
		_fail("ordinary command did not trim left, preserve right, and echo locally: %s" % GameState.chat_log)
		return
	panel.call("_on_command_submitted", "   !   ")
	if GameState.chat_log.size() != 1:
		_fail("empty broadcast command changed visible chat state")
		return
	panel.call("_on_command_submitted", "   @help")
	if GameState.chat_log.size() != 11 or GameState.chat_log[1].text != "@moveTo" or GameState.chat_log[-1].text != "@help":
		_fail("leading whitespace did not list the original user commands: %s" % GameState.chat_log)
		return
	GameState.chat_log.clear()
	panel.call("_on_command_submitted", "@add nope")
	if GameState.chat_log.size() != 3 or GameState.chat_log[0].text != ">> 用户命令有歧义：add" or GameState.chat_log[1].text != ">> 候选命令：addHP" or GameState.chat_log[2].text != ">> 候选命令：addExp":
		_fail("ambiguous user-command prefix did not list C++ candidates: %s" % GameState.chat_log)
		return
	GameState.chat_log.clear()
	panel.call("_on_command_submitted", "@addH nope")
	panel.call("_on_command_submitted", "@makeItem 不存在的物品")
	panel.call("_on_command_submitted", "@luaE")
	if GameState.chat_log.size() != 3 or GameState.chat_log[0].text != "用法：@addHP 数量" or GameState.chat_log[1].text != "无效的物品名：不存在的物品" or GameState.chat_log[2].text != ">> Lua 编辑器尚未实现":
		_fail("user-command validation or unique prefix routing mismatch: %s" % GameState.chat_log)
		return
	if not NetworkClient.has_method("send_query_map_base_uid") or not panel.has_method("_parse_move_to_arguments"):
		_fail("moveTo does not expose the original map UID query and argument forms")
		return
	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	var move_cases := [
		[["moveTo"], {"map_id": 24, "random": true}],
		[["moveTo", "25"], {"map_id": 25, "random": true}],
		[["moveTo", "10", "11"], {"map_id": 24, "random": false, "x": 10, "y": 11}],
		[["moveTo", "25", "12", "13"], {"map_id": 25, "random": false, "x": 12, "y": 13}],
	]
	for move_case: Array in move_cases:
		var parsed: Dictionary = panel.call("_parse_move_to_arguments", move_case[0])
		for key: String in move_case[1]:
			if parsed.get(key) != move_case[1][key]:
				_fail("moveTo argument parity mismatch: tokens=%s parsed=%s" % [move_case[0], parsed])
				return
	if NetworkClient.send_query_map_base_uid(24, Callable()) != ERR_UNCONFIGURED:
		_fail("offline map UID query unexpectedly connected or used the wrong request path")
		return
	var move_world := WorldResourceScript.new()
	if not move_world.load_map(24):
		_fail("moveTo regression could not load its map fixture")
		return
	var move_location: Vector2i = panel.call("_random_walkable_location", move_world)
	if move_location.x < 0 or not move_world.can_walk(move_location.x, move_location.y):
		_fail("moveTo random location was not a valid original ground cell: %s" % move_location)
		return
	GameState.chat_log.clear()
	panel.call("_on_command_submitted", "@moveTo %d %d" % [move_location.x, move_location.y])
	if GameState.chat_log.size() != 1 or not str(GameState.chat_log[0].text).ends_with("failed"):
		_fail("current-map moveTo still used local walking instead of a space-move request: %s" % GameState.chat_log)
		return
	print("HUD STATE PASS: self/focus face, HP, compact buffs, target depth, experience, load, controls and original user commands")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("HUD_STATE_SMOKE %s" % message)
	get_tree().quit(1)


func _face_uses_original_crop(face: TextureRect) -> bool:
	var crop := face.texture as AtlasTexture
	if crop == null or crop.atlas == null:
		return false
	var expected_size := Vector2(maxi(0, crop.atlas.get_width() - 2), crop.atlas.get_height())
	return crop.region == Rect2(Vector2.ZERO, expected_size) and face.size == expected_size
