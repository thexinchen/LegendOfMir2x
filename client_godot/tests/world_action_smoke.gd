extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldPathfinderScript = preload("res://scripts/game/world_pathfinder.gd")


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	var offline_action := PackedByteArray()
	offline_action.resize(43)
	if NetworkClient.send_action(offline_action) != ERR_UNCONFIGURED:
		_fail("offline action unexpectedly initiated a server connection")
		return
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var physical_id: int = resources.magic_id("物理攻击")
	var next_strike_id: int = resources.magic_id("攻杀剑术")
	if physical_id == 0 or next_strike_id == 0:
		_fail("attack magic metadata unavailable")
		return

	var encoded := Protocol.encode_action_node({"type": 7, "speed": 100, "magicID": physical_id, "modifierID": 9})
	var decoded := Protocol.decode_action_node(encoded)
	if decoded.get("magicID", 0) != physical_id or decoded.get("modifierID", 0) != 9:
		_fail("attack extParam encoding mismatch: %s" % decoded)
		return

	var main: Control = load("res://scenes/game/main.tscn").instantiate()
	add_child(main)
	await get_tree().process_frame
	if OS.has_environment("MIR2X_FPS_SCREENSHOT"):
		if not await _capture_fps_visual(main):
			return
		print("FPS OVERLAY VISUAL PASS: %s" % OS.get_environment("MIR2X_FPS_SCREENSHOT"))
		get_tree().quit()
		return
	if not _test_strike_grid_rendering(main):
		return
	if not _test_camera_centering(main):
		return
	if not _test_missing_minimap_feedback(main):
		return
	if not _test_fps_overlay(main):
		return
	if not _test_team_flag_cursor(main):
		return
	main.call("_on_server_message", NetworkClient.SM_NEXTSTRIKE, PackedByteArray())
	if main.call("_consume_attack_magic_id") != next_strike_id or main.call("_consume_attack_magic_id") != physical_id:
		_fail("SM_NEXTSTRIKE was not consumed exactly once")
		return
	if not _test_player_say(main):
		return
	if not _test_system_chat_feedback(main):
		return
	if not _test_magic_actions(main, resources, physical_id):
		return
	if not _test_magic_panel_hotkey_precedence(main, resources):
		return
	if not _test_async_combat_feedback(main, resources):
		return
	if not _test_health_feedback(main, resources):
		return
	if not _test_buff_transitions(main, resources):
		return
	if not _test_progression_feedback(main, resources):
		return
	if not _test_ping_feedback(main):
		return
	if not await _test_action_seff(main, resources):
		return
	if not _test_shield_hit_action(main, resources):
		return
	if not _test_spinkick_direction(main):
		return
	if not _test_actor_record_lifecycle(main):
		return
	if not _test_npc_actions(main):
		return
	if not _test_monster_spawn_actions(main, resources):
		return
	if not _test_monster_transform_actions(main, resources):
		return
	if not _test_monster_jump_stands(main):
		return
	if not _test_self_action_map_transition(main):
		return

	if not _test_inventory_transaction_feedback(main, resources):
		return
	if not _test_death_and_map_filter(main, resources):
		return
	if not _test_world_displacement(main, resources):
		return
	if not _test_path_decomposition():
		return
	if not _test_chase_retry(main):
		return
	if not _test_exact_frame_input(main):
		return
	if not _test_mining(main, resources):
		return
	if not _test_pickup_action(main, resources):
		return
	print("WORLD ACTION PASS: team flag, focus channels, mining, exact-frame focus, action SEFF, attack/chase, magic keys, pickup, one-hop pathing, operation feedback, death and map filtering")
	get_tree().quit()


func _test_strike_grid_rendering(main: Control) -> bool:
	var renderer: Control = main.get_node("WorldRenderer")
	var expected := Color8(0xFF, 0x00, 0x00, 0x60)
	for age_ms in [0, 500, 1000]:
		if renderer.call("_strike_grid_color", age_ms) != expected:
			_fail("strike grid did not keep C++ pure-red alpha-96 color for one second")
			return false
	if renderer.call("_strike_grid_color", 1001) != Color.TRANSPARENT or renderer.call("_strike_grid_color", -1) != Color.TRANSPARENT:
		_fail("strike grid color escaped its original one-second lifecycle")
		return false
	return true


func _test_camera_centering(main: Control) -> bool:
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.hud_minimized = false
	GameState.center_camera_on_player()
	if GameState.camera_center_y() != 234 or not is_equal_approx(GameState.view_y, float(132 * 32 - 234)):
		_fail("normal HUD camera does not use C++ 469px world viewport")
		return false
	var control_panel: Control = main.get_node("ControlPanel")
	control_panel.call("_on_minimize_pressed")
	if not GameState.hud_minimized or GameState.camera_center_y() != 300:
		_fail("minimized HUD did not expose the full 600px world viewport")
		return false
	var previous_view_y := GameState.view_y
	GameState.scroll_camera()
	if not is_equal_approx(GameState.view_y, previous_view_y - 2.0):
		_fail("minimized HUD camera did not smoothly converge at the C++ vertical rate")
		return false
	control_panel.call("_on_minimize_pressed")
	main.call("_center_hero")
	if GameState.hud_minimized or not is_equal_approx(GameState.view_y, float(132 * 32 - 234)):
		_fail("restored HUD or ESC centering did not return to the 469px viewport")
		return false
	return true


func _test_missing_minimap_feedback(main: Control) -> bool:
	var panels: Dictionary = main.get("_extra_panel_nodes")
	var minimap := panels.get("res://scenes/game/panels/minimap.tscn") as Control
	if minimap == null or minimap.call("has_map_texture"):
		_fail("missing-map feedback fixture unexpectedly has a minimap texture")
		return false
	var requested_before: bool = minimap.call("requested_visible")
	GameState.chat_log.clear()
	main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/minimap.tscn")
	if GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "没有可用的地图":
		_fail("missing minimap control did not provide the original error feedback")
		return false
	if minimap.call("requested_visible") != requested_before:
		_fail("missing minimap control changed the requested visibility state")
		return false
	return true


func _test_fps_overlay(main: Control) -> bool:
	GameState.runtime_config[6] = PackedByteArray([1, 1, 0])
	main.call("_update_fps_overlay")
	var fps := main.get_node("FPS") as Label
	if not fps.visible or fps.text.is_empty() or fps.get_theme_font_size("font_size") != 15:
		_fail("runtime FPS option did not restore the original top-right overlay")
		return false
	var one_digit: Vector2i = main.call("_layout_fps_overlay", "9")
	var three_digits: Vector2i = main.call("_layout_fps_overlay", "120")
	var font := fps.get_theme_font("font")
	var measured := font.get_string_size("120", HORIZONTAL_ALIGNMENT_LEFT, -1, 15)
	var style := fps.get_theme_stylebox("normal") as StyleBoxFlat
	if one_digit.x >= three_digits.x or three_digits != Vector2i(ceili(measured.x) + 1, ceili(measured.y)) or Vector2i(fps.size) != three_digits:
		_fail("runtime FPS overlay did not follow the original content-sized geometry")
		return false
	if not is_equal_approx(fps.position.x + fps.size.x, main.size.x) or style == null or not is_equal_approx(style.content_margin_left, 1.0) or not is_equal_approx(style.content_margin_top, 0.0) or not is_equal_approx(style.content_margin_right, 0.0) or not is_equal_approx(style.content_margin_bottom, 0.0):
		_fail("runtime FPS overlay did not preserve the original right anchor and one-pixel left margin")
		return false
	GameState.runtime_config[6] = PackedByteArray([1, 0, 0])
	main.call("_update_fps_overlay")
	if fps.visible:
		_fail("runtime FPS overlay did not hide")
		return false
	return true


func _capture_fps_visual(main: Control) -> bool:
	GameState.player_uid = 999
	GameState.player_x = 371
	GameState.player_y = 132
	GameState.view_x = float(371 * 48 - 304)
	GameState.view_y = float(132 * 32 - 234)
	var renderer := main.get_node("WorldRenderer")
	if not renderer.load_map(24):
		_fail("unable to load FPS visual map")
		return false
	main.set_process(false)
	renderer.set_process(false)
	var fps := main.get_node("FPS") as Label
	fps.visible = true
	main.call("_layout_fps_overlay", "120")
	await RenderingServer.frame_post_draw
	var error := get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FPS_SCREENSHOT"))
	if error != OK:
		_fail("unable to save FPS overlay screenshot: %s" % error)
		return false
	return true


func _test_team_flag_cursor(main: Control) -> bool:
	var previous_uid := GameState.player_uid
	var previous_members := GameState.team_members.duplicate(true)
	var target_uid := (5 << 59) | 202
	var monster_uid := (1 << 59) | 203
	GameState.player_uid = (5 << 59) | 1
	GameState.team_members = []
	GameState.creatures[target_uid] = {"uid": target_uid, "type": 2, "name": "队旗目标"}
	GameState.creatures[monster_uid] = {"uid": monster_uid, "type": 1, "name": "队旗非玩家"}
	main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/team.tscn")
	main.call("_update_team_flag_cursor")
	var cursor := main.get_node("TeamFlagCursor") as TextureRect
	if not main.get("_team_flag_active") or not cursor.visible or cursor.texture == null:
		_fail("no-team HUD request did not enable the original team-flag cursor")
		return false
	if main.call("_team_flag_frame_index", 0) != 0 or main.call("_team_flag_frame_index", 199) != 0 or main.call("_team_flag_frame_index", 200) != 1 or main.call("_team_flag_frame_index", 2600) != 0:
		_fail("team-flag animation does not run through 13 frames at 5 FPS")
		return false
	if main.call("_request_team_flag_target", target_uid) != ERR_UNCONFIGURED:
		_fail("team-flag player target did not issue the join-team request")
		return false
	if main.call("_request_team_flag_target", monster_uid) != ERR_INVALID_PARAMETER:
		_fail("team-flag accepted a non-player target")
		return false
	var move_path: Array = main.get("_move_path")
	move_path.append(Vector2i(10, 10))
	var cancel_event := InputEventMouseButton.new()
	cancel_event.button_index = MOUSE_BUTTON_RIGHT
	cancel_event.pressed = true
	cancel_event.position = Vector2(400, 300)
	main.call("_handle_mouse_click", cancel_event)
	if main.get("_team_flag_active") or cursor.visible or move_path.size() != 1:
		_fail("team-flag right-click did not cancel without changing movement")
		return false
	move_path.clear()
	GameState.team_members = [{"uid": GameState.player_uid, "name": "自己"}]
	main.call("_on_control_panel_panel_requested", "res://scenes/game/panels/team.tscn")
	var panels: Dictionary = main.get("_extra_panel_nodes")
	var team_panel := panels.get("res://scenes/game/panels/team.tscn") as Control
	if team_panel == null or not team_panel.visible or main.get("_team_flag_active"):
		_fail("in-team HUD request did not open the team panel")
		return false
	team_panel.hide()
	GameState.creatures.erase(target_uid)
	GameState.creatures.erase(monster_uid)
	GameState.player_uid = previous_uid
	GameState.team_members = previous_members
	return true


func _test_player_say(main: Control) -> bool:
	GameState.chat_log.clear()
	GameState.player_say_messages.clear()
	GameState.player_uid = (5 << 59) | 1
	var remote_uid: int = (5 << 59) | 2
	var monster_uid: int = (4 << 59) | 3
	GameState.creatures = {
		remote_uid: {"uid": remote_uid, "type": 2, "name": "远端玩家"},
		monster_uid: {"uid": monster_uid, "type": 1},
	}
	main.call("_on_server_message", NetworkClient.SM_PLAYERSAY, _player_say_payload(GameState.player_uid, "自己的头顶消息"))
	if GameState.player_say_messages.get(GameState.player_uid, []).size() != 1 or not GameState.chat_log.is_empty():
		_fail("self player say did not remain bubble-only")
		return false
	main.call("_on_server_message", NetworkClient.SM_PLAYERSAY, _player_say_payload(remote_uid, "远端原文"))
	if GameState.player_say_messages.get(remote_uid, []).size() != 1 or GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "远端原文":
		_fail("remote player say bubble/chat semantics mismatch: %s" % GameState.chat_log)
		return false
	main.call("_on_server_message", NetworkClient.SM_PLAYERSAY, _player_say_payload(monster_uid, "无效气泡"))
	if GameState.player_say_messages.has(monster_uid) or GameState.chat_log.size() != 2 or GameState.chat_log[1].text != "无效气泡":
		_fail("non-hero player say filtering mismatch")
		return false
	GameState.player_say_messages.clear()
	GameState.creatures.clear()
	return true


