extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const Protocol = preload("res://scripts/network/protocol.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var fireball_id: int = resources.magic_id("火球术")
	var thunder_id: int = resources.magic_id("雷电术")
	var firewall_id: int = resources.magic_id("火墙")
	if fireball_id == 0 or thunder_id == 0 or firewall_id == 0:
		_fail("magic name metadata incomplete")
		return
	var fireball_run: PackedInt32Array = resources.magic_layout(fireball_id, 2)
	var thunder_run: PackedInt32Array = resources.magic_layout(thunder_id, 2)
	var firewall_run: PackedInt32Array = resources.magic_layout(firewall_id, 2)
	if fireball_run.is_empty() or fireball_run[5] != 3 or thunder_run.is_empty() or thunder_run[5] != 2 or firewall_run.is_empty() or not bool(firewall_run[7] & 3):
		_fail("fixed/bound/follow metadata mismatch")
		return
	if resources.frame("magic", fireball_run[0]).is_empty() or resources.frame("magic", thunder_run[0]).is_empty() or resources.frame("magic", firewall_run[0]).is_empty():
		_fail("original magic frame missing")
		return

	var ext := PackedByteArray()
	ext.resize(8)
	ext.encode_u32(0, fireball_id)
	var encoded := Protocol.encode_action_node({"type": 9, "extParam": ext})
	var decoded := Protocol.decode_action_node(encoded)
	if decoded.get("magicID", 0) != fireball_id:
		_fail("ACTION_SPELL magicID decode mismatch")
		return

	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	GameState.player_uid = (5 << 59) | (1 << 35) | 1
	GameState.player_x = 405
	GameState.player_y = 120
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.view_x = 405 * 48 - 400
	GameState.view_y = 120 * 32 - 300
	var target_uid: int = (4 << 59) | (1 << 35) | 2
	GameState.creatures = {target_uid: {"uid": target_uid, "x": 409, "y": 120, "type": 1, "monster_id": 1, "direction": 7}}
	GameState.firewalls = [{"x": 407, "y": 122, "count": 2}]
	$WorldRenderer.game_state = GameState
	if not $WorldRenderer.load_map(24):
		_fail("map 24 failed to load")
		return

	var now := Time.get_ticks_msec()
	var fireball := {
		"source": "action", "magicID": fireball_id, "uid": GameState.player_uid,
		"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
		"direction": 3, "start_time": now,
	}
	var spell_meta: PackedInt32Array = resources.magic_layout(fireball_id, 1)
	if not spell_meta.is_empty():
		fireball.start_time -= $WorldRenderer.call("_magic_stage_duration", spell_meta, fireball)
	fireball.start_time -= 250
	GameState.magic_effects = [
		fireball,
		{
			"source": "cast", "magicID": thunder_id, "uid": GameState.player_uid,
			"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
			"direction": 3, "start_time": now,
		},
	]
	var active: Array = $WorldRenderer.call("_resolve_magic_effects", now)
	if active.size() != 2 or active[0].get("stage", 0) != 2 or active[1].get("stage", 0) != 2:
		_fail("magic stage chain resolution mismatch: %s" % active)
		return
	# Restore effects because the renderer's resolver keeps only still-active source records.
	GameState.magic_effects = [fireball, {
		"source": "cast", "magicID": thunder_id, "uid": GameState.player_uid,
		"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
		"direction": 3, "start_time": now,
	}]
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_MAGIC_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MAGIC_SCREENSHOT"))
	print("MAGIC EFFECT PASS: action spell, cast attach, firewall loop and original frames")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("MAGIC_EFFECT_SMOKE %s" % message)
	get_tree().quit(1)
