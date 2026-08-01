extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")
const WorldRendererScript = preload("res://scripts/game/world_renderer.gd")


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
	GameState.add_chat_log("普通消息", 0)
	GameState.add_chat_log("获得物品", 1)
	GameState.add_chat_log("广播消息", 2)
	GameState.add_chat_log("错误消息", 3)
	var panel: Control = load("res://scenes/game/control_panel.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("%Face").texture == null:
		_fail("player face missing")
		return
	if panel.get_node("%BuffContainer").get_child_count() != 1:
		_fail("compact self buff missing")
		return
	var monster_id: int = resources.monster_meta.keys()[0]
	panel.call("_apply_focus_hud", {"uid": 101, "type": 1, "monster_id": monster_id, "hp": 25, "hp_max": 100, "buffs": GameState.buff_list})
	if absf(panel.get_node("%FaceHealth").size.x - 20.5) > 0.01 or panel.get_node("%Face").texture == null:
		_fail("focused monster portrait or health mismatch")
		return
	if panel.get_node("%BuffContainer").get_child_count() != 1:
		_fail("focused monster buff missing")
		return
	var renderer: Control = WorldRendererScript.new()
	renderer.game_state = GameState
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
	if command.get_theme_font_size("font_size") != 15:
		_fail("command font size mismatch")
		return
	if chat_log.get_theme_font_size("normal_font_size") != 15 or chat_log.get_theme_font("normal_font") == null:
		_fail("chat font mismatch")
		return
	if chat_log.get_v_scroll_bar().modulate.a != 0.0:
		_fail("default chat scrollbar is visible")
		return
	panel.call("_on_expand_pressed")
	if panel.get_node("%Face").visible or panel.get_node("%FaceHealth").visible or panel.get_node("%BuffContainer").visible:
		_fail("compact focus HUD remains visible while chat is expanded")
		return
	if panel.get_node("Body/ExpandButton").position.y != -266.0 or not panel.get_node("Body/EmojiButton").visible or not panel.get_node("Body/MuteButton").visible:
		_fail("expanded HUD switch or hover-only controls mismatch")
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
	if GameState.chat_log.size() != 2 or GameState.chat_log[-1].text != "可用命令：@help":
		_fail("leading whitespace did not preserve prefixed command routing")
		return
	print("HUD STATE PASS: self/focus face, HP, compact buffs, target depth, experience, load, controls and command input")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("HUD_STATE_SMOKE %s" % message)
	get_tree().quit(1)