func _test_system_chat_feedback(main: Control) -> bool:
	var saved_uid: int = GameState.player_uid
	var saved_name: String = GameState.player_name
	var saved_creatures := GameState.creatures.duplicate(true)
	var local_uid: int = (5 << 59) | 11
	var remote_uid: int = (5 << 59) | 12
	var monster_uid: int = (4 << 59) | 13
	GameState.player_uid = local_uid
	GameState.player_name = "本地角色"
	GameState.creatures = {
		remote_uid: {"uid": remote_uid, "type": 2, "name": "远端角色"},
		monster_uid: {"uid": monster_uid, "type": 1, "name": "怪物名"},
	}
	GameState.chat_log.clear()
	main.call("_on_server_message", NetworkClient.SM_TEXT, "服务器正文".to_utf8_buffer())
	main.call("_on_server_message", NetworkClient.SM_PLAYERBROADCAST, _player_say_payload(local_uid, "本地广播"))
	main.call("_on_server_message", NetworkClient.SM_PLAYERBROADCAST, _player_say_payload(remote_uid, "远端广播"))
	main.call("_on_server_message", NetworkClient.SM_PLAYERBROADCAST, _player_say_payload(monster_uid, "非角色广播"))
	main.call("_on_server_message", NetworkClient.SM_PLAYERBROADCAST, _player_say_payload((5 << 59) | 99, "未知广播"))
	var expected := ["服务器正文", "本地角色: 本地广播", "远端角色: 远端广播", "非角色广播", "未知广播"]
	if GameState.chat_log.size() != expected.size():
		_fail("system chat feedback count mismatch: %s" % GameState.chat_log)
		return false
	for index in expected.size():
		if GameState.chat_log[index].text != expected[index] or GameState.chat_log[index].type != 1:
			_fail("system chat routing mismatch at %d: %s" % [index, GameState.chat_log[index]])
			return false
	GameState.player_uid = saved_uid
	GameState.player_name = saved_name
	GameState.creatures = saved_creatures
	GameState.chat_log.clear()
	return true


func _test_inventory_transaction_feedback(main: Control, resources: RefCounted) -> bool:
	var known_item_id: int = resources.item_names.keys()[0]
	var packable_id := 0
	var non_packable_id := 0
	for item_id_value in resources.item_meta:
		var item_id: int = item_id_value
		if resources.item_is_packable(item_id) and packable_id == 0:
			packable_id = item_id
		elif not resources.item_is_packable(item_id) and resources.item_type(item_id) != "金币" and non_packable_id == 0:
			non_packable_id = item_id
		if packable_id != 0 and non_packable_id != 0:
			break
	if packable_id == 0 or non_packable_id == 0:
		_fail("inventory transaction fixtures unavailable")
		return false
	var saved_chat := GameState.chat_log.duplicate(true)
	var saved_sell := GameState.npc_sell.duplicate(true)
	var saved_detail := GameState.npc_sell_detail.duplicate(true)
	GameState.chat_log.clear()
	main.call("_on_server_message", NetworkClient.SM_PICKUPERROR, _u32_payload(known_item_id))
	main.call("_on_server_message", NetworkClient.SM_PICKUPERROR, _u32_payload(0xFFFFFFFE))
	main.call("_on_server_message", NetworkClient.SM_PICKUPERROR, _u32_payload(0))
	var pickup_expected := [
		"无法捡起%s" % resources.item_name(known_item_id),
		"无法捡起物品ID = 4294967294",
		"当前无法捡起物品，请稍后再试",
	]
	for index in pickup_expected.size():
		if GameState.chat_log[index].text != pickup_expected[index] or GameState.chat_log[index].type != 1:
			_fail("pickup error feedback mismatch at %d: %s" % [index, GameState.chat_log])
			return false
	GameState.chat_log.clear()
	for error in [1, 2, 3, 4]:
		main.call("_on_server_message", NetworkClient.SM_EQUIPWEARERROR, _item_error_payload(known_item_id, 1, error))
	main.call("_on_server_message", NetworkClient.SM_GRABWEARERROR, _u16_payload(1))
	main.call("_on_server_message", NetworkClient.SM_GRABWEARERROR, _u16_payload(2))
	for error in [1, 2, 3, 4]:
		main.call("_on_server_message", NetworkClient.SM_EQUIPBELTERROR, _item_error_payload(known_item_id, 1, error))
	main.call("_on_server_message", NetworkClient.SM_GRABBELTERROR, _u16_payload(1))
	var equipment_expected := [
		"无效的物品", "无效的物品", "无法放置：%s" % resources.item_name(known_item_id),
		"无法取下装备",
		"无效的物品", "无效的物品", "无法装备：%s" % resources.item_name(known_item_id),
	]
	if GameState.chat_log.size() != equipment_expected.size():
		_fail("silent equipment error branches created feedback: %s" % GameState.chat_log)
		return false
	for index in equipment_expected.size():
		if GameState.chat_log[index].text != equipment_expected[index] or GameState.chat_log[index].type != 3:
			_fail("equipment error feedback mismatch at %d: %s" % [index, GameState.chat_log])
			return false
	GameState.chat_log.clear()
	main.call("_on_server_message", NetworkClient.SM_BUYERROR, _buy_error_payload(77, known_item_id, 1, 4))
	main.call("_on_server_message", NetworkClient.SM_BUYERROR, _buy_error_payload(77, known_item_id, 1, 2))
	if GameState.chat_log.size() != 2 or GameState.chat_log[0].text != "金币不够" or GameState.chat_log[1].text != "购买失败" or GameState.chat_log[0].type != 3 or GameState.chat_log[1].type != 3:
		_fail("buy error feedback mismatch: %s" % GameState.chat_log)
		return false
	GameState.chat_log.clear()
	GameState.npc_sell = {"npcUID": 88, "itemList": [packable_id, non_packable_id]}
	GameState.npc_sell_detail = {"npcUID": 77, "list": [
		{"item": {"itemID": packable_id, "seqID": 0}},
		{"item": {"itemID": non_packable_id, "seqID": 9}},
		{"item": {"itemID": non_packable_id, "seqID": 10}},
	]}
	main.call("_on_server_message", NetworkClient.SM_BUYSUCCEED, _buy_succeed_payload(77, non_packable_id, 9))
	if GameState.npc_sell_detail.list.size() != 3:
		_fail("stale shop buy success changed detail list")
		return false
	GameState.npc_sell["npcUID"] = 77
	main.call("_on_server_message", NetworkClient.SM_BUYSUCCEED, _buy_succeed_payload(77, packable_id, 0))
	main.call("_on_server_message", NetworkClient.SM_BUYSUCCEED, _buy_succeed_payload(77, non_packable_id, 9))
	if GameState.npc_sell_detail.list.size() != 2 or GameState.npc_sell_detail.list[0].item.itemID != packable_id or GameState.npc_sell_detail.list[1].item.seqID != 10 or not GameState.chat_log.is_empty():
		_fail("buy success mutation/feedback mismatch: detail=%s log=%s" % [GameState.npc_sell_detail, GameState.chat_log])
		return false
	GameState.chat_log = saved_chat
	GameState.npc_sell = saved_sell
	GameState.npc_sell_detail = saved_detail
	return true


func _player_say_payload(uid: int, text: String) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(136)
	payload.encode_u64(0, uid)
	var encoded := text.to_utf8_buffer()
	for index in mini(encoded.size(), 127):
		payload[8 + index] = encoded[index]
	return payload


func _test_action_seff(main: Control, resources: RefCounted) -> bool:
	AudioService.set_seff_enabled(true)
	AudioService.set_seff_volume(0.5)
	GameState.player_uid = (5 << 59) | 1
	GameState.player_x = 10
	GameState.player_y = 10
	var weapon_id := 0
	var weapon_sound := 7
	for item_id_value in resources.item_attributes:
		var sound_class: int = resources.item_weapon_sound(item_id_value)
		if sound_class < 7:
			weapon_id = int(item_id_value)
			weapon_sound = sound_class
			break
	if weapon_id == 0:
		_fail("weapon SEFF test item unavailable")
		return false
	main.call("_play_action_seff", GameState.player_uid, {"type": 7, "x": 10, "y": 10}, {
		"uid": GameState.player_uid, "type": 2, "gender": 0,
		"desp": {"wear": {3: {"itemID": weapon_id}}},
	})
	if AudioService.last_seff_id != 0x01010032 + weapon_sound:
		_fail("Hero weapon SEFF route mismatch")
		return false
	var monster_id := 224
	var monster_attack: int = resources.monster_seff(monster_id, 7)
	main.call("_play_action_seff", (4 << 59) | (monster_id << 35) | 1, {"type": 7, "x": 13, "y": 10}, {
		"type": 1, "monster_id": monster_id,
	})
	if AudioService.last_seff_id != monster_attack or AudioService.last_seff_distance != 3:
		_fail("monster attack SEFF route mismatch")
		return false
	var monster_die: int = resources.monster_seff(monster_id, 13)
	main.call("_play_action_seff", (4 << 59) | (monster_id << 35) | 1, {"type": 13, "x": 13, "y": 10}, {
		"type": 1, "monster_id": monster_id,
	})
	if AudioService.last_seff_id != monster_die:
		_fail("monster die SEFF route mismatch")
		return false
	main.call("_play_action_seff", GameState.player_uid, {"type": 11, "x": 10, "y": 10, "aimUID": 0}, {
		"uid": GameState.player_uid, "type": 2, "gender": 0, "desp": {"wear": {}},
	})
	if AudioService.last_seff_id != 0x01010049:
		_fail("Hero hitted impact SEFF route mismatch")
		return false
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	GameState.player_action_type = 3
	GameState.player_action_started_ms = Time.get_ticks_msec()
	main.call("_play_action_seff", GameState.player_uid, {
		"type": 3, "speed": 100, "x": 10, "y": 10, "aimX": 11, "aimY": 10,
	}, {
		"uid": GameState.player_uid, "type": 2, "action_started_ms": GameState.player_action_started_ms,
	})
	await get_tree().create_timer(0.15).timeout
	if AudioService.last_seff_id != 0x01000001:
		_fail("Hero first movement step did not trigger at frame 1")
		return false
	await get_tree().create_timer(0.3).timeout
	if AudioService.last_seff_id != 0x01000002:
		_fail("Hero second movement step did not trigger at frame 4")
		return false
	AudioService.stop_seff()
	return true


func _test_magic_actions(main: Control, resources: RefCounted, physical_id: int) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 10
	GameState.player_y = 10
	GameState.player_direction = 5
	GameState.magic_effects.clear()
	var firewall_id: int = resources.magic_id("火墙")
	var shield_id: int = resources.magic_id("魔法盾")
	var fireball_id: int = resources.magic_id("火球术")
	var flame_sword_id: int = resources.magic_id("烈火剑法")
	var half_moon_id: int = resources.magic_id("半月弯刀")
	if 0 in [firewall_id, shield_id, fireball_id, flame_sword_id, half_moon_id]:
		_fail("spell metadata unavailable")
		return false
	GameState.learned_magic = [{"magicID": firewall_id, "exp": 0}]
	GameState.magic_keys = {firewall_id: 120}
	var key_event := InputEventKey.new()
	key_event.unicode = 120
	key_event.keycode = KEY_X
	if not main.call("_try_magic_key", key_event) or GameState.player_action_type != 9:
		_fail("configured magic key did not enter ACTION_SPELL")
		return false
	if GameState.magic_effects.is_empty() or GameState.magic_effects.back().get("magicID", 0) != firewall_id:
		_fail("ground spell did not create local magic effect")
		return false
	if not main.call("_cast_magic", shield_id, Vector2i(12, 12)) or GameState.magic_effects.back().get("aimUID", 0) != GameState.player_uid:
		_fail("self spell did not target player UID")
		return false
	GameState.update_creature(404, {"uid": 404, "x": 9, "y": 10, "type": 1, "action_type": 2})
	GameState.update_creature(505, {"uid": 505, "x": 12, "y": 10, "type": 1, "action_type": 2})
	main.get_node("WorldRenderer")._actor_target_rects = {505: {"rect": Rect2(-100, -100, 200, 200), "map_y": 10}}
	main.set("_magic_focus_uid", 404)
	GameState.magic_cast_times[fireball_id] = Time.get_ticks_msec()
	var effect_count := GameState.magic_effects.size()
	if main.call("_cast_magic", fireball_id, Vector2i(12, 10)) or main.get("_magic_focus_uid") != 505 or GameState.magic_effects.size() != effect_count:
		_fail("cooldown-blocked spell did not refresh magic focus without casting")
		return false
	GameState.magic_cast_times.erase(fireball_id)
	if not main.call("_cast_magic", fireball_id, Vector2i(12, 10)) or GameState.magic_effects.back().get("aimUID", 0) != 505:
		_fail("target spell did not retain focused creature UID")
		return false
	if not main.call("_cast_magic", flame_sword_id, Vector2i.ZERO) or main.call("_consume_attack_magic_id") != flame_sword_id or main.call("_consume_attack_magic_id") != physical_id:
		_fail("single-use swing magic was not consumed once")
		return false
	if not main.call("_cast_magic", half_moon_id, Vector2i.ZERO) or main.call("_consume_attack_magic_id") != half_moon_id or main.call("_consume_attack_magic_id") != half_moon_id:
		_fail("persistent swing magic was not retained")
		return false
	main.set("_swing_magic", {})
	main.call("_cancel_movement")
	main.get_node("WorldRenderer")._actor_target_rects.clear()
	return true


