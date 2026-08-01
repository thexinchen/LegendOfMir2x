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
	var hellfire_id: int = resources.magic_id("地狱火")
	var ice_thrust_id: int = resources.magic_id("冰沙掌")
	var fire_ash_id: int = resources.magic_id("魔法特效_火焰灰烬")
	var ice_thorn_id: int = resources.magic_id("魔法特效_冰刺")
	var wind_chain_id: int = resources.magic_id("风震天")
	var laser_id: int = resources.magic_id("疾光电影")
	var healing_id: int = resources.magic_id("治愈术")
	var flame_sword_id: int = resources.magic_id("烈火剑法")
	var target_attachment_ids := [
		resources.magic_id("乾坤大挪移"), healing_id, resources.magic_id("圣言术"), resources.magic_id("云寂术"),
		resources.magic_id("回生术"), resources.magic_id("施毒术"), resources.magic_id("诱惑之光"), resources.magic_id("移花接玉"),
	]
	var hit_wind_id: int = resources.magic_id("击风")
	var fixed_action_ids := [
		hit_wind_id, resources.magic_id("冰咆哮"), resources.magic_id("龙卷风"), resources.magic_id("爆裂火焰"),
		resources.magic_id("地狱雷光"), resources.magic_id("怒神霹雳"), resources.magic_id("群体治愈术"),
	]
	var fixed_projectile_ids := [resources.magic_id("月魂断玉"), resources.magic_id("月魂灵波"), resources.magic_id("冰月震天")]
	var projectile_ids := [
		fireball_id, resources.magic_id("大火球"), resources.magic_id("霹雳掌"), resources.magic_id("风掌"),
		fixed_projectile_ids[0], fixed_projectile_ids[1], resources.magic_id("灵魂火符"), resources.magic_id("冰月神掌"),
		fixed_projectile_ids[2], resources.magic_id("幽灵盾"), resources.magic_id("神圣战甲术"), resources.magic_id("强魔震法"),
		resources.magic_id("猛虎强势"), resources.magic_id("集体隐身术"),
	]
	if fireball_id == 0 or thunder_id == 0 or firewall_id == 0 or shield_id == 0 or ring_id == 0 or hellfire_id == 0 or ice_thrust_id == 0 or fire_ash_id == 0 or ice_thorn_id == 0 or wind_chain_id == 0 or laser_id == 0 or flame_sword_id == 0 or target_attachment_ids.has(0) or fixed_action_ids.has(0) or projectile_ids.has(0):
		_fail("magic name metadata incomplete")
		return
	for magic_id in target_attachment_ids:
		var attachment_meta: PackedInt32Array = resources.magic_layout(magic_id, 2)
		if attachment_meta.is_empty() or attachment_meta[2] <= 0 or attachment_meta[5] != 2:
			_fail("target attachment metadata mismatch: id=%d meta=%s" % [magic_id, attachment_meta])
			return
	for magic_id in fixed_action_ids:
		var fixed_meta: PackedInt32Array = resources.magic_layout(magic_id, 2)
		if fixed_meta.is_empty() or fixed_meta[2] <= 0 or fixed_meta[5] != 1:
			_fail("fixed action metadata mismatch: id=%d meta=%s" % [magic_id, fixed_meta])
			return
	for magic_id in projectile_ids:
		var projectile_meta: PackedInt32Array = resources.magic_layout(magic_id, 2)
		if projectile_meta.size() < 41 or projectile_meta[2] <= 0 or projectile_meta[5] != 3:
			_fail("follow projectile metadata mismatch: id=%d meta=%s" % [magic_id, projectile_meta])
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
	if resources.frame("magic", 0x0F0000DC).is_empty() or resources.frame("magic", 0x0F000104).is_empty() or resources.frame("magic", 0x0F000105).is_empty():
		_fail("special ground textures missing")
		return
	var special_source := _find_open_wave_source($WorldRenderer, 8, $WorldRenderer.map_height - 8)
	if special_source.x < 0:
		_fail("no open eight-grid line for special magic fixture")
		return
	var ice_source := _find_open_wave_source($WorldRenderer, special_source.y + 2, mini($WorldRenderer.map_height - 8, special_source.y + 12))
	if ice_source.x < 0:
		ice_source = special_source + Vector2i(8, 0)
	var variants: Array = []
	for index in 8:
		variants.append({
			"ash_direction": index % 5,
			"frame_offset": index % 10,
			"rotation": index * 31,
			"slag_indices": [index % 2, (index + 1) % 2],
			"ice_rotations": [index * 23, index * 29],
		})
	var hellfire_meta: PackedInt32Array = resources.magic_layout(hellfire_id, 2)
	var ice_thrust_meta: PackedInt32Array = resources.magic_layout(ice_thrust_id, 2)
	var ice_thorn_meta: PackedInt32Array = resources.magic_layout(ice_thorn_id, 2)
	if hellfire_meta.is_empty() or ice_thrust_meta.is_empty() or ice_thorn_meta.is_empty() or not bool(ice_thrust_meta[7] & 4):
		_fail("special composite run metadata missing: hell=%d ice_parent=%s ice_child=%d" % [hellfire_meta.size(), ice_thrust_meta, ice_thorn_meta.size()])
		return
	var special_effect := {
		"source": "action", "magicID": hellfire_id, "x": special_source.x, "y": special_source.y,
		"aimX": special_source.x + 8, "aimY": special_source.y, "direction": 3, "speed": 100,
		"_special_variants": variants, "_special_seff_mask": 0xFF,
	}
	var pending_special: Dictionary = $WorldRenderer.call("_resolve_special_magic", special_effect, hellfire_id, "hellfire", hellfire_meta, 0)
	if pending_special.is_empty() or not pending_special.get("components", []).is_empty():
		_fail("special wave was not retained before its first 100ms delay")
		return
	var hellfire_trigger: int = $WorldRenderer.call("_magic_frame_reach_duration", hellfire_meta, 10)
	var hellfire_state: Dictionary = $WorldRenderer.call("_resolve_special_magic", special_effect, hellfire_id, "hellfire", hellfire_meta, 100 + hellfire_trigger + 500)
	if hellfire_state.get("underlays", []).is_empty() or hellfire_state.get("components", []).is_empty():
		_fail("hellfire ash composite missing: %s" % hellfire_state)
		return
	var first_ash: Dictionary = hellfire_state.get("underlays", [])[0]
	if first_ash.get("texture_id", 0) != 0x0F0000DC or first_ash.get("crop", Vector2i.ZERO) != Vector2i(102, 72):
		_fail("hellfire ash texture/crop mismatch: %s" % first_ash)
		return
	var ice_effect := special_effect.duplicate(true)
	ice_effect["magicID"] = ice_thrust_id
	var ice_state: Dictionary = $WorldRenderer.call("_resolve_special_magic", ice_effect, ice_thrust_id, "ice_thrust", ice_thrust_meta, 1100)
	if ice_state.get("components", []).size() < 2 or ice_state.get("underlays", []).size() < 2:
		_fail("ice thrust child/slag composite missing: %s" % ice_state)
		return
	var ice_underlay: Dictionary = ice_state.get("underlays", [])[0]
	if int(ice_underlay.get("texture_id", 0)) not in [0x0F000104, 0x0F000105]:
		_fail("ice slag texture mismatch: %s" % ice_underlay)
		return
	if $WorldRenderer.call("_direction_step", 3) != Vector2i(1, 0):
		_fail("special wave direction mapping mismatch")
		return
	var ice_lifetime: int = $WorldRenderer.call("_magic_frame_reach_duration", ice_thorn_meta, 55)
	if not $WorldRenderer.call("_resolve_special_magic", ice_effect, ice_thrust_id, "ice_thrust", ice_thrust_meta, 800 + ice_lifetime).is_empty():
		_fail("ice thrust composite did not expire after the eighth slag lifetime")
		return
	if resources.magic_seff(ice_thrust_id, 2) < 0:
		_fail("invisible ice thrust parent lost its run SEFF metadata")
		return
	var wind_run: PackedInt32Array = resources.magic_layout(wind_chain_id, 2)
	var wind_explode: PackedInt32Array = resources.magic_layout(wind_chain_id, 3)
	var laser_run: PackedInt32Array = resources.magic_layout(laser_id, 2)
	if wind_run.is_empty() or wind_explode.is_empty() or laser_run.is_empty():
		_fail("propagated spell metadata missing")
		return
	var propagated_effect := {
		"source": "action", "x": special_source.x, "y": special_source.y,
		"aimX": special_source.x + 8, "aimY": special_source.y, "direction": 3, "speed": 100,
		"_propagated_seff_mask": 0xFF,
	}
	var wind_effect := propagated_effect.duplicate(true)
	wind_effect["magicID"] = wind_chain_id
	var first_wind: Dictionary = $WorldRenderer.call("_resolve_wind_chain", wind_effect, wind_chain_id, wind_run, 0)
	var first_wind_components: Array = first_wind.get("components", [])
	if first_wind_components.size() != 1 or first_wind_components[0].position != Vector2(special_source + Vector2i(1, 0)):
		_fail("wind chain did not begin one grid forward: %s" % first_wind)
		return
	var wind_run_duration: int = $WorldRenderer.call("_magic_frame_duration", wind_run)
	var wind_explode_state: Dictionary = $WorldRenderer.call("_resolve_wind_chain", wind_effect, wind_chain_id, wind_run, 7 * wind_run_duration)
	var wind_explode_components: Array = wind_explode_state.get("components", [])
	if wind_explode_components.size() != 1 or wind_explode_components[0].meta != wind_explode or wind_explode_components[0].position != Vector2(special_source + Vector2i(8, 0)):
		_fail("wind chain eighth-grid explode mismatch: %s" % wind_explode_state)
		return
	var laser_effect := propagated_effect.duplicate(true)
	laser_effect["magicID"] = laser_id
	var laser_state: Dictionary = $WorldRenderer.call("_resolve_caster_laser", laser_effect, laser_id, laser_run, 100)
	var laser_components: Array = laser_state.get("components", [])
	if laser_components.size() != 1 or laser_components[0].position != Vector2(special_source) or laser_components[0].direction != 2:
		_fail("laser did not stay on caster grid/direction: %s" % laser_state)
		return
	var fixed_effect := propagated_effect.duplicate(true)
	fixed_effect["aimUID"] = target_uid
	fixed_effect["aimX"] = 409
	fixed_effect["aimY"] = 120
	fixed_effect["_seff_stage_mask"] = 0xFFFF
	for fixed_id in fixed_action_ids:
		var action_fixed := fixed_effect.duplicate(true)
		action_fixed["magicID"] = fixed_id
		var fixed_run: PackedInt32Array = resources.magic_layout(fixed_id, 2)
		var before_fixed: Dictionary = $WorldRenderer.call("_resolve_fixed_action_magic", action_fixed, fixed_id, "run_explode" if fixed_id == hit_wind_id else "run", 299)
		for component in before_fixed.get("components", []):
			if component.meta == fixed_run:
				_fail("fixed action triggered before spell frame 3: id=%d" % fixed_id)
				return
		var running_fixed: Dictionary = $WorldRenderer.call("_resolve_fixed_action_magic", action_fixed, fixed_id, "run_explode" if fixed_id == hit_wind_id else "run", 300)
		var run_components: Array = running_fixed.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fixed_run)
		if run_components.size() != 1 or run_components[0].position != Vector2(409, 120):
			_fail("fixed action frame-3 placement mismatch: id=%d state=%s" % [fixed_id, running_fixed])
			return
		GameState.creatures[target_uid]["x"] = 410
		var moved_fixed: Dictionary = $WorldRenderer.call("_resolve_fixed_action_magic", action_fixed, fixed_id, "run_explode" if fixed_id == hit_wind_id else "run", 400)
		for component in moved_fixed.get("components", []):
			if component.meta == fixed_run and component.position != Vector2(409, 120):
				_fail("fixed action followed target after trigger: id=%d state=%s" % [fixed_id, moved_fixed])
				return
		GameState.creatures[target_uid]["x"] = 409
	var hit_run: PackedInt32Array = resources.magic_layout(hit_wind_id, 2)
	var hit_explode: PackedInt32Array = resources.magic_layout(hit_wind_id, 3)
	var hit_effect := fixed_effect.duplicate(true)
	hit_effect["magicID"] = hit_wind_id
	$WorldRenderer.call("_resolve_fixed_action_magic", hit_effect, hit_wind_id, "run_explode", 300)
	var hit_explode_state: Dictionary = $WorldRenderer.call("_resolve_fixed_action_magic", hit_effect, hit_wind_id, "run_explode", 300 + int($WorldRenderer.call("_magic_frame_duration", hit_run)))
	var hit_components: Array = hit_explode_state.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == hit_explode)
	if hit_components.size() != 1 or hit_components[0].position != Vector2(409, 120):
		_fail("hit-wind explode chain mismatch: %s" % hit_explode_state)
		return
	if not is_equal_approx(float($WorldRenderer.call("_fire_ash_alpha", 500)), 0.5) or not is_equal_approx(float($WorldRenderer.call("_ice_slag_alpha", 5)), 0.5):
		_fail("special ground alpha envelope mismatch")
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
	var active_fireball_components: Array = active[0].get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fireball_run) if active.size() == 1 else []
	if active.size() != 1 or active_fireball_components.size() != 1:
		_fail("magic stage chain resolution mismatch: %s" % active)
		return

	GameState.attached_magic_effects.clear()
	var attachment_action := {
		"source": "action", "uid": GameState.player_uid,
		"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
		"direction": 3, "speed": 100, "start_time": now, "_seff_stage_mask": 0xFFFF,
	}
	for attachment_id in target_attachment_ids:
		var action_effect := attachment_action.duplicate(true)
		action_effect["magicID"] = attachment_id
		$WorldRenderer.call("_resolve_magic_effect", action_effect, now + 299)
		if not GameState.attached_magic_effects.is_empty():
			_fail("target attachment triggered before spell frame 3: id=%d" % attachment_id)
			return
		$WorldRenderer.call("_resolve_magic_effect", action_effect, now + 300)
		if GameState.attached_magic_effects.size() != 1 or GameState.attached_magic_effects[0].get("target_uid", 0) != target_uid:
			_fail("target attachment did not bind aimed creature: id=%d effects=%s" % [attachment_id, GameState.attached_magic_effects])
			return
		$WorldRenderer.call("_resolve_magic_effect", action_effect, now + 400)
		if GameState.attached_magic_effects.size() != 1:
			_fail("target attachment duplicated during redraw: id=%d" % attachment_id)
			return
		GameState.attached_magic_effects.clear()
	var moving_effect := attachment_action.duplicate(true)
	moving_effect["magicID"] = healing_id
	$WorldRenderer.call("_resolve_magic_effect", moving_effect, now + 300)
	GameState.creatures[target_uid]["x"] = 411
	GameState.creatures[target_uid]["y"] = 121
	if $WorldRenderer.call("_attached_target_grid", target_uid) != Vector2(411, 121):
		_fail("target attachment did not follow creature movement")
		return
	GameState.creatures.erase(target_uid)
	if not $WorldRenderer.call("_resolve_attached_magic", now + 301).is_empty() or not GameState.attached_magic_effects.is_empty():
		_fail("target attachment survived missing creature")
		return
	var healing_fallback := attachment_action.duplicate(true)
	healing_fallback["magicID"] = healing_id
	healing_fallback["aimUID"] = target_uid
	$WorldRenderer.call("_resolve_magic_effect", healing_fallback, now + 300)
	if GameState.attached_magic_effects.size() != 1 or GameState.attached_magic_effects[0].get("target_uid", 0) != GameState.player_uid:
		_fail("healing attachment did not fall back to caster: %s" % GameState.attached_magic_effects)
		return
	GameState.attached_magic_effects.clear()
	var strict_missing := attachment_action.duplicate(true)
	strict_missing["magicID"] = target_attachment_ids[0]
	strict_missing["aimUID"] = target_uid
	$WorldRenderer.call("_resolve_magic_effect", strict_missing, now + 300)
	if not GameState.attached_magic_effects.is_empty():
		_fail("strict target attachment incorrectly fell back to caster")
		return
	GameState.creatures[target_uid] = {"uid": target_uid, "x": 409, "y": 120, "type": 1, "monster_id": 1, "direction": 7}

	GameState.attached_magic_effects.clear()
	var projectile_effect := {
		"source": "action", "magicID": fireball_id, "uid": GameState.player_uid,
		"x": 405, "y": 120, "aimX": 409, "aimY": 120, "aimUID": target_uid,
		"direction": 3, "speed": 100, "start_time": now, "_seff_stage_mask": 0xFFFF,
	}
	$WorldRenderer.set("_actor_target_rects", {target_uid: {"world_center": Vector2(409 * 48, 120 * 32)}})
	var before_projectile: Dictionary = $WorldRenderer.call("_resolve_magic_effect", projectile_effect, now + 399)
	for component in before_projectile.get("components", []):
		if component.meta == fireball_run:
			_fail("follow projectile launched before spell frame 4")
			return
	var launched_projectile: Dictionary = $WorldRenderer.call("_resolve_magic_effect", projectile_effect, now + 400)
	var launched_components: Array = launched_projectile.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fireball_run)
	var first_position: Vector2 = projectile_effect.get("_projectile_position", Vector2.ZERO)
	var source_position := Vector2(405 * 48, 120 * 32)
	if launched_components.size() != 1 or first_position == source_position or absf(first_position.distance_to(source_position) - 20.0) > 1.0:
		_fail("follow projectile did not advance 20px on launch: %s pos=%s" % [launched_projectile, first_position])
		return
	$WorldRenderer.set("_actor_target_rects", {target_uid: {"world_center": Vector2(first_position.x, first_position.y - 160)}})
	var prior_position := first_position
	$WorldRenderer.call("_resolve_magic_effect", projectile_effect, now + 416)
	var turned_position: Vector2 = projectile_effect.get("_projectile_position", Vector2.ZERO)
	if turned_position.y >= prior_position.y:
		_fail("follow projectile did not home toward moved target: before=%s after=%s" % [prior_position, turned_position])
		return
	var fireball_offset := Vector2(resources.magic_target_offset(fireball_id, 2, projectile_effect.get("_projectile_gfx_direction", 0)))
	$WorldRenderer.set("_actor_target_rects", {target_uid: {"world_center": turned_position + fireball_offset + Vector2(1, 1)}})
	var hit_state: Dictionary = $WorldRenderer.call("_resolve_magic_effect", projectile_effect, now + 432)
	var hit_run_components: Array = hit_state.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fireball_run)
	if not hit_run_components.is_empty() or GameState.attached_magic_effects.size() != 1:
		_fail("follow projectile hit did not replace flight with one impact: state=%s effects=%s" % [hit_state, GameState.attached_magic_effects])
		return
	var impact: Dictionary = GameState.attached_magic_effects[0]
	if impact.get("target_uid", 0) != target_uid or impact.get("stage", 0) != 3 or impact.get("kind", "") != "projectile_impact":
		_fail("follow projectile impact metadata mismatch: %s" % impact)
		return
	var impact_active: Dictionary = $WorldRenderer.call("_resolve_attached_magic", now + 432)
	if impact_active.get(target_uid, []).is_empty() or impact_active[target_uid][0].meta != resources.magic_layout(fireball_id, 3):
		_fail("follow projectile impact did not use explode-stage graphics: %s" % impact_active)
		return

	for fixed_projectile_id in fixed_projectile_ids:
		var fixed_projectile := projectile_effect.duplicate(true)
		fixed_projectile["magicID"] = fixed_projectile_id
		fixed_projectile.erase("_projectile_position")
		fixed_projectile.erase("_projectile_start")
		fixed_projectile.erase("_projectile_fly_direction")
		fixed_projectile.erase("_projectile_gfx_direction")
		fixed_projectile.erase("_projectile_last_fly_offset")
		fixed_projectile.erase("_projectile_done")
		fixed_projectile.erase("_projectile_impact_spawned")
		$WorldRenderer.set("_actor_target_rects", {target_uid: {"world_center": Vector2(409 * 48, 120 * 32)}})
		var fixed_projectile_state: Dictionary = $WorldRenderer.call("_resolve_magic_effect", fixed_projectile, now + 400)
		var fixed_run_meta: PackedInt32Array = resources.magic_layout(fixed_projectile_id, 2)
		var fixed_components: Array = fixed_projectile_state.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fixed_run_meta)
		if fixed_components.size() != 1 or fixed_components[0].direction != 0:
			_fail("fixed-gfx projectile direction mismatch: id=%d state=%s" % [fixed_projectile_id, fixed_projectile_state])
			return

	GameState.attached_magic_effects.clear()
	var missing_projectile := projectile_effect.duplicate(true)
	for key in ["_projectile_position", "_projectile_start", "_projectile_fly_direction", "_projectile_gfx_direction", "_projectile_last_fly_offset", "_projectile_impact_spawned", "_projectile_done"]:
		missing_projectile.erase(key)
	$WorldRenderer.set("_actor_target_rects", {target_uid: {"world_center": Vector2(409 * 48, 120 * 32)}})
	$WorldRenderer.call("_resolve_magic_effect", missing_projectile, now + 400)
	var last_offset: Vector2 = missing_projectile.get("_projectile_last_fly_offset", Vector2.ZERO)
	GameState.creatures.erase(target_uid)
	$WorldRenderer.set("_actor_target_rects", {})
	var missing_before: Vector2 = missing_projectile.get("_projectile_position", Vector2.ZERO)
	$WorldRenderer.call("_resolve_magic_effect", missing_projectile, now + 416)
	if missing_projectile.get("_projectile_position", Vector2.ZERO) != missing_before + last_offset:
		_fail("missing-target projectile did not continue its last direction")
		return
	missing_projectile["_projectile_position"] = Vector2(missing_projectile.get("_projectile_start", Vector2.ZERO)) + Vector2(256 * 48, 0)
	var expired_projectile: Dictionary = $WorldRenderer.call("_resolve_magic_effect", missing_projectile, now + 432)
	var expired_run_components: Array = expired_projectile.get("components", []).filter(func(component: Dictionary) -> bool: return component.meta == fireball_run)
	if not expired_run_components.is_empty() or not GameState.attached_magic_effects.is_empty():
		_fail("missing-target projectile did not expire silently after 255 grids")
		return
	GameState.creatures[target_uid] = {"uid": target_uid, "x": 409, "y": 120, "type": 1, "monster_id": 1, "direction": 7}

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
	var visual_now := Time.get_ticks_msec()
	var hellfire_visual := special_effect.duplicate(true)
	hellfire_visual["start_time"] = visual_now - 300 - 100 - hellfire_trigger - 700
	var ice_visual := ice_effect.duplicate(true)
	ice_visual["x"] = ice_source.x
	ice_visual["y"] = ice_source.y
	ice_visual["aimX"] = ice_source.x + 8
	ice_visual["aimY"] = ice_source.y
	ice_visual["direction"] = 7 if ice_source.y == special_source.y else 3
	ice_visual["start_time"] = visual_now - 300 - 100 - 500
	var visual_ice_state: Dictionary = $WorldRenderer.call("_resolve_magic_effect", ice_visual, visual_now)
	if visual_ice_state.get("components", []).is_empty() or visual_ice_state.get("underlays", []).is_empty():
		_fail("ice visual phase resolved empty: state=%s kind=%s source=%s id=%d run=%d elapsed=%d" % [visual_ice_state, $WorldRenderer.call("_special_magic_kind", int(ice_visual.get("magicID", 0))), ice_visual.get("source", ""), ice_visual.get("magicID", 0), resources.magic_layout(int(ice_visual.get("magicID", 0)), 2).size(), visual_now - int(ice_visual.get("start_time", visual_now))])
		return
	var on_ground_component_count := 0
	for component_value in visual_ice_state.get("components", []):
		var component: Dictionary = component_value
		if component.get("on_ground", false):
			on_ground_component_count += 1
		var component_meta: PackedInt32Array = component.meta
		var texture_id: int = component_meta[0] + int(component.direction) * component_meta[3] + int(component.frame)
		if resources.frame("magic", texture_id).is_empty():
			_fail("ice visual component texture missing: %08X" % texture_id)
			return
	if on_ground_component_count < 2:
		_fail("ice thorn components lost original on-ground row depth")
		return
	GameState.player_x = special_source.x
	GameState.player_y = special_source.y
	GameState.view_x = special_source.x * 48 - 240
	GameState.view_y = roundi(float(special_source.y + ice_source.y) * 16.0) - 260
	GameState.magic_effects = [hellfire_visual, ice_visual, fireball]
	GameState.attached_magic_effects = [shield, thunder]
	shield.start_time = now
	thunder.start_time = now
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_MAGIC_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_MAGIC_SCREENSHOT"))
	if OS.has_environment("MIR2X_ICE_SCREENSHOT"):
		GameState.magic_effects = [ice_visual]
		GameState.view_x = ice_source.x * 48 - 240
		GameState.view_y = ice_source.y * 32 - 260
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_ICE_SCREENSHOT"))
	if OS.has_environment("MIR2X_PROPAGATED_SCREENSHOT"):
		var propagated_now := Time.get_ticks_msec()
		var wind_visual := wind_effect.duplicate(true)
		wind_visual["start_time"] = propagated_now - 400 - 3 * wind_run_duration - 200
		var laser_visual := laser_effect.duplicate(true)
		laser_visual["x"] = ice_source.x
		laser_visual["y"] = ice_source.y
		laser_visual["start_time"] = propagated_now - 300 - 200
		GameState.magic_effects = [wind_visual, laser_visual]
		GameState.view_x = special_source.x * 48 - 240
		GameState.view_y = roundi(float(special_source.y + ice_source.y) * 16.0) - 260
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PROPAGATED_SCREENSHOT"))
	if OS.has_environment("MIR2X_LASER_SCREENSHOT"):
		var laser_now := Time.get_ticks_msec()
		var laser_capture := laser_effect.duplicate(true)
		laser_capture["x"] = ice_source.x
		laser_capture["y"] = ice_source.y
		laser_capture["start_time"] = laser_now - 300 - 200
		GameState.magic_effects = [laser_capture]
		GameState.view_x = ice_source.x * 48 - 240
		GameState.view_y = ice_source.y * 32 - 260
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_LASER_SCREENSHOT"))
	if OS.has_environment("MIR2X_ATTACHMENT_SCREENSHOT"):
		var attachment_now := Time.get_ticks_msec()
		GameState.magic_effects.clear()
		GameState.firewalls.clear()
		GameState.player_action_type = 2
		GameState.player_action_started_ms = attachment_now
		GameState.creatures[target_uid] = {"uid": target_uid, "x": 409, "y": 120, "type": 1, "monster_id": 1, "direction": 7}
		GameState.attached_magic_effects = [
			{"magicID": healing_id, "target_uid": target_uid, "start_time": attachment_now - 300, "cycles": 1, "kind": "action_attachment"},
			{"magicID": healing_id, "target_uid": GameState.player_uid, "start_time": attachment_now - 300, "cycles": 1, "kind": "action_attachment"},
		]
		GameState.view_x = 409 * 48 - 400
		GameState.view_y = 120 * 32 - 300
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_ATTACHMENT_SCREENSHOT"))
	if OS.has_environment("MIR2X_FIXED_ACTION_SCREENSHOT"):
		var fixed_now := Time.get_ticks_msec()
		var ice_roar_visual := fixed_effect.duplicate(true)
		ice_roar_visual["magicID"] = fixed_action_ids[1]
		ice_roar_visual["aimUID"] = 0
		ice_roar_visual["aimX"] = special_source.x
		ice_roar_visual["aimY"] = special_source.y
		ice_roar_visual["start_time"] = fixed_now - 600
		var hit_visual := fixed_effect.duplicate(true)
		hit_visual["magicID"] = hit_wind_id
		hit_visual["aimUID"] = 0
		hit_visual["aimX"] = special_source.x + 5
		hit_visual["aimY"] = special_source.y
		hit_visual["start_time"] = fixed_now - 300 - int($WorldRenderer.call("_magic_frame_duration", hit_run)) - 200
		GameState.attached_magic_effects.clear()
		GameState.firewalls.clear()
		GameState.magic_effects = [ice_roar_visual, hit_visual]
		GameState.view_x = (special_source.x + 2) * 48 - 400
		GameState.view_y = special_source.y * 32 - 300
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FIXED_ACTION_SCREENSHOT"))
	if OS.has_environment("MIR2X_PROJECTILE_SCREENSHOT"):
		var projectile_now := Time.get_ticks_msec()
		GameState.creatures[target_uid] = {"uid": target_uid, "x": 413, "y": 120, "type": 1, "monster_id": 1, "direction": 7}
		GameState.attached_magic_effects.clear()
		GameState.firewalls.clear()
		GameState.magic_effects = [{
			"source": "action", "magicID": fireball_id, "uid": GameState.player_uid,
			"x": 405, "y": 120, "aimX": 413, "aimY": 120, "aimUID": target_uid,
			"direction": 3, "speed": 100, "start_time": projectile_now - 400,
		}]
		GameState.view_x = 409 * 48 - 400
		GameState.view_y = 120 * 32 - 300
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PROJECTILE_SCREENSHOT"))
	if OS.has_environment("MIR2X_ATTACK_MAGIC_SCREENSHOT"):
		var attack_now := Time.get_ticks_msec()
		GameState.player_x = 405
		GameState.player_y = 120
		GameState.player_action_type = 7
		GameState.player_action_magic_id = flame_sword_id
		GameState.player_action_speed = 100
		GameState.player_action_started_ms = attack_now - 200
		GameState.player_direction = 3
		GameState.magic_effects.clear()
		GameState.attached_magic_effects.clear()
		GameState.firewalls.clear()
		GameState.strike_grids.clear()
		GameState.view_x = GameState.player_x * 48 - 400
		GameState.view_y = GameState.player_y * 32 - 300
		$WorldRenderer.queue_redraw()
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_ATTACK_MAGIC_SCREENSHOT"))
	print("MAGIC EFFECT PASS: target/server attachments, follow/fixed/composite/propagated magic and caster-grid laser")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("MAGIC_EFFECT_SMOKE %s" % message)
	get_tree().quit(1)


func _find_open_wave_source(renderer, start_y: int, end_y: int) -> Vector2i:
	for y in range(start_y, end_y):
		for x in range(8, renderer.map_width - 16):
			var open := true
			for distance in range(0, 9):
				if not renderer.can_walk(x + distance, y):
					open = false
					break
			if open:
				return Vector2i(x, y)
	return Vector2i(-1, -1)
