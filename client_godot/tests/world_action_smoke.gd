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
	if not _test_magic_actions(main, resources, physical_id):
		return
	if not await _test_action_seff(main, resources):
		return
	if not _test_shield_hit_action(main, resources):
		return
	if not _test_spinkick_direction(main):
		return

	GameState.chat_log.clear()
	var item_id: int = resources.item_names.keys()[0]
	var pickup_error := PackedByteArray()
	pickup_error.resize(4)
	pickup_error.encode_u32(0, item_id)
	main.call("_on_server_message", NetworkClient.SM_PICKUPERROR, pickup_error)
	var equip_error := PackedByteArray()
	equip_error.resize(10)
	equip_error.encode_u32(0, item_id)
	equip_error.encode_u32(4, 1)
	equip_error.encode_u16(8, 3)
	main.call("_on_server_message", NetworkClient.SM_EQUIPWEARERROR, equip_error)
	if GameState.chat_log.size() != 2 or resources.item_name(item_id) not in GameState.chat_log[0].text or "无法放置" not in GameState.chat_log[1].text:
		_fail("world operation error feedback mismatch: %s" % GameState.chat_log)
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
	GameState.runtime_config[6] = PackedByteArray([1, 0, 0])
	main.call("_update_fps_overlay")
	if fps.visible:
		_fail("runtime FPS overlay did not hide")
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
	GameState.update_creature(505, {"uid": 505, "x": 12, "y": 10, "type": 1, "action_type": 2})
	main.get_node("WorldRenderer")._actor_target_rects = {505: {"rect": Rect2(-100, -100, 200, 200), "map_y": 10}}
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
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(101))
	if GameState.player_action_type != 13:
		_fail("player death action was not applied")
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
	main.call("_on_server_message", NetworkClient.SM_ACTION, _sm_action(101, 999, {
		"type": 6, "speed": 100, "direction": 5, "x": 20, "y": 21, "aimX": 30, "aimY": 31,
	}))
	if GameState.player_x != 20 or GameState.player_y != 21:
		_fail("stale-map world action changed current player position")
		return false
	if GameState.magic_effects.is_empty() or GameState.magic_effects.back().get("magicID", 0) != resources.magic_id("瞬息移动"):
		_fail("space move did not create original teleport effect")
		return false
	var remote_uid: int = (5 << 59) | 606
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
	GameState.grabbed_item = {}
	GameState.creatures.erase(707)
	renderer._actor_target_rects.clear()
	return true


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