func _test_magic_panel_hotkey_precedence(main: Control, resources: RefCounted) -> bool:
	var firewall_id: int = resources.magic_id("火墙")
	var previous_learned: Array = GameState.learned_magic.duplicate(true)
	var previous_keys: Dictionary = GameState.magic_keys.duplicate(true)
	var previous_cast_times: Dictionary = GameState.magic_cast_times.duplicate(true)
	var inventory := main.get_node("InventoryPanel") as Control
	inventory.hide()
	GameState.learned_magic = [{"magicID": firewall_id, "exp": 0}]
	GameState.magic_keys = {firewall_id: 98}
	GameState.magic_cast_times.erase(firewall_id)
	var b_key := InputEventKey.new()
	b_key.keycode = KEY_B
	b_key.unicode = 98
	b_key.pressed = true
	var effect_count := GameState.magic_effects.size()
	main.call("_unhandled_input", b_key)
	if inventory.visible or GameState.magic_effects.size() != effect_count + 1 or GameState.player_action_type != 9:
		_fail("learned B-bound magic was swallowed by the inventory shortcut")
		return false
	effect_count = GameState.magic_effects.size()
	main.call("_unhandled_input", b_key)
	if inventory.visible or GameState.magic_effects.size() != effect_count:
		_fail("cooldown-blocked B-bound magic leaked through to the inventory shortcut")
		return false
	GameState.learned_magic = []
	main.call("_unhandled_input", b_key)
	if not inventory.visible:
		_fail("unlearned B binding did not fall back to the Godot inventory shortcut")
		return false
	inventory.hide()
	var horse_path := "res://scenes/game/panels/horse.tscn"
	var panels: Dictionary = main.get("_extra_panel_nodes")
	if panels.has(horse_path):
		(panels[horse_path] as Control).hide()
	GameState.learned_magic = [{"magicID": firewall_id, "exp": 0}]
	GameState.magic_keys = {firewall_id: 104}
	GameState.magic_cast_times.erase(firewall_id)
	var h_key := InputEventKey.new()
	h_key.keycode = KEY_H
	h_key.unicode = 104
	h_key.pressed = true
	effect_count = GameState.magic_effects.size()
	main.call("_unhandled_input", h_key)
	panels = main.get("_extra_panel_nodes")
	if GameState.magic_effects.size() != effect_count + 1 or (panels.has(horse_path) and (panels[horse_path] as Control).visible):
		_fail("learned H-bound magic was swallowed by the horse-panel shortcut")
		return false
	GameState.learned_magic = []
	main.call("_unhandled_input", h_key)
	panels = main.get("_extra_panel_nodes")
	if not panels.has(horse_path) or not (panels[horse_path] as Control).visible:
		_fail("unlearned H binding did not fall back to the Godot horse-panel shortcut")
		return false
	(panels[horse_path] as Control).hide()
	GameState.learned_magic = previous_learned
	GameState.magic_keys = previous_keys
	GameState.magic_cast_times = previous_cast_times
	return true


func _test_death_and_map_filter(main: Control, resources: RefCounted) -> bool:
	var fade_monster_id := 0
	var persistent_monster_id := 0
	for monster_id_value in resources.monster_meta:
		var monster_id: int = monster_id_value
		if resources.monster_dead_fade_out(monster_id) and fade_monster_id == 0:
			fade_monster_id = monster_id
		elif not resources.monster_dead_fade_out(monster_id) and persistent_monster_id == 0:
			persistent_monster_id = monster_id
	if fade_monster_id == 0 or persistent_monster_id == 0:
		_fail("death lifecycle monster metadata unavailable")
		return false
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_action_type = 2
	GameState.update_creature(303, {
		"uid": 303, "type": 1, "monster_id": fade_monster_id,
		"x": 5, "y": 5, "direction": 5, "action_type": 2, "action_speed": 100,
	})
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(303))
	if GameState.get_creature(303).get("action_type", 0) != 13:
		_fail("creature death action was not applied")
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	if not renderer.call("_is_dead_actor", GameState.get_creature(303)):
		_fail("dead creature was not assigned to the pre-item draw pass")
		return false
	GameState.chat_log.clear()
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(101))
	if GameState.player_action_type != 13:
		_fail("player death action was not applied")
		return false
	if not GameState.chat_log.is_empty():
		_fail("player death notification invented a non-original chat entry: %s" % GameState.chat_log)
		return false
	main.call("_update_death_overlay")
	var death_overlay := main.get_node("DeathOverlay") as ColorRect
	if not death_overlay.visible or not death_overlay.color.is_equal_approx(Color(128.0 / 255.0, 0, 0, 64.0 / 255.0)):
		_fail("player death veil color or visibility mismatch: %s" % death_overlay.color)
		return false
	var command := main.get_node("ControlPanel").get_node("%Command") as LineEdit
	command.release_focus()
	var enter_key := InputEventKey.new()
	enter_key.keycode = KEY_ENTER
	enter_key.pressed = true
	main.call("_unhandled_input", enter_key)
	if not command.has_focus():
		_fail("dead-player gate blocked the original Enter chat focus")
		return false
	main.get_node("ControlPanel").call("_on_command_submitted", "")
	if command.has_focus():
		_fail("submitted HUD command retained input focus")
		return false
	var inventory_panel := main.get_node("InventoryPanel") as Control
	inventory_panel.hide()
	var panel_key := InputEventKey.new()
	panel_key.keycode = KEY_B
	panel_key.pressed = true
	main.call("_unhandled_input", panel_key)
	if not inventory_panel.visible:
		_fail("dead-player gate blocked UI panel interaction")
		return false
	inventory_panel.hide()
	main.set("_follow_focus_uid", 777)
	var blocked_click := InputEventMouseButton.new()
	blocked_click.button_index = MOUSE_BUTTON_RIGHT
	blocked_click.pressed = true
	blocked_click.position = Vector2(500, 300)
	main.call("_unhandled_input", blocked_click)
	if main.get("_follow_focus_uid") != 777:
		_fail("dead player still processed a world mouse command")
		return false
	main.call("_set_player_action", 2)
	main.call("_update_death_overlay")
	main.call("_unhandled_input", blocked_click)
	if death_overlay.visible or main.get("_follow_focus_uid") != 0:
		_fail("living action did not clear the death veil and restore mouse input")
		return false
	main.call("_on_server_message", NetworkClient.SM_OFFLINE, _map_message(303, 999, 16))
	if GameState.get_creature(303).is_empty():
		_fail("stale-map offline message removed current creature")
		return false
	main.call("_on_server_message", NetworkClient.SM_DEADFADEOUT, _map_message(303, 202, 24))
	var fading: Dictionary = GameState.get_creature(303)
	var requested_ms: int = fading.get("dead_fade_requested_ms", 0)
	if requested_ms <= 0:
		_fail("current-map dead fade did not retain and mark the corpse")
		return false
	var early_alpha: float = renderer.call("_dead_actor_alpha", fading, requested_ms + 500)
	var first_fade_alpha: float = renderer.call("_dead_actor_alpha", fading, requested_ms + 1100)
	if not is_equal_approx(early_alpha, 254.0 / 255.0) or not is_equal_approx(first_fade_alpha, 244.0 / 255.0):
		_fail("death animation gate or fade step mismatch: %s %s" % [early_alpha, first_fade_alpha])
		return false
	GameState.update_creature(304, {
		"uid": 304, "type": 1, "monster_id": persistent_monster_id,
		"x": 6, "y": 5, "direction": 5, "action_type": 13, "action_speed": 100,
		"action_started_ms": requested_ms - 2000,
	})
	main.call("_on_server_message", NetworkClient.SM_DEADFADEOUT, _map_message(304, 202, 24))
	if GameState.get_creature(304).has("dead_fade_requested_ms"):
		_fail("persistent corpse incorrectly started fading")
		return false
	renderer.call("_update_dead_fades", requested_ms + 3600)
	if not GameState.get_creature(303).is_empty() or GameState.get_creature(304).is_empty():
		_fail("faded/persistent corpse cleanup mismatch")
		return false
	GameState.remove_creature(304)
	return true


