extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldPathfinderScript = preload("res://scripts/game/world_pathfinder.gd")


func _ready() -> void:
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
	main.call("_on_server_message", NetworkClient.SM_NEXTSTRIKE, PackedByteArray())
	if main.call("_consume_attack_magic_id") != next_strike_id or main.call("_consume_attack_magic_id") != physical_id:
		_fail("SM_NEXTSTRIKE was not consumed exactly once")
		return
	if not _test_magic_actions(main, resources, physical_id):
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
	if not _test_death_and_map_filter(main):
		return
	if not _test_path_decomposition():
		return
	if not _test_chase_retry(main):
		return
	if not _test_pickup_action(main, resources):
		return
	print("WORLD ACTION PASS: attack/chase, magic keys, pickup, one-hop pathing, operation feedback, death and map filtering")
	get_tree().quit()


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
	return true


func _test_death_and_map_filter(main: Control) -> bool:
	GameState.player_uid = 101
	GameState.player_map_uid = 202
	GameState.player_action_type = 2
	GameState.update_creature(303, {"uid": 303, "action_type": 2})
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(303))
	if GameState.get_creature(303).get("action_type", 0) != 13:
		_fail("creature death action was not applied")
		return false
	main.call("_on_server_message", NetworkClient.SM_NOTIFYDEAD, _u64_payload(101))
	if GameState.player_action_type != 13:
		_fail("player death action was not applied")
		return false
	main.call("_on_server_message", NetworkClient.SM_OFFLINE, _map_message(303, 999, 16))
	if GameState.get_creature(303).is_empty():
		_fail("stale-map offline message removed current creature")
		return false
	main.call("_on_server_message", NetworkClient.SM_DEADFADEOUT, _map_message(303, 202, 24))
	if not GameState.get_creature(303).is_empty():
		_fail("current-map dead fade did not remove creature")
		return false
	return true


func _u64_payload(value: int) -> PackedByteArray:
	var payload := PackedByteArray()
	payload.resize(8)
	payload.encode_u64(0, value)
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
	var half_moon_id: int = resources.magic_id("半月弯刀")
	var wheel_id: int = resources.magic_id("十方斩")
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
