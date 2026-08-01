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
	var shield_id: int = resources.magic_id("魔法盾")
	var ring_id: int = resources.magic_id("阴阳法环")
	if fireball_id == 0 or thunder_id == 0 or firewall_id == 0 or shield_id == 0 or ring_id == 0:
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
	GameState.magic_effects = [fireball]
	var active: Array = $WorldRenderer.call("_resolve_magic_effects", now)
	if active.size() != 1 or active[0].get("stage", 0) != 2:
		_fail("magic stage chain resolution mismatch: %s" % active)
		return

	GameState.attached_magic_effects.clear()
	var cast_data := {
		"uid": GameState.player_uid, "mapUID": GameState.player_map_uid,
		"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
	}
	cast_data["magic"] = fireball_id
	if GameState.add_cast_magic_attachment(cast_data, "火球术") or not GameState.attached_magic_effects.is_empty():
		_fail("ordinary cast unexpectedly created an attached effect")
		return
	cast_data["magic"] = shield_id
	if not GameState.add_cast_magic_attachment(cast_data, "魔法盾"):
		_fail("magic shield attachment rejected")
		return
	var shield: Dictionary = GameState.attached_magic_effects.back()
	if shield.get("target_uid", 0) != GameState.player_uid or shield.get("cycles", 0) != 2 or shield.get("kind", "") != "shield":
		_fail("magic shield attachment metadata mismatch: %s" % shield)
		return
	shield.start_time = now
	var attached: Dictionary = $WorldRenderer.call("_resolve_attached_magic", now)
	var player_attached: Array = attached.get(GameState.player_uid, [])
	if player_attached.size() != 1 or not is_equal_approx(player_attached[0].get("alpha_mod", 0.0), 240.0 / 255.0):
		_fail("magic shield alpha/layer resolution mismatch: %s" % attached)
		return
	var shield_meta: PackedInt32Array = resources.magic_layout(shield_id, 2)
	var shield_duration: int = maxi(100, roundi(shield_meta[2] * 1000.0 / (10.0 * shield_meta[4] / 100.0)))
	shield.start_time = now - shield_duration
	attached = $WorldRenderer.call("_resolve_attached_magic", now)
	if attached.get(GameState.player_uid, []).size() != 1:
		_fail("magic shield second cycle missing")
		return
	shield.start_time = now - shield_duration * 2
	attached = $WorldRenderer.call("_resolve_attached_magic", now)
	if attached.has(GameState.player_uid) or not GameState.attached_magic_effects.is_empty():
		_fail("magic shield did not expire after two cycles")
		return

	cast_data["magic"] = ring_id
	GameState.add_cast_magic_attachment(cast_data, "阴阳法环")
	var ring: Dictionary = GameState.attached_magic_effects.back()
	var ring_meta: PackedInt32Array = resources.magic_layout(ring_id, 2)
	var ring_duration: int = maxi(100, roundi(ring_meta[2] * 1000.0 / (10.0 * ring_meta[4] / 100.0)))
	var ring_elapsed := mini(100, ring_duration - 1)
	ring.start_time = now - ring_duration - ring_elapsed
	attached = $WorldRenderer.call("_resolve_attached_magic", now)
	var ring_alpha: float = attached.get(GameState.player_uid, [])[0].get("alpha_mod", 0.0)
	var expected_ring_alpha := maxf(absf(cos(float(ring_elapsed) / 800.0)), 32.0 / 255.0)
	if not is_equal_approx(ring_alpha, expected_ring_alpha):
		_fail("yin-yang ring second-cycle alpha mismatch: %f" % ring_alpha)
		return

	GameState.attached_magic_effects.clear()
	cast_data["magic"] = thunder_id
	GameState.add_cast_magic_attachment(cast_data, "雷电术")
	var thunder: Dictionary = GameState.attached_magic_effects.back()
	thunder.start_time = now
	thunder["play_seff"] = false
	attached = $WorldRenderer.call("_resolve_attached_magic", now)
	var target_attached: Array = attached.get(target_uid, [])
	if target_attached.size() != 1 or not target_attached[0].get("mirror_vertical", false):
		_fail("thunderbolt target/mirror resolution mismatch: %s" % attached)
		return

	# Restore effects because the render pass consumes expired source records.
	GameState.magic_effects = [fireball]
	GameState.attached_magic_effects = [shield, thunder]
	shield.start_time = now
	thunder.start_time = now
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_MAGIC_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MAGIC_SCREENSHOT"))
	print("MAGIC EFFECT PASS: action spell, original cast attachments, firewall loop and original frames")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("MAGIC_EFFECT_SMOKE %s" % message)
	get_tree().quit(1)