func _u64_payload(value: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(8)
	payload.encode_u64(0, value)
	return payload


func _sm_cast_magic(uid: int, map_uid: int, magic_id: int, aim_uid: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(36)
	payload.encode_u64(0, uid)
	payload.encode_u64(8, map_uid)
	payload[16] = magic_id
	payload[18] = 100
	payload[19] = 1
	payload.encode_u16(20, 3)
	payload.encode_u16(22, 4)
	payload.encode_u16(24, 3)
	payload.encode_u16(26, 4)
	payload.encode_u64(28, aim_uid)
	return payload


func _test_async_combat_feedback(main: Control, resources: RefCounted) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 3
	GameState.player_y = 4
	GameState.ascend_strings.clear()
	main.call("_on_server_message", NetworkClient.SM_MISS, _u64_payload(999))
	if not GameState.ascend_strings.is_empty():
		_fail("stale SM_MISS created false local-player feedback: %s" % GameState.ascend_strings)
		return false
	main.call("_on_server_message", NetworkClient.SM_MISS, _u64_payload(101))
	if GameState.ascend_strings.size() != 1 or GameState.ascend_strings[0].x != 3 * 48 + 24 - 20 or GameState.ascend_strings[0].y != 3 * 32 or GameState.ascend_strings[0].type != 0:
		_fail("local-player SM_MISS did not use the existing actor position: %s" % GameState.ascend_strings)
		return false
	var target_uid := 303
	GameState.update_creature(target_uid, {"uid": target_uid, "type": 1, "x": 8, "y": 9})
	main.call("_on_server_message", NetworkClient.SM_MISS, _u64_payload(target_uid))
	if GameState.ascend_strings.size() != 2 or GameState.ascend_strings[1].x != 8 * 48 + 24 - 20 or GameState.ascend_strings[1].y != 8 * 32 or GameState.ascend_strings[1].type != 0:
		_fail("creature SM_MISS did not use the existing actor position: %s" % GameState.ascend_strings)
		return false
	GameState.chat_log.clear()
	GameState.attached_magic_effects.clear()
	main.call("_on_server_message", NetworkClient.SM_CASTMAGIC, _sm_cast_magic(101, 202, 0, 101))
	if not GameState.chat_log.is_empty() or not GameState.attached_magic_effects.is_empty():
		_fail("invalid magic record created feedback: log=%s effects=%s" % [GameState.chat_log, GameState.attached_magic_effects])
		return false
	var shield_id: int = resources.magic_id("魔法盾")
	main.call("_on_server_message", NetworkClient.SM_CASTMAGIC, _sm_cast_magic(999, 202, shield_id, 0))
	if GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "使用魔法: 魔法盾" or GameState.chat_log[0].type != 1 or not GameState.attached_magic_effects.is_empty():
		_fail("valid missing-caster magic did not keep log while rejecting attachment: log=%s effects=%s" % [GameState.chat_log, GameState.attached_magic_effects])
		return false
	main.call("_on_server_message", NetworkClient.SM_CASTMAGIC, _sm_cast_magic(101, 202, shield_id, 0))
	if GameState.chat_log.size() != 2 or GameState.attached_magic_effects.size() != 1 or GameState.attached_magic_effects[0].target_uid != 101:
		_fail("valid existing-caster magic feedback was suppressed: log=%s effects=%s" % [GameState.chat_log, GameState.attached_magic_effects])
		return false
	GameState.ascend_strings.clear()
	GameState.chat_log.clear()
	GameState.attached_magic_effects.clear()
	GameState.remove_creature(target_uid)
	return true


func _sd_health_payload(uid: int, hp: int, mp: int, max_hp: int, max_mp: int) -> PackedByteArray:
	var payload := PackedByteArray([1])
	_append_u64(payload, uid)
	for value in [hp, mp, max_hp, max_mp, 0, 0]:
		var offset := payload.size()
		payload.resize(offset + 4)
		payload.encode_s32(offset, value)
	for _index in range(4):
		_append_u64(payload, 0)
	payload.append(0)
	return payload


func _test_health_feedback(main: Control, resources: RefCounted) -> bool:
	for key in [0x03000000, 0x0300000A, 0x0300000B, 0x03000010, 0x0300001A, 0x0300001B, 0x03000020, 0x0300002A, 0x0300002B, 0x03000030]:
		if resources.frame("proguse", key).is_empty():
			_fail("health feedback sprite is unavailable: %08X" % key)
			return false
	GameState.player_uid = 101
	GameState.player_x = 3
	GameState.player_y = 4
	GameState.player_health_initialized = false
	GameState.ascend_strings.clear()
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(101, 100, 40, 120, 60))
	if not GameState.ascend_strings.is_empty() or not GameState.player_health_initialized:
		_fail("first local health snapshot was not silent")
		return false
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(101, 75, 47, 120, 60))
	if GameState.ascend_strings.size() != 2:
		_fail("combined HP/MP change did not create two feedback entries: %s" % GameState.ascend_strings)
		return false
	var hp_entry: Dictionary = GameState.ascend_strings[0]
	var mp_entry: Dictionary = GameState.ascend_strings[1]
	if hp_entry.type != 1 or hp_entry.value != -25 or hp_entry.x != 3 * 48 + 24 or hp_entry.y != 3 * 32:
		_fail("HP loss feedback mismatched C++: %s" % hp_entry)
		return false
	if mp_entry.type != 2 or mp_entry.value != 7 or mp_entry.x != hp_entry.x or mp_entry.y != hp_entry.y - 32:
		_fail("simultaneous MP gain feedback did not stack above HP: %s" % mp_entry)
		return false
	GameState.ascend_strings.clear()
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(101, 85, 42, 120, 60))
	if GameState.ascend_strings.size() != 1 or GameState.ascend_strings[0].type != 3 or GameState.ascend_strings[0].value != 10:
		_fail("HP gain or silent MP loss feedback mismatched C++: %s" % GameState.ascend_strings)
		return false
	var target_uid := 303
	GameState.update_creature(target_uid, {"uid": target_uid, "type": 1, "x": 8, "y": 9, "hp": 999, "mp": 999})
	GameState.ascend_strings.clear()
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(target_uid, 50, 10, 50, 20))
	if not GameState.ascend_strings.is_empty():
		_fail("first creature health snapshot was not silent")
		return false
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(target_uid, 45, 10, 50, 20))
	if GameState.ascend_strings.size() != 1 or GameState.ascend_strings[0].type != 1 or GameState.ascend_strings[0].value != -5 or GameState.ascend_strings[0].y != 8 * 32:
		_fail("creature health delta feedback mismatch: %s" % GameState.ascend_strings)
		return false
	GameState.ascend_strings.clear()
	main.call("_on_server_message", NetworkClient.SM_HEALTH, _sd_health_payload(999, 1, 1, 1, 1))
	if not GameState.ascend_strings.is_empty():
		_fail("unknown health target created feedback")
		return false
	for value in range(1, 52):
		GameState.add_ascend_value(100, 100, 1, -value)
	if GameState.ascend_strings.size() != 51 or GameState.ascend_strings[0].value != -1:
		_fail("active combat feedback was capped before its C++ lifetime expired: %s" % GameState.ascend_strings.size())
		return false
	GameState.remove_creature(target_uid)
	GameState.ascend_strings.clear()
	return true


func _test_buff_transitions(main: Control, resources: RefCounted) -> bool:
	var shield_id: int = resources.magic_id("魔法盾")
	var buff_id: int = resources.buff_meta.keys()[0]
	var saved_uid: int = GameState.player_uid
	var saved_buffs := GameState.buff_list.duplicate(true)
	var saved_creatures := GameState.creatures.duplicate(true)
	var saved_effects := GameState.attached_magic_effects.duplicate(true)
	var local_uid: int = (5 << 59) | 21
	var remote_uid: int = (5 << 59) | 22
	var monster_uid: int = (4 << 59) | 23
	GameState.player_uid = local_uid
	GameState.buff_list = [buff_id]
	GameState.creatures = {
		remote_uid: {"uid": remote_uid, "type": 2, "buffs": [buff_id]},
		monster_uid: {"uid": monster_uid, "type": 1, "buffs": [buff_id]},
	}
	GameState.attached_magic_effects = [
		{"target_uid": local_uid, "magicID": shield_id, "kind": "shield_hit"},
		{"target_uid": local_uid, "magicID": shield_id + 1, "kind": "yin_yang_ring"},
		{"target_uid": remote_uid, "magicID": shield_id, "kind": "shield"},
		{"target_uid": monster_uid, "magicID": shield_id, "kind": "shield"},
	]
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload(local_uid, 1, 1))
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload(local_uid, 2, 2))
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload(monster_uid, 1, 2))
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload((5 << 59) | 99, 1, 2))
	main.call("_on_server_message", NetworkClient.SM_BUFF, PackedByteArray([1, 2]))
	if GameState.attached_magic_effects.size() != 4 or GameState.buff_list != [buff_id] or GameState.creatures[remote_uid].buffs != [buff_id] or GameState.creatures[monster_uid].buffs != [buff_id]:
		_fail("non-shield-off SM_BUFF changed attachments or buff IDs")
		return false
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload(local_uid, 1, 2))
	if GameState.attached_magic_effects.size() != 3 or GameState.attached_magic_effects[0].magicID != shield_id + 1 or GameState.buff_list != [buff_id]:
		_fail("local shield OFF did not remove only the shield attachment: %s" % GameState.attached_magic_effects)
		return false
	main.call("_on_server_message", NetworkClient.SM_BUFF, _sm_buff_payload(remote_uid, 1, 2))
	if GameState.attached_magic_effects.size() != 2 or GameState.attached_magic_effects.any(func(effect): return effect.get("target_uid", 0) == remote_uid) or GameState.creatures[remote_uid].buffs != [buff_id]:
		_fail("remote Hero shield OFF did not preserve list ownership: %s" % GameState.attached_magic_effects)
		return false
	main.call("_on_server_message", NetworkClient.SM_BUFFIDLIST, _buff_id_list_payload(local_uid, [buff_id, buff_id + 1]))
	main.call("_on_server_message", NetworkClient.SM_BUFFIDLIST, _buff_id_list_payload(remote_uid, []))
	if GameState.buff_list != [buff_id, buff_id + 1] or not GameState.creatures[remote_uid].buffs.is_empty() or GameState.creatures[monster_uid].buffs != [buff_id]:
		_fail("SM_BUFFIDLIST did not exclusively own displayed buff IDs")
		return false
	GameState.player_uid = saved_uid
	GameState.buff_list = saved_buffs
	GameState.creatures = saved_creatures
	GameState.attached_magic_effects = saved_effects
	return true


func _test_progression_feedback(main: Control, resources: RefCounted) -> bool:
	var saved_exp := GameState.player_exp
	var saved_gold := GameState.player_gold
	var saved_inventory := GameState.inventory.duplicate(true)
	var saved_chat := GameState.chat_log.duplicate(true)
	var control_panel: Control = main.get_node("ControlPanel")
	var saved_blinks: Dictionary = control_panel.get("_button_blinks").duplicate(true)
	GameState.chat_log.clear()
	GameState.player_exp = 0
	main.call("_on_server_message", NetworkClient.SM_EXP, _u32_payload(100))
	if GameState.player_exp != 100 or not GameState.chat_log.is_empty():
		_fail("initial experience sync produced feedback: exp=%d log=%s" % [GameState.player_exp, GameState.chat_log])
		return false
	main.call("_on_server_message", NetworkClient.SM_EXP, _u32_payload(145))
	main.call("_on_server_message", NetworkClient.SM_EXP, _u32_payload(120))
	if GameState.player_exp != 120 or GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "你获得了经验值45" or GameState.chat_log[0].type != 1:
		_fail("experience feedback did not match C++ gain-only semantics: %s" % GameState.chat_log)
		return false
	GameState.chat_log.clear()
	GameState.player_gold = 0
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	main.call("_on_server_message", NetworkClient.SM_GOLD, _u32_payload(80))
	if AudioService.last_seff_id != 0x0102006A:
		_fail("gold gain omitted original coin sound")
		return false
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	main.call("_on_server_message", NetworkClient.SM_GOLD, _u32_payload(80))
	main.call("_on_server_message", NetworkClient.SM_GOLD, _u32_payload(30))
	if AudioService.last_seff_id != AudioService.INVALID_SEFF_ID:
		_fail("unchanged or reduced gold incorrectly played the gain sound")
		return false
	if GameState.player_gold != 30 or GameState.chat_log.size() != 2 or GameState.chat_log[0].text != "你获得了80金币" or GameState.chat_log[1].text != "你失去了50金币":
		_fail("gold feedback did not match C++ change semantics: %s" % GameState.chat_log)
		return false
	var packable_id := 0
	var non_packable_id := 0
	for item_id_value in resources.item_meta:
		var candidate_id: int = item_id_value
		if resources.item_is_packable(candidate_id) and packable_id == 0:
			packable_id = candidate_id
		elif not resources.item_is_packable(candidate_id) and resources.item_type(candidate_id) != "金币" and non_packable_id == 0:
			non_packable_id = candidate_id
		if packable_id != 0 and non_packable_id != 0:
			break
	if packable_id == 0 or non_packable_id == 0:
		_fail("item feedback fixtures unavailable")
		return false
	GameState.chat_log.clear()
	GameState.inventory = [_item_record(packable_id, 71, 2)]
	control_panel.set("_button_blinks", {})
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(packable_id, 71, 5))
	if GameState.inventory[0].count != 5 or GameState.chat_log.size() != 1 or GameState.chat_log[0].text != "你获得了3个%s" % resources.item_name(packable_id):
		_fail("packable item gain feedback mismatch: inventory=%s log=%s" % [GameState.inventory, GameState.chat_log])
		return false
	if not control_panel.get("_button_blinks").has("Inventory") or AudioService.last_seff_id != resources.item_sound_effect(packable_id):
		_fail("item gain did not start original blink/sound: blink=%s seff=%08X" % [control_panel.get("_button_blinks"), AudioService.last_seff_id])
		return false
	control_panel.set("_button_blinks", {})
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(packable_id, 71, 4))
	if GameState.chat_log.size() != 2 or GameState.chat_log[1].text != "你失去了1个%s" % resources.item_name(packable_id) or not control_panel.get("_button_blinks").is_empty() or AudioService.last_seff_id != AudioService.INVALID_SEFF_ID:
		_fail("item loss incorrectly used gain-only notification: log=%s blink=%s seff=%08X" % [GameState.chat_log, control_panel.get("_button_blinks"), AudioService.last_seff_id])
		return false
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(non_packable_id, 72, 1))
	if GameState.chat_log.size() != 3 or GameState.chat_log[2].text != "你获得了%s" % resources.item_name(non_packable_id):
		_fail("non-packable item wording mismatch: %s" % GameState.chat_log)
		return false
	var item_count := GameState.inventory.size()
	var log_count := GameState.chat_log.size()
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(0xFFFFFFFE, 73, 1))
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(packable_id, 71, 100))
	main.call("_on_server_message", NetworkClient.SM_UPDATEITEM, _sd_item_payload(non_packable_id, 72, 2))
	if GameState.inventory.size() != item_count or GameState.inventory[0].count != 4 or GameState.inventory[1].count != 1 or GameState.chat_log.size() != log_count:
		_fail("invalid item update changed visible state")
		return false
	GameState.player_exp = saved_exp
	GameState.player_gold = saved_gold
	GameState.inventory = saved_inventory
	GameState.chat_log = saved_chat
	control_panel.set("_button_blinks", saved_blinks)
	AudioService.stop_seff()
	return true


func _test_ping_feedback(main: Control) -> bool:
	var saved_chat := GameState.chat_log.duplicate(true)
	var saved_pending: bool = main.get("_ping_pending")
	var saved_tick: int = main.get("_ping_tick")
	var saved_last_sent: int = main.get("_last_ping_sent_ms")
	GameState.chat_log.clear()
	main.set("_ping_pending", false)
	main.call("_handle_ping", _u32_payload(123))
	if not GameState.chat_log.is_empty():
		_fail("unsolicited ping echo created feedback")
		return false
	var current_tick: int = Time.get_ticks_msec() & 0xFFFFFFFF
	var sent_tick: int = (current_tick - 20) & 0xFFFFFFFF
	main.set("_ping_pending", true)
	main.set("_ping_tick", sent_tick)
	main.call("_handle_ping", _u32_payload(sent_tick))
	if main.get("_ping_pending") or GameState.chat_log.size() != 1 or GameState.chat_log[0].type != 1 or not GameState.chat_log[0].text.begins_with("延迟") or not GameState.chat_log[0].text.ends_with("ms"):
		_fail("matching ping echo did not complete with original feedback: %s" % GameState.chat_log)
		return false
	var elapsed: int = GameState.chat_log[0].text.trim_prefix("延迟").trim_suffix("ms").to_int()
	if elapsed < 20 or elapsed > 1000:
		_fail("ping latency was not derived from the wire timestamp: %d" % elapsed)
		return false
	main.call("_handle_ping", _u32_payload(sent_tick))
	if GameState.chat_log.size() != 1:
		_fail("duplicate ping echo created repeated feedback")
		return false
	main.set("_ping_pending", true)
	main.set("_ping_tick", 123)
	main.call("_handle_ping", _u32_payload(124))
	main.call("_handle_ping", PackedByteArray([1, 2]))
	if not main.get("_ping_pending") or GameState.chat_log.size() != 1:
		_fail("mismatched or malformed ping echo changed pending state")
		return false
	main.set("_ping_pending", saved_pending)
	main.set("_ping_tick", saved_tick)
	main.set("_last_ping_sent_ms", saved_last_sent)
	GameState.chat_log = saved_chat
	return true


func _u32_payload(value: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(4)
	payload.encode_u32(0, value)
	return payload


func _u16_payload(value: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(2)
	payload.encode_u16(0, value)
	return payload


func _item_error_payload(item_id: int, seq_id: int, error: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(10)
	payload.encode_u32(0, item_id)
	payload.encode_u32(4, seq_id)
	payload.encode_u16(8, error)
	return payload


func _buy_succeed_payload(npc_uid: int, item_id: int, seq_id: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(16)
	payload.encode_u64(0, npc_uid)
	payload.encode_u32(8, item_id)
	payload.encode_u32(12, seq_id)
	return payload


func _buy_error_payload(npc_uid: int, item_id: int, seq_id: int, error: int) -> PackedByteArray:
	var payload := _buy_succeed_payload(npc_uid, item_id, seq_id)
	payload.resize(18)
	payload.encode_u16(16, error)
	return payload


func _sm_buff_payload(uid: int, type: int, state: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(16)
	payload.encode_u64(0, uid)
	payload.encode_u32(8, type)
	payload.encode_u32(12, state)
	return payload


func _buff_id_list_payload(uid: int, ids: Array) -> PackedByteArray:
	var payload := PackedByteArray([1])
	_append_u64(payload, uid)
	_append_u64(payload, ids.size())
	for id_value in ids:
		_append_u32(payload, id_value)
	payload.append(0)
	return payload


func _item_record(item_id: int, seq_id: int, count: int) -> Dictionary:
	return {"itemID": item_id, "seqID": seq_id, "count": count, "duration": [0, 0], "extAttrList": {}}


func _sd_item_payload(item_id: int, seq_id: int, count: int) -> PackedByteArray:
	var payload := PackedByteArray([1])
	_append_u32(payload, item_id)
	_append_u32(payload, seq_id)
	_append_u64(payload, count)
	_append_u64(payload, 0)
	_append_u64(payload, 0)
	_append_u64(payload, 0)
	payload.append(0)
	return payload


func _append_u32(payload: PackedByteArray, value: int) -> void:
	var offset := payload.size()
	payload.resize(offset + 4)
	payload.encode_u32(offset, value)


func _append_u64(payload: PackedByteArray, value: int) -> void:
	var offset := payload.size()
	payload.resize(offset + 8)
	payload.encode_u64(offset, value)


func _test_shield_hit_action(main: Control, resources: RefCounted) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 3
	GameState.player_y = 4
	GameState.attached_magic_effects.clear()
	var shield_id: int = resources.magic_id("魔法盾")
	if not GameState.add_cast_magic_attachment({"magic": shield_id, "uid": 101}, "魔法盾"):
		_fail("shield action fixture could not attach shield")
		return false
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 202, {
		"type": 11, "speed": 100, "direction": 5, "x": 3, "y": 4, "fromUID": 303,
	}))
	var shield: Dictionary = GameState.attached_magic_effects[0]
	if GameState.player_action_type != 11 or shield.get("kind", "") != "shield_hit" or shield.get("stage", 0) != 5:
		_fail("SM_ACTION ACTION_HITTED did not switch the shield stage: %s" % shield)
		return false
	GameState.attached_magic_effects.clear()
	main.set("_player_action_timer", -1.0)
	GameState.player_action_type = 2
	return true


func _test_spinkick_direction(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 3
	GameState.player_y = 4
	GameState.player_direction = 5
	GameState.update_creature(303, {
		"uid": 303, "type": 1, "x": 4, "y": 4, "direction": 5, "action_type": 2,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 202, {
		"type": 12, "speed": 100, "x": 3, "y": 4, "aimUID": 303,
	}))
	if GameState.player_action_type != 12 or GameState.player_direction != 7:
		_fail("ACTION_SPINKICK did not face away from its adjacent target like C++")
		return false
	GameState.player_direction = 5
	GameState.update_creature(303, {
		"uid": 303, "type": 1, "x": 8, "y": 4, "direction": 5, "action_type": 2,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 202, {
		"type": 12, "speed": 100, "x": 3, "y": 4, "aimUID": 303,
	}))
	if GameState.player_direction != 5:
		_fail("non-adjacent ACTION_SPINKICK incorrectly replaced the retained direction")
		return false
	GameState.remove_creature(303)
	main.set("_player_action_timer", -1.0)
	GameState.player_action_type = 2
	return true


func _test_actor_record_lifecycle(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	var monster_uid: int = (4 << 59) | (224 << 35) | 303
	GameState.update_creature(monster_uid, {
		"uid": monster_uid, "type": 1, "monster_id": 224,
		"x": 5, "y": 6, "direction": 3, "action_type": 2,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(monster_uid, 202, {
		"type": 1, "speed": 100, "direction": 7, "x": 20, "y": 21,
	}))
	var retained: Dictionary = GameState.get_creature(monster_uid)
	if retained.get("x", 0) != 5 or retained.get("y", 0) != 6 or retained.get("direction", 0) != 3 or retained.get("action_type", 0) != 2:
		_fail("late ACTION_SPAWN reset an existing actor: %s" % retained)
		return false

	var player_uid: int = (5 << 59) | 404
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(player_uid, 202, {
		"type": 3, "speed": 100, "direction": 5, "x": 8, "y": 9, "aimX": 9, "aimY": 9,
	}))
	if not GameState.get_creature(player_uid).is_empty():
		_fail("unknown player action created a blank phantom before SM_COREORD")
		return false
	var player_union := PackedByteArray([5, 18, 0, 0, 0])
	main.call("_on_server_message", NetworkClient.SM_COREORD, _sm_corecord(player_uid, 202, {
		"type": 2, "speed": 100, "direction": 6, "x": 9, "y": 9,
	}, player_union))
	var resolved: Dictionary = GameState.get_creature(player_uid)
	if resolved.get("type", 0) != 2 or resolved.get("gender", 0) != 1 or resolved.get("job", 0) != 2 or resolved.get("level", 0) != 18:
		_fail("matching SM_COREORD did not create the queried player: %s" % resolved)
		return false

	var stale_uid: int = (5 << 59) | 405
	main.call("_on_server_message", NetworkClient.SM_COREORD, _sm_corecord(stale_uid, 999, {
		"type": 2, "speed": 100, "direction": 5, "x": 30, "y": 31,
	}, player_union))
	if not GameState.get_creature(stale_uid).is_empty():
		_fail("stale-map SM_COREORD inserted an actor into the current world")
		return false
	var new_monster_uid: int = (4 << 59) | (225 << 35) | 406
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(new_monster_uid, 202, {
		"type": 2, "speed": 100, "direction": 4, "x": 11, "y": 12,
	}))
	var new_npc_uid: int = (3 << 59) | (7 << 35) | 407
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(new_npc_uid, 202, {
		"type": 1, "speed": 100, "direction": 1, "x": 13, "y": 14,
	}))
	if GameState.get_creature(new_monster_uid).get("monster_id", 0) != 225 or GameState.get_creature(new_monster_uid).get("action_type", 0) != 2 or GameState.get_creature(new_npc_uid).get("npc_id", 0) != 7:
		_fail("player-specific record query suppressed unknown monster or NPC creation")
		return false
	GameState.remove_creature(monster_uid)
	GameState.remove_creature(player_uid)
	GameState.remove_creature(new_monster_uid)
	GameState.remove_creature(new_npc_uid)
	return true


func _test_npc_actions(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	var renderer: Control = main.get_node("WorldRenderer")
	var npc_uid: int = (3 << 59) | (7 << 35) | 408
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(npc_uid, 202, {
		"type": 1, "speed": 100, "direction": 8, "x": 13, "y": 14,
	}))
	var npc: Dictionary = GameState.get_creature(npc_uid)
	var sequence: Dictionary = renderer.call("_npc_render_sequence", npc)
	if npc.get("npc_motion", -1) != 0 or sequence != {"motion": 0, "direction": 7, "count": 4}:
		_fail("NPC spawn did not select the exact stand motion/view: npc=%s sequence=%s" % [npc, sequence])
		return false
	var ext_act := PackedByteArray([1, 0, 0, 0, 0, 0, 0, 0])
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(npc_uid, 202, {
		"type": 2, "speed": 150, "direction": 4, "x": 13, "y": 14, "extParam": ext_act,
	}))
	npc = GameState.get_creature(npc_uid)
	sequence = renderer.call("_npc_render_sequence", npc)
	if npc.get("npc_motion", -1) != 1 or npc.get("action_speed", 0) != 100 or sequence != {"motion": 1, "direction": 3, "count": 4}:
		_fail("NPC stand act did not preserve motion/view metadata: npc=%s sequence=%s" % [npc, sequence])
		return false
	var ext_act_ext := PackedByteArray([2, 0, 0, 0, 0, 0, 0, 0])
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(npc_uid, 202, {
		"type": 2, "speed": 100, "direction": 1, "x": 13, "y": 14, "extParam": ext_act_ext,
	}))
	npc = GameState.get_creature(npc_uid)
	sequence = renderer.call("_npc_render_sequence", npc)
	if npc.get("npc_motion", -1) != 2 or sequence.count != 0 or main.call("_creature_action_duration", 2, 100, npc) >= 0.0:
		_fail("normal NPC ACTEXT did not preserve the original empty sequence: npc=%s sequence=%s" % [npc, sequence])
		return false
	var special_uid: int = (3 << 59) | (56 << 35) | 409
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(special_uid, 202, {
		"type": 1, "speed": 100, "direction": 1, "x": 15, "y": 16,
	}))
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(special_uid, 202, {
		"type": 2, "speed": 100, "direction": 1, "x": 15, "y": 16, "extParam": ext_act_ext,
	}))
	var special: Dictionary = GameState.get_creature(special_uid)
	sequence = renderer.call("_npc_render_sequence", special)
	if sequence.count != 12 or not is_equal_approx(float(main.call("_creature_action_duration", 2, 100, special)), 1.2):
		_fail("six-face-stone NPC did not use its original twelve-frame sequence: npc=%s sequence=%s" % [special, sequence])
		return false
	main.call("_finish_creature_action", special_uid, 2, special.action_started_ms)
	if GameState.get_creature(special_uid).get("npc_motion", -1) != 1:
		_fail("completed NPC ACTEXT did not enter the original ACT idle motion")
		return false
	for special_id in [59, 64, 65]:
		sequence = renderer.call("_npc_render_sequence", {"npc_id": special_id, "npc_motion": 0, "direction": 1})
		if sequence.count != 1:
			_fail("single-frame NPC %d did not retain its exact count: %s" % [special_id, sequence])
			return false
	GameState.remove_creature(npc_uid)
	GameState.remove_creature(special_uid)
	return true


func _test_monster_spawn_actions(main: Control, resources: RefCounted) -> bool:
	var special_id := 0
	for monster_id_value in resources.monster_meta:
		if resources.monster_spawn_look(int(monster_id_value)) > 0:
			special_id = int(monster_id_value)
			break
	if special_id == 0:
		_fail("special monster spawn metadata unavailable")
		return false
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	var special_uid: int = (4 << 59) | (special_id << 35) | 703
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(special_uid, 202, {
		"type": 1, "speed": 100, "direction": 5, "x": 32, "y": 33,
	}))
	var special: Dictionary = GameState.get_creature(special_uid)
	if special.get("action_type", 0) != 1 or special.get("monster_stand_look", 0) != resources.monster_spawn_look(special_id):
		_fail("TaoDog spawn did not retain its redirected special motion/look: %s" % special)
		return false
	if not is_equal_approx(float(main.call("_action_duration", 1, 100, 1)), 1.0):
		_fail("special monster spawn did not use the C++ ten-frame duration")
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	if renderer.call("_monster_motion", special.get("action_type", 0)) != PackedInt32Array([8, 10]):
		_fail("special monster spawn did not select MOTION_MON_SPAWN graphics")
		return false
	var ordinary_id := 0
	for monster_id_value in resources.monster_meta:
		var candidate_id: int = monster_id_value
		if resources.monster_spawn_look(candidate_id) == 0:
			ordinary_id = candidate_id
			break
	if ordinary_id == 0:
		_fail("ordinary monster spawn fixture unavailable")
		return false
	var runtime_resources: RefCounted = main.get("_resources")
	var ordinary_meta: PackedInt32Array = runtime_resources.monster_meta[ordinary_id]
	var ordinary_spawn_seff: int = ordinary_meta[2]
	ordinary_meta[2] = 0xFFFFFFFF
	runtime_resources.monster_meta[ordinary_id] = ordinary_meta
	var ordinary_uid: int = (4 << 59) | (ordinary_id << 35) | 704
	var ordinary_union := PackedByteArray()
	ordinary_union.resize(4)
	ordinary_union.encode_u32(0, ordinary_id)
	main.call("_on_server_message", NetworkClient.SM_COREORD, _sm_corecord(ordinary_uid, 202, {
		"type": 1, "speed": 100, "direction": 3, "x": 34, "y": 35,
	}, ordinary_union))
	var ordinary: Dictionary = GameState.get_creature(ordinary_uid)
	ordinary_meta[2] = ordinary_spawn_seff
	runtime_resources.monster_meta[ordinary_id] = ordinary_meta
	if ordinary.get("monster_id", 0) != ordinary_id or ordinary.get("action_type", 0) != 2:
		_fail("ordinary SM_COREORD spawn did not create a standing monster: %s" % ordinary)
		return false
	GameState.remove_creature(special_uid)
	GameState.remove_creature(ordinary_uid)
	return true


func _test_monster_transform_actions(main: Control, resources: RefCounted) -> bool:
	var monster_id := 0
	var hidden_focusable_id := 0
	for monster_id_value in resources.monster_meta:
		var candidate_id: int = monster_id_value
		var candidate: Dictionary = resources.monster_transform(candidate_id)
		if not candidate.is_empty() and candidate.hidden_focusable:
			hidden_focusable_id = candidate_id
		if not candidate.is_empty() and not candidate.hidden_focusable:
			monster_id = candidate_id
	if monster_id == 0 or hidden_focusable_id == 0:
		_fail("transformed monster focus fixtures unavailable")
		return false
	var transform: Dictionary = resources.monster_transform(monster_id)
	var runtime_resources: RefCounted = main.get("_resources")
	var meta: PackedInt32Array = runtime_resources.monster_meta[monster_id]
	var spawn_seff: int = meta[2]
	meta[2] = 0xFFFFFFFF
	runtime_resources.monster_meta[monster_id] = meta
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	var uid: int = (4 << 59) | (monster_id << 35) | 705
	var union_data := PackedByteArray()
	union_data.resize(4)
	union_data.encode_u32(0, monster_id)
	main.call("_on_server_message", NetworkClient.SM_COREORD, _sm_corecord(uid, 202, {
		"type": 1, "speed": 100, "direction": 5, "x": 40, "y": 41,
	}, union_data))
	var creature: Dictionary = GameState.get_creature(uid)
	var renderer: Control = main.get_node("WorldRenderer")
	var sequence: Dictionary = renderer.call("_monster_render_sequence", creature)
	if creature.get("action_type", 0) != 2 or creature.get("monster_stand_mode", true) or sequence.focusable or sequence.motion != transform.hidden_stand[0] or sequence.begin != transform.hidden_stand[1]:
		_fail("monster spawn did not enter the C++ hidden form: creature=%s sequence=%s" % [creature, sequence])
		return false
	var tao_sequence: Dictionary = renderer.call("_monster_render_sequence", {
		"monster_id": hidden_focusable_id, "action_type": 2, "monster_stand_mode": false, "direction": 5,
	})
	var tao_transform: Dictionary = resources.monster_transform(hidden_focusable_id)
	if not tao_sequence.focusable or tao_sequence.look != tao_transform.hidden_look or tao_sequence.count != 4:
		_fail("TaoDog hidden form lost its redirected look or focusability: %s" % tao_sequence)
		return false
	var ext_active := PackedByteArray()
	ext_active.resize(8)
	ext_active[0] = 1
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 2, "speed": 100, "direction": 5, "x": 40, "y": 41, "extParam": ext_active,
	}))
	creature = GameState.get_creature(uid)
	sequence = renderer.call("_monster_render_sequence", creature)
	if creature.get("action_type", 0) != 10 or not creature.get("monster_stand_mode", false) or sequence.motion != transform.active_transform[0] or sequence.begin != transform.active_transform[1] or sequence.reverse != transform.active_reverse or not sequence.focusable:
		_fail("ACTION_STAND did not queue the active-form transformation: creature=%s sequence=%s" % [creature, sequence])
		return false
	var expected_duration := float(transform.active_transform[2]) * 0.1
	if not is_equal_approx(float(main.call("_creature_action_duration", 10, 100, creature)), expected_duration):
		_fail("active transformation duration mismatch")
		return false
	var started_ms: int = creature.action_started_ms
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 10, "speed": 100, "direction": 5, "x": 40, "y": 41, "extParam": ext_active,
	}))
	if GameState.get_creature(uid).get("action_started_ms", 0) != started_ms:
		_fail("redundant ACTION_TRANSF restarted an existing form")
		return false
	var ext_hidden := PackedByteArray()
	ext_hidden.resize(8)
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 10, "speed": 100, "direction": 5, "x": 40, "y": 41, "extParam": ext_hidden,
	}))
	creature = GameState.get_creature(uid)
	sequence = renderer.call("_monster_render_sequence", creature)
	if creature.get("action_type", 0) != 10 or not creature.get("monster_stand_mode", false) or creature.get("monster_pending_form_modes", []) != [false] or sequence.motion != transform.active_transform[0]:
		_fail("ACTION_TRANSF did not queue behind the active transformation: creature=%s sequence=%s" % [creature, sequence])
		return false
	main.call("_finish_creature_action", uid, 10, creature.action_started_ms)
	creature = GameState.get_creature(uid)
	sequence = renderer.call("_monster_render_sequence", creature)
	if creature.get("action_type", 0) != 10 or creature.get("monster_stand_mode", true) or sequence.motion != transform.hidden_transform[0] or sequence.begin != transform.hidden_transform[1] or sequence.reverse != transform.hidden_reverse or sequence.focusable:
		_fail("queued hidden transformation did not start after active transformation: creature=%s sequence=%s" % [creature, sequence])
		return false
	main.call("_finish_creature_action", uid, 10, creature.action_started_ms)
	creature = GameState.get_creature(uid)
	var idle_started_ms: int = creature.action_started_ms
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 10, "speed": 100, "direction": 3, "x": 99, "y": 98, "extParam": ext_hidden,
	}))
	creature = GameState.get_creature(uid)
	if creature.get("action_type", 0) != 2 or creature.action_started_ms != idle_started_ms or creature.x != 40 or creature.y != 41:
		_fail("redundant idle ACTION_TRANSF changed the current motion or position: %s" % creature)
		return false
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 7, "speed": 100, "direction": 5, "x": 40, "y": 41,
	}))
	creature = GameState.get_creature(uid)
	sequence = renderer.call("_monster_render_sequence", creature)
	if creature.get("action_type", 0) != 10 or not creature.get("monster_stand_mode", false) or creature.get("monster_pending_action", {}).get("type", 0) != 7 or sequence.motion != transform.active_transform[0]:
		_fail("hidden monster attack did not queue transformation before attack: %s" % creature)
		return false
	var reveal_started_ms: int = creature.action_started_ms
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 7, "speed": 150, "direction": 6, "x": 44, "y": 45,
	}))
	creature = GameState.get_creature(uid)
	var replaced_pending: Dictionary = creature.get("monster_pending_action", {})
	if creature.get("action_type", 0) != 10 or creature.action_started_ms != reveal_started_ms or creature.x != 40 or creature.y != 41 or replaced_pending.get("speed", 0) != 150 or replaced_pending.get("state", {}).get("x", 0) != 44:
		_fail("new attack did not replace the pending action while preserving the active transformation: %s" % creature)
		return false
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 2, "speed": 100, "direction": 5, "x": 40, "y": 41, "extParam": ext_hidden,
	}))
	creature = GameState.get_creature(uid)
	if creature.get("action_type", 0) != 10 or not creature.get("monster_stand_mode", false) or creature.get("monster_pending_form_modes", []) != [false] or creature.has("monster_pending_action"):
		_fail("new stand request did not cancel the queued monster attack: %s" % creature)
		return false
	main.call("_finish_creature_action", uid, 10, creature.action_started_ms)
	creature = GameState.get_creature(uid)
	main.call("_finish_creature_action", uid, 10, creature.action_started_ms)
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 7, "speed": 100, "direction": 5, "x": 40, "y": 41,
	}))
	creature = GameState.get_creature(uid)
	if creature.get("monster_pending_action", {}).get("type", 0) != 7:
		_fail("hidden monster attack was not queued after interruption recovery: %s" % creature)
		return false
	main.call("_finish_creature_action", uid, 10, creature.action_started_ms)
	creature = GameState.get_creature(uid)
	if creature.get("action_type", 0) != 7 or creature.has("monster_pending_action"):
		_fail("queued monster attack did not start after transformation: %s" % creature)
		return false
	var attack_started_ms: int = creature.action_started_ms
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(uid, 202, {
		"type": 10, "speed": 100, "direction": 2, "x": 91, "y": 92, "extParam": ext_active,
	}))
	creature = GameState.get_creature(uid)
	if creature.get("action_type", 0) != 7 or creature.action_started_ms != attack_started_ms or creature.x != 40 or creature.y != 41:
		_fail("redundant transform interrupted or restarted the current attack: %s" % creature)
		return false
	var reveal_on_hit_id := 0
	for monster_id_value in resources.monster_meta:
		if resources.monster_transform(int(monster_id_value)).get("reveal_on_hit", false):
			reveal_on_hit_id = int(monster_id_value)
			break
	if reveal_on_hit_id == 0:
		_fail("reveal-on-hit monster fixture unavailable")
		return false
	var hit_meta: PackedInt32Array = runtime_resources.monster_meta[reveal_on_hit_id]
	var hit_spawn_seff: int = hit_meta[2]
	hit_meta[2] = 0xFFFFFFFF
	runtime_resources.monster_meta[reveal_on_hit_id] = hit_meta
	var hit_uid: int = (4 << 59) | (reveal_on_hit_id << 35) | 706
	GameState.update_creature(hit_uid, {
		"uid": hit_uid, "type": 1, "monster_id": reveal_on_hit_id, "x": 42, "y": 43,
		"direction": 1, "action_type": 2, "action_started_ms": Time.get_ticks_msec(), "monster_stand_mode": false,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(hit_uid, 202, {
		"type": 11, "speed": 100, "direction": 1, "x": 42, "y": 43,
	}))
	var hit_creature: Dictionary = GameState.get_creature(hit_uid)
	if hit_creature.get("action_type", 0) != 10 or hit_creature.get("monster_pending_action", {}).get("type", 0) != 11:
		_fail("EvilCentipede hit did not queue transformation before reaction: %s" % hit_creature)
		return false
	var die_seff: int = meta[5]
	meta[5] = 0xFFFFFFFF
	runtime_resources.monster_meta[monster_id] = meta
	var death_uid: int = (4 << 59) | (monster_id << 35) | 707
	GameState.update_creature(death_uid, {
		"uid": death_uid, "type": 1, "monster_id": monster_id, "x": 50, "y": 51,
		"direction": 1, "action_type": 2, "action_started_ms": Time.get_ticks_msec(), "monster_stand_mode": false,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(death_uid, 202, {
		"type": 7, "speed": 100, "direction": 1, "x": 52, "y": 53,
	}))
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(death_uid))
	var death_creature: Dictionary = GameState.get_creature(death_uid)
	if death_creature.get("action_type", 0) != 10 or death_creature.x != 50 or death_creature.y != 51 or death_creature.has("monster_pending_action") or death_creature.get("monster_pending_forced_action", {}).get("type", 0) != 13:
		_fail("death did not wait behind the forced transformation: %s" % death_creature)
		return false
	var forced_started_ms: int = death_creature.action_started_ms
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(death_uid, 202, {
		"type": 2, "speed": 100, "direction": 1, "x": 10, "y": 10, "extParam": ext_active,
	}))
	if GameState.get_creature(death_uid).action_started_ms != forced_started_ms:
		_fail("action received behind a queued death was not ignored")
		return false
	main.call("_finish_creature_action", death_uid, 10, death_creature.action_started_ms)
	death_creature = GameState.get_creature(death_uid)
	if death_creature.get("action_type", 0) != 13 or death_creature.x != 50 or death_creature.y != 51 or death_creature.has("monster_pending_forced_action"):
		_fail("queued death did not start after the forced transformation: %s" % death_creature)
		return false
	var flush_uid: int = (4 << 59) | (monster_id << 35) | 708
	GameState.update_creature(flush_uid, {
		"uid": flush_uid, "type": 1, "monster_id": monster_id, "x": 60, "y": 61,
		"direction": 1, "action_type": 2, "action_started_ms": Time.get_ticks_msec(), "monster_stand_mode": false,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(flush_uid, 202, {
		"type": 7, "speed": 100, "direction": 1, "x": 60, "y": 61,
	}))
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(flush_uid, 202, {
		"type": 4, "speed": 100, "direction": 3, "x": 62, "y": 63,
	}))
	var flush_creature: Dictionary = GameState.get_creature(flush_uid)
	if flush_creature.get("action_type", 0) != 2 or flush_creature.x != 62 or flush_creature.y != 63 or not flush_creature.get("monster_stand_mode", false) or flush_creature.has("monster_pending_action") or flush_creature.has("monster_pending_form_modes"):
		_fail("ACTION_JUMP did not flush the forced transformation queue: %s" % flush_creature)
		return false
	flush_creature["x"] = 70
	flush_creature["y"] = 71
	flush_creature["action_type"] = 2
	flush_creature["monster_stand_mode"] = false
	GameState.update_creature(flush_uid, flush_creature)
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(flush_uid, 202, {
		"type": 7, "speed": 100, "direction": 1, "x": 70, "y": 71,
	}))
	var effect_count := GameState.magic_effects.size()
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(flush_uid, 202, {
		"type": 6, "speed": 100, "direction": 4, "x": 70, "y": 71, "aimX": 75, "aimY": 76,
	}))
	flush_creature = GameState.get_creature(flush_uid)
	if flush_creature.get("action_type", 0) != 6 or flush_creature.x != 75 or flush_creature.y != 76 or not flush_creature.get("monster_stand_mode", false) or flush_creature.has("monster_pending_action"):
		_fail("ACTION_SPACEMOVE did not flush the forced transformation queue: %s" % flush_creature)
		return false
	GameState.magic_effects.resize(effect_count)
	meta[2] = spawn_seff
	meta[5] = die_seff
	runtime_resources.monster_meta[monster_id] = meta
	hit_meta[2] = hit_spawn_seff
	runtime_resources.monster_meta[reveal_on_hit_id] = hit_meta
	GameState.remove_creature(uid)
	GameState.remove_creature(hit_uid)
	GameState.remove_creature(death_uid)
	GameState.remove_creature(flush_uid)
	return true


func _test_monster_jump_stands(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	var known_uid: int = (4 << 59) | (224 << 35) | 701
	GameState.update_creature(known_uid, {
		"uid": known_uid, "type": 1, "monster_id": 224,
		"x": 5, "y": 6, "direction": 3, "action_type": 7,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(known_uid, 202, {
		"type": 4, "speed": 100, "direction": 7, "x": 20, "y": 21,
	}))
	var known: Dictionary = GameState.get_creature(known_uid)
	if known.get("x", 0) != 20 or known.get("y", 0) != 21 or known.get("direction", 0) != 7 or known.get("action_type", 0) != 2:
		_fail("known monster ACTION_JUMP did not become stand at action position: %s" % known)
		return false

	var new_uid: int = (4 << 59) | (225 << 35) | 702
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(new_uid, 202, {
		"type": 4, "speed": 100, "direction": 5, "x": 30, "y": 31,
	}))
	var created: Dictionary = GameState.get_creature(new_uid)
	if created.get("monster_id", 0) != 225 or created.get("x", 0) != 30 or created.get("y", 0) != 31 or created.get("direction", 0) != 5 or created.get("action_type", 0) != 2:
		_fail("new monster ACTION_JUMP did not create a standing actor: %s" % created)
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	var now := Time.get_ticks_msec()
	if renderer.call("_motion_frame", created.get("action_type", 0), 4, now - 500, 100) != 1:
		_fail("monster jump stand motion did not remain cyclic")
		return false
	GameState.remove_creature(known_uid)
	GameState.remove_creature(new_uid)
	return true


func _test_self_action_map_transition(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 25 << 35
	GameState.player_map_id = 25
	GameState.player_x = 10
	GameState.player_y = 11
	GameState.update_creature(303, {"uid": 303, "type": 1, "x": 11, "y": 11, "action_type": 2})
	GameState.ground_items["10,11"] = [1]
	GameState.firewalls = [{"x": 10, "y": 11}]
	GameState.magic_effects = [{"uid": 303}]
	GameState.attached_magic_effects = [{"uid": 303}, {"uid": 101, "kind": "self"}]
	GameState.strike_grids["10,11"] = 1
	GameState.ascend_strings = [{"x": 10, "y": 11, "text": "1"}]
	GameState.player_say_messages[303] = [{"text": "old"}]
	GameState.player_say_messages[101] = [{"text": "self"}]
	var next_map_uid: int = 24 << 35
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(303, next_map_uid, {
		"type": 2, "speed": 100, "direction": 5, "x": 30, "y": 31,
	}))
	if GameState.player_map_id != 25 or GameState.get_creature(303).is_empty():
		_fail("foreign stale-map action changed the local world")
		return false
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, next_map_uid, {
		"type": 2, "speed": 100, "direction": 6, "x": 30, "y": 31,
	}))
	if GameState.player_map_uid != next_map_uid or GameState.player_map_id != 24 or GameState.player_x != 30 or GameState.player_y != 31 or GameState.player_direction != 6:
		_fail("local different-map action did not switch and continue: %s %s %s" % [GameState.player_map_id, GameState.player_x, GameState.player_y])
		return false
	if not GameState.creatures.is_empty() or not GameState.ground_items.is_empty() or not GameState.firewalls.is_empty() or not GameState.magic_effects.is_empty() or GameState.attached_magic_effects.size() != 1 or GameState.attached_magic_effects[0].get("uid", 0) != 101 or not GameState.strike_grids.is_empty() or not GameState.ascend_strings.is_empty() or GameState.player_say_messages.keys() != [101]:
		_fail("map transition retained old-world transient state")
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	if renderer.world_resource.map_id != 24 or renderer.map_width <= 0 or AudioService.current_bgm_id != renderer.world_resource.bgm_id:
		_fail("map transition did not reload map resources and BGM")
		return false
	return true


func _test_world_displacement(main: Control, resources: RefCounted) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_x = 3
	GameState.player_y = 4
	GameState.magic_effects.clear()
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 202, {
		"type": 6, "speed": 100, "direction": 5, "x": 3, "y": 4, "aimX": 20, "aimY": 21,
	}))
	if GameState.player_x != 20 or GameState.player_y != 21 or GameState.player_action_type != 6:
		_fail("space move did not jump immediately to aim grid")
		return false
	var stale_uid: int = (4 << 59) | (224 << 35) | 605
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(stale_uid, 24 << 35, {
		"type": 6, "speed": 100, "direction": 5, "x": 20, "y": 21, "aimX": 30, "aimY": 31,
	}))
	if GameState.player_x != 20 or GameState.player_y != 21:
		_fail("foreign stale-map world action changed current player position")
		return false
	if GameState.magic_effects.is_empty() or GameState.magic_effects.back().get("magicID", 0) != resources.magic_id("瞬息移动"):
		_fail("space move did not create original teleport effect")
		return false
	var remote_uid: int = (5 << 59) | 606
	GameState.update_creature(remote_uid, {
		"uid": remote_uid, "type": 2, "gender": 0, "job": 0,
		"x": 6, "y": 7, "direction": 3, "action_type": 2,
	})
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(remote_uid, 202, {
		"type": 5, "speed": 100, "direction": 3, "x": 6, "y": 7, "aimX": 8, "aimY": 7,
	}))
	var remote: Dictionary = GameState.get_creature(remote_uid)
	if remote.get("x", 0) != 8 or remote.get("y", 0) != 7:
		_fail("push move did not retain aim grid as logical position")
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	var pushed: Vector2 = renderer.call("_action_draw_grid", 8, 7, 6, 7, 5, Time.get_ticks_msec() - 1000, 100)
	if not is_equal_approx(pushed.x, 8.0):
		_fail("push move rendering did not interpolate to aim grid")
		return false
	if renderer.call("_hero_motion", 14) != PackedInt32Array([10, 6]) or not is_equal_approx(float(main.call("_action_duration", 14, 100, 2)), 0.9):
		_fail("mine action did not use C++ two-handed swing and attack-mode timing")
		return false
	main.call("_process_player_action", 0.02)
	GameState.remove_creature(remote_uid)
	main.call("_cancel_movement")
	return true


func _sm_action(uid: int, map_uid: int, action: Dictionary) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(43)
	payload.encode_u64(0, uid)
	payload.encode_u64(8, map_uid)
	var encoded_action := Protocol.encode_action_node(action)
	for index in range(encoded_action.size()):
		payload[16 + index] = encoded_action[index]
	return payload


func _sm_corecord(uid: int, map_uid: int, action: Dictionary, union_data: PackedByteArray) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(48)
	payload.encode_u64(0, uid)
	payload.encode_u64(8, map_uid)
	var action_data := Protocol.encode_action_node(action)
	for index in action_data.size():
		payload[16 + index] = action_data[index]
	for index in mini(5, union_data.size()):
		payload[43 + index] = union_data[index]
	return payload


func _map_message(uid: int, map_uid: int, size: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(size)
	payload.encode_u64(0, uid)
	payload.encode_u64(8, map_uid)
	return payload


func _test_path_decomposition() -> bool:
	var pathfinder: RefCounted = WorldPathfinderScript.new()
	var goals: Array[Vector2i] = [Vector2i(4, 2)]
	var path: Array[Vector2i] = pathfinder.find_path(Vector2i(0, 2), goals, _test_walkable, {})
	if path.is_empty() or path.back() != goals[0]:
		_fail("pathfinder did not reach destination: %s" % path)
		return false
	var previous := Vector2i(0, 2)
	for point in path:
		if maxi(absi(point.x - previous.x), absi(point.y - previous.y)) != 1:
			_fail("path contains non one-hop movement: %s" % path)
			return false
		previous = point
	return true


func _test_walkable(x: int, y: int) -> bool:
	if x < 0 or y < 0 or x > 4 or y > 4:
		return false
	return not (x == 2 and y in [1, 2, 3])


func _test_chase_retry(main: Control) -> bool:
	GameState.player_x = 0
	GameState.player_y = 0
	GameState.update_creature(404, {"uid": 404, "x": 5, "y": 5, "type": 1})
	main.set("_next_strike", true)
	main.call("_start_chase", 404)
	main.call("_process_movement", 1.0)
	if main.get("_chase_target_uid") != 404 or not main.get("_next_strike"):
		_fail("blocked chase did not retain target/next strike")
		return false
	GameState.update_creature(404, {"uid": 404, "x": 1, "y": 0, "type": 1})
	main.call("_process_movement", 1.0)
	if main.get("_chase_target_uid") != 404 or main.get("_next_strike") or GameState.player_action_type != 7 or float(main.get("_player_action_timer")) < 0.89:
		_fail("adjacent chase did not attack and consume next strike")
		return false
	main.call("_process_player_action", 1.0)
	if GameState.player_action_type != 2:
		_fail("local attack did not return to stand")
		return false
	main.call("_cancel_movement")
	return true


func _test_exact_frame_input(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_x = 0
	GameState.player_y = 0
	GameState.grabbed_item = {"itemID": 1, "seqID": 2, "count": 1}
	GameState.update_creature(707, {"uid": 707, "x": 5, "y": 5, "type": 1, "action_type": 2})
	var renderer: Control = main.get_node("WorldRenderer")
	renderer._actor_target_rects = {707: {"rect": Rect2(90, 90, 20, 20), "map_y": 5}}
	var left_click := InputEventMouseButton.new()
	left_click.button_index = MOUSE_BUTTON_LEFT
	left_click.position = Vector2(100, 100)
	left_click.pressed = true
	main.call("_handle_mouse_click", left_click)
	if main.get("_chase_target_uid") != 707 or main.get("_attack_focus_uid") != 707 or GameState.grabbed_item.is_empty():
		_fail("exact-frame monster click did not outrank grabbed-item drop")
		return false
	main.call("_cancel_movement")
	var right_click := InputEventMouseButton.new()
	right_click.button_index = MOUSE_BUTTON_RIGHT
	right_click.position = Vector2(100, 100)
	right_click.pressed = true
	main.call("_handle_mouse_click", right_click)
	if main.get("_attack_focus_uid") != 0 or main.get("_follow_focus_uid") != 707 or not main.get("_move_path").is_empty():
		_fail("right-click focused creature incorrectly issued ground movement")
		return false
	main.call("_cancel_movement")
	if main.get("_follow_focus_uid") != 707:
		_fail("generic movement cancellation erased persistent follow focus")
		return false
	if renderer.focus_color(1) != Color8(0xFF, 0x86, 0x00) or renderer.focus_color(2) != Color8(0x92, 0xC6, 0x20) or renderer.focus_color(3) != Color8(0x00, 0xC6, 0xF0) or renderer.focus_color(4) != Color8(0xD0, 0x2C, 0x70):
		_fail("focus channel colors do not match C++")
		return false
	renderer._actor_target_rects.clear()
	GameState.grabbed_item = {"itemID": 1, "seqID": 2, "count": 3}
	left_click.position = Vector2(300, 200)
	main.call("_handle_mouse_click", left_click)
	if GameState.grabbed_item.get("count", 0) != 3:
		_fail("drop request optimistically cleared the grabbed item before server acknowledgement")
		return false
	main.call("_on_server_message", NetworkClient.SM_REMOVEITEM, _remove_item_payload(1, 2, 1))
	if GameState.grabbed_item.get("count", 0) != 2:
		_fail("partial SM_REMOVEITEM did not decrement the grabbed item")
		return false
	main.call("_on_server_message", NetworkClient.SM_REMOVEITEM, _remove_item_payload(1, 2, 2))
	if not GameState.grabbed_item.is_empty():
		_fail("final SM_REMOVEITEM did not clear the grabbed item")
		return false
	GameState.grabbed_item = {}
	GameState.creatures.erase(707)
	return true


func _remove_item_payload(item_id: int, seq_id: int, count: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(10)
	payload.encode_u32(0, item_id)
	payload.encode_u32(4, seq_id)
	payload.encode_u16(8, count)
	return payload


func _test_mining(main: Control, resources: RefCounted) -> bool:
	var mine_weapon := 0
	for item_id_value in resources.item_attributes:
		if resources.item_can_mine(int(item_id_value)):
			mine_weapon = int(item_id_value)
			break
	if mine_weapon == 0:
		_fail("mine-capable weapon metadata unavailable")
		return false
	GameState.wear = {3: {"itemID": mine_weapon}}
	GameState.grabbed_item = {}
	GameState.ground_items.clear()
	var renderer: Control = main.get_node("WorldRenderer")
	renderer._actor_target_rects.clear()
	GameState.view_x = 0.0
	GameState.view_y = 0.0
	var click_position := Vector2(96, 32)
	var mine_grid: Vector2i = renderer.grid_from_screen(int(click_position.x), int(click_position.y))
	var encoded_mine := Protocol.encode_action_node({"type": 14, "speed": 100, "x": mine_grid.x, "y": mine_grid.y})
	var decoded_mine := Protocol.decode_action_node(encoded_mine)
	if int(decoded_mine.get("type", 0)) != 14 or int(decoded_mine.get("x", -1)) != mine_grid.x or int(decoded_mine.get("y", -1)) != mine_grid.y:
		_fail("ACTION_MINE target is not encoded in ActionNode x/y")
		return false
	GameState.player_x = mine_grid.x - 1
	GameState.player_y = mine_grid.y
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = click_position
	click.pressed = true
	main.call("_handle_mouse_click", click)
	if main.get("_mine_target") != mine_grid or GameState.player_action_type != 14 or float(main.get("_player_action_timer")) < 0.89:
		_fail("mine click did not enter repeating ACTION_MINE at the target grid")
		return false
	main.call("_process_player_action", 1.0)
	main.call("_process_movement", 0.01)
	if GameState.player_action_type != 14 or float(main.get("_player_action_timer")) < 0.89:
		_fail("mine action did not repeat after attack-mode timing")
		return false
	main.call("_cancel_movement")
	if main.get("_mine_target") != Vector2i(-1, -1):
		_fail("new operation did not cancel mining")
		return false
	return true


func _test_pickup_action(main: Control, resources: RefCounted) -> bool:
	GameState.player_action_type = 2
	main.call("_begin_pickup_action")
	if GameState.player_action_type != 8 or float(main.get("_pickup_action_timer")) <= 0.0:
		_fail("pickup did not enter delayed action state")
		return false
	var pickup_motion: PackedInt32Array = main.get_node("WorldRenderer").call("_hero_motion", 8)
	if pickup_motion != PackedInt32Array([8, 2]):
		_fail("pickup did not use C++ cut/stand combined timing")
		return false
	var renderer: Control = main.get_node("WorldRenderer")
	if renderer.call("_hero_motion", 7) != PackedInt32Array([9, 6]):
		_fail("physical attack does not use the C++ one-handed vertical swing")
		return false
	var sky_sword_id: int = resources.magic_id("翔空剑法")
	var flame_sword_id: int = resources.magic_id("烈火剑法")
	var lotus_sword_id: int = resources.magic_id("莲月剑法")
	var half_moon_id: int = resources.magic_id("半月弯刀")
	var wheel_id: int = resources.magic_id("十方斩")
	var attack_magic_ids := [
		flame_sword_id, sky_sword_id, lotus_sword_id, half_moon_id, wheel_id,
		resources.magic_id("攻杀剑术"), resources.magic_id("刺杀剑术"),
	]
	if attack_magic_ids.has(0):
		_fail("attack magic name metadata incomplete")
		return false
	for attack_magic_id in attack_magic_ids:
		var attack_meta: PackedInt32Array = resources.magic_layout(attack_magic_id, 2)
		if attack_meta.is_empty() or attack_meta[2] <= 0 or attack_meta[5] != 2:
			_fail("motion-synced attack metadata mismatch: id=%d meta=%s" % [attack_magic_id, attack_meta])
			return false
	if renderer.call("_hero_motion", 7, sky_sword_id, {}) != PackedInt32Array([17, 10]):
		_fail("sky sword attack does not use C++ random swing motion")
		return false
	if renderer.call("_hero_motion", 7, half_moon_id, {}) != PackedInt32Array([11, 6]):
		_fail("half moon attack does not use C++ one-handed horizontal swing")
		return false
	if renderer.call("_hero_motion", 7, wheel_id, {}) != PackedInt32Array([16, 10]):
		_fail("wheel attack does not use C++ whirlwind motion")
		return false
	var double_hand_weapon := 0
	for item_id_value in resources.item_attributes:
		if resources.item_attribute(item_id_value).get("double_hand", false):
			double_hand_weapon = int(item_id_value)
			break
	if double_hand_weapon == 0:
		_fail("item meta v3 did not export any double-handed weapon")
		return false
	var double_hand_desp := {"wear": {3: {"itemID": double_hand_weapon}}}
	if renderer.call("_hero_motion", 7, 0, double_hand_desp) != PackedInt32Array([10, 6]) or renderer.call("_hero_motion", 7, half_moon_id, double_hand_desp) != PackedInt32Array([12, 6]):
		_fail("double-handed weapon did not select C++ vertical/horizontal swing motions")
		return false
	if not is_equal_approx(float(main.call("_action_duration", 7, 100, 2, sky_sword_id)), 1.3) or not is_equal_approx(float(main.call("_action_duration", 7, 100, 2, wheel_id)), 0.9666667):
		_fail("special attack action timing mismatch")
		return false
	var now := Time.get_ticks_msec()
	var flame_effect: Dictionary = renderer.call("_attack_motion_effect_state", flame_sword_id, 3, now - 100)
	if flame_effect.is_empty() or not flame_effect.visible or flame_effect.frame != 1 or flame_effect.direction != (2 if flame_effect.meta[6] > 1 else 0):
		_fail("ordinary sword effect did not sync to the attack motion: %s" % flame_effect)
		return false
	var wheel_lag: Dictionary = renderer.call("_attack_motion_effect_state", wheel_id, 3, now)
	var wheel_visible: Dictionary = renderer.call("_attack_motion_effect_state", wheel_id, 3, now - 200)
	if wheel_lag.is_empty() or wheel_lag.visible or wheel_visible.is_empty() or not wheel_visible.visible or wheel_visible.frame != 0:
		_fail("wheel sword effect did not preserve the C++ three-frame lag: early=%s visible=%s" % [wheel_lag, wheel_visible])
		return false
	if not renderer.call("_attack_motion_effect_state", flame_sword_id, 3, now - 600).is_empty():
		_fail("motion-synced sword effect outlived its six-frame attack")
		return false
	if not is_equal_approx(float(main.call("_action_duration", 7, 250, 2, flame_sword_id)), 0.9):
		_fail("ordinary sword action incorrectly inherited network speed")
		return false
	if renderer.call("_motion_frame", 8, 2, now, 100) != 0 or renderer.call("_motion_frame", 8, 2, now - 1000, 100) != 1:
		_fail("transient motion does not start at frame zero and clamp at the last frame")
		return false
	var move_start: Vector2 = renderer.call("_action_draw_grid", 5, 0, 0, 0, 3, now, 100)
	var move_end: Vector2 = renderer.call("_action_draw_grid", 5, 0, 0, 0, 3, now - 1000, 100)
	if move_start.x > 0.2 or not is_equal_approx(move_end.x, 5.0):
		_fail("move rendering does not interpolate from action origin to destination")
		return false
	main.call("_process_pickup_action", 0.21)
	if GameState.player_action_type != 2 or float(main.get("_pickup_action_timer")) >= 0.0:
		_fail("pickup action did not finish and return to stand")
		return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
