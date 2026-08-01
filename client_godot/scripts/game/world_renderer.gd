extends Control

# World renderer for the game scene
# Corresponds to ProcessRun::draw() in the C++ client
# Draws: map tiles, ground objects, creatures, ground items, magic effects

const GRID_XP := 48
const GRID_YP := 32
const SCREEN_W := 800
const SCREEN_H := 600
const OBJMAXW := 3
const OBJMAXH := 25
const WorldResourceScript = preload("res://scripts/game/world_resource.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const ANIMATION_DELAYS := [150, 200, 250, 300, 350, 400, 420, 450]
const MAGIC_STAGE_SPELL := 1
const MAGIC_STAGE_RUN := 2
const MAGIC_STAGE_EXPLODE := 3
const MAGIC_STAGE_HITTED := 5
const MAGIC_TYPE_FIXED := 1
const MAGIC_TYPE_BOUND := 2
const MAGIC_TYPE_FOLLOW := 3
const GROUND_ITEM_STAR_GFX_ID := 0x00000090
const GROUND_ITEM_STAR_CYCLE := 2.50
const GROUND_ITEM_STAR_STEP := 0.05
const DEAD_ACTION := 13
const DEAD_FRAME_COUNT := 10
const DEAD_FADE_STEP := 10
const FIRE_ASH_TEXTURE_ID := 0x0F0000DC
const ICE_SLAG_TEXTURE_IDS := [0x0F000105, 0x0F000104]
const SPECIAL_WAVE_COUNT := 8
const SPECIAL_WAVE_DELAY_MS := 100
const FIRE_ASH_FADE_IN_MS := 1000
const FIRE_ASH_HOLD_MS := 5000
const FIRE_ASH_FADE_OUT_MS := 3000
const ICE_SLAG_FADE_IN_FRAMES := 10
const ICE_SLAG_HOLD_FRAMES := 30
const ICE_SLAG_FADE_OUT_FRAMES := 15
const PLAYER_SAY_WIDTH := 160
const PLAYER_SAY_FONT_SIZE := 15
const PLAYER_SAY_SHOW_TIME := 5000
const PLAYER_SAY_MARGIN := 2
const FOCUS_COLORS := [
	Color.WHITE,
	Color8(0xFF, 0x86, 0x00),
	Color8(0x92, 0xC6, 0x20),
	Color8(0x00, 0xC6, 0xF0),
	Color8(0xD0, 0x2C, 0x70),
]

var game_state: Node = null

# Map data
var map_width: int = 0
var map_height: int = 0
var world_resource: RefCounted = WorldResourceScript.new()
var actor_resource: RefCounted = ActorResourceScript.new()
var _active_attached_magic: Dictionary = {}
var _actor_target_rects: Dictionary = {}
var _mouse_focus_uid := 0
var _magic_focus_uid := 0
var _follow_focus_uid := 0
var _attack_focus_uid := 0
var _ground_item_star_ratio := 0.0


func _process(_delta: float) -> void:
	_ground_item_star_ratio = fmod(_ground_item_star_ratio + GROUND_ITEM_STAR_STEP, GROUND_ITEM_STAR_CYCLE)
	_update_dead_fades(Time.get_ticks_msec())
	queue_redraw()


func load_map(map_id: int) -> bool:
	if world_resource.load_map(map_id):
		map_width = world_resource.width
		map_height = world_resource.height
		actor_resource.configure(world_resource.base_path)
		queue_redraw()
		return true
	map_width = 0
	map_height = 0
	push_error(world_resource.last_error)
	return false


func can_walk(x: int, y: int) -> bool:
	return world_resource.can_walk(x, y)


func set_focus_channels(magic_uid: int, follow_uid: int, attack_uid: int) -> void:
	_magic_focus_uid = magic_uid
	_follow_focus_uid = follow_uid
	_attack_focus_uid = attack_uid


func _draw() -> void:
	if game_state == null:
		return
	_mouse_focus_uid = focus_uid_at_screen(get_local_mouse_position())
	
	var view_x: int = int(game_state.view_x)
	var view_y: int = int(game_state.view_y)
	
	# Compute visible grid range (same as C++ draw())
	var x0 := maxi(0, floori(float(view_x) / GRID_XP) - OBJMAXW)
	var y0 := maxi(0, floori(float(view_y) / GRID_YP) - OBJMAXH)
	var x1 := floori(float(view_x + SCREEN_W) / GRID_XP) + OBJMAXW
	var y1 := floori(float(view_y + SCREEN_H) / GRID_YP) + OBJMAXH
	if map_width > 0:
		x1 = mini(x1, map_width - 1)
	if map_height > 0:
		y1 = mini(y1, map_height - 1)
	
	# Tiles and ground objects are below every actor.
	for gy in range(y0, y1 + 1):
		for gx in range(x0, x1 + 1):
			if gy % 2 != 0 or gx % 2 != 0:
				continue
			var texture_id: int = world_resource.tiles.get(gx + gy * map_width, -1)
			if texture_id >= 0:
				var texture: Texture2D = world_resource.texture(texture_id)
				if texture:
					draw_texture(texture, Vector2(gx * GRID_XP - view_x, gy * GRID_YP - view_y))
	_draw_object_depth(0, x0, y0, x1, y1, view_x, view_y)

	var now := Time.get_ticks_msec()
	var active_magic := _resolve_magic_effects(now)
	_active_attached_magic = _resolve_attached_magic(now)
	_actor_target_rects.clear()
	_draw_special_ground_underlays(active_magic, view_x, view_y)
	_draw_dead_actors(view_x, view_y, now)

	# Ground items precede living actors in the original renderer.
	_draw_ground_items(x0, y0, x1, y1, view_x, view_y)

	# Overground objects and actors are interleaved one map row at a time.
	var creatures_by_row: Dictionary = {}
	for uid in game_state.creatures:
		var creature: Dictionary = game_state.creatures[uid]
		if _is_dead_actor(creature):
			continue
		var row: int = creature.get("y", 0)
		var row_creatures: Array = creatures_by_row.get(row, [])
		row_creatures.append(creature)
		creatures_by_row[row] = row_creatures
	for gy in range(y0, y1 + 1):
		_draw_object_row(1, gy, x0, x1, view_x, view_y)
		_draw_firewall_row(gy, x0, x1, view_x, view_y, now)
		_draw_magic_row(active_magic, gy, true, view_x, view_y)
		_draw_strike_row(gy, x0, x1, view_x, view_y, now)
		for creature in creatures_by_row.get(gy, []):
			_draw_creature(creature, view_x, view_y)
		if game_state.player_y == gy and game_state.player_action_type != DEAD_ACTION:
			_draw_player(view_x, view_y)
		_draw_object_row(2, gy, x0, x1, view_x, view_y)
	_draw_object_depth(3, x0, y0, x1, y1, view_x, view_y)
	_draw_ground_item_stars(x0, y0, x1, y1, view_x, view_y)
	_draw_magic_row(active_magic, -1, false, view_x, view_y)

	# Floating combat text is a screen overlay.
	game_state.update_ascend_strings()
	var font := get_theme_default_font()
	if font:
		for s in game_state.ascend_strings:
			var age: int = Time.get_ticks_msec() - s.get("start_time", 0)
			var progress: float = float(age) / 1500.0
			var sx: int = int(s.get("x", 0)) - view_x
			var sy: int = int(s.get("y", 0)) - view_y - int(progress * 30)
			var alpha: float = 1.0 - progress
			var text: String = s.get("text", "")
			var color: Color = s.get("color", Color(1, 1, 1, 1))
			color.a = alpha
			var tw := font.get_string_size(text, HORIZONTAL_ALIGNMENT_CENTER, -1, 13)
			font.draw_string(get_canvas_item(), Vector2(sx - tw.x * 0.5, sy), text, HORIZONTAL_ALIGNMENT_CENTER, -1, 13, color)


func _draw_object_depth(depth: int, x0: int, y0: int, x1: int, y1: int, view_x: int, view_y: int) -> void:
	for y in range(y0, y1 + 1):
		_draw_object_row(depth, y, x0, x1, view_x, view_y)


func _draw_ground_items(x0: int, y0: int, x1: int, y1: int, view_x: int, view_y: int) -> void:
	var mouse_grid := grid_from_screen(roundi(get_local_mouse_position().x), roundi(get_local_mouse_position().y))
	var font := get_theme_default_font()
	for grid_key in game_state.ground_items:
		var parts: PackedStringArray = grid_key.split(",")
		if parts.size() != 2:
			continue
		var gx := int(parts[0])
		var gy := int(parts[1])
		if gx < x0 or gx > x1 or gy < y0 or gy > y1:
			continue
		var mouse_over := mouse_grid == Vector2i(gx, gy)
		for item_value in game_state.ground_items[grid_key]:
			var item_id: int = item_value
			var frame: Dictionary = actor_resource.ground_item(item_id)
			var texture := frame.get("texture") as Texture2D
			if texture == null:
				continue
			var position := Vector2(
				gx * GRID_XP - view_x + (GRID_XP - texture.get_width()) * 0.5,
				gy * GRID_YP - view_y + (GRID_YP - texture.get_height()) * 0.5,
			)
			draw_texture(texture, position + Vector2(1, -1), Color(0, 0, 0, 0.5))
			draw_texture(texture, position, Color(1.35, 1.35, 1.35, 1) if mouse_over else Color.WHITE)
			if mouse_over and font:
				var item_name: String = actor_resource.item_name(item_id)
				var text_size := font.get_string_size(item_name, HORIZONTAL_ALIGNMENT_LEFT, -1, 15)
				font.draw_string(
					get_canvas_item(),
					Vector2(gx * GRID_XP - view_x + (GRID_XP - text_size.x) * 0.5, gy * GRID_YP - view_y - 4),
					item_name,
					HORIZONTAL_ALIGNMENT_LEFT,
					-1,
					15,
					Color.YELLOW,
				)


func _draw_dead_actors(view_x: int, view_y: int, now: int) -> void:
	for creature_value in game_state.creatures.values():
		var creature: Dictionary = creature_value
		if _is_dead_actor(creature):
			_draw_creature(creature, view_x, view_y, _dead_actor_alpha(creature, now))
	if game_state.player_action_type == DEAD_ACTION:
		_draw_player(view_x, view_y)


func _is_dead_actor(creature: Dictionary) -> bool:
	return creature.get("action_type", 0) == DEAD_ACTION


func _dead_actor_alpha(creature: Dictionary, now: int) -> float:
	var requested_ms: int = creature.get("dead_fade_requested_ms", 0)
	if requested_ms <= 0:
		return 1.0
	var frame_delay := 10000.0 / float(clampi(creature.get("action_speed", 100), 20, 500))
	var animation_done_ms := int(creature.get("action_started_ms", requested_ms) + DEAD_FRAME_COUNT * frame_delay)
	var fade_start_ms := maxi(requested_ms, animation_done_ms)
	var fade_step := maxi(0, floori(float(now - fade_start_ms) / frame_delay))
	var fade_value := mini(255, 1 + fade_step * DEAD_FADE_STEP)
	return float(255 - fade_value) / 255.0


func _update_dead_fades(now: int) -> void:
	if game_state == null:
		return
	var remove_uids: Array[int] = []
	for uid_value in game_state.creatures:
		var uid: int = uid_value
		var creature: Dictionary = game_state.creatures[uid]
		if _is_dead_actor(creature) and creature.get("dead_fade_requested_ms", 0) > 0 and _dead_actor_alpha(creature, now) <= 0.0:
			remove_uids.append(uid)
	for uid in remove_uids:
		game_state.remove_creature(uid)


func _draw_ground_item_stars(x0: int, y0: int, x1: int, y1: int, view_x: int, view_y: int) -> void:
	if _ground_item_star_ratio > 1.0:
		return
	var frame: Dictionary = actor_resource.frame("proguse", GROUND_ITEM_STAR_GFX_ID)
	var texture := frame.get("texture") as Texture2D
	if texture == null:
		return
	var current_size := _ground_item_star_size(texture.get_width())
	if current_size <= 0:
		return
	var scale := Vector2(float(current_size) / texture.get_width(), float(current_size) / texture.get_height())
	var angle := deg_to_rad(roundf(_ground_item_star_ratio * 360.0))
	for grid_key in game_state.ground_items:
		var items: Array = game_state.ground_items[grid_key]
		if items.is_empty():
			continue
		var parts: PackedStringArray = grid_key.split(",")
		if parts.size() != 2:
			continue
		var gx := int(parts[0])
		var gy := int(parts[1])
		if gx < x0 or gx > x1 or gy < y0 or gy > y1:
			continue
		var center := Vector2(gx * GRID_XP - view_x + GRID_XP * 0.5, gy * GRID_YP - view_y + GRID_YP * 0.5)
		draw_set_transform(center, angle, scale)
		draw_texture(texture, Vector2(texture.get_width(), texture.get_height()) * -0.5, Color(1, 1, 1, 128.0 / 255.0))
	draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)


func _ground_item_star_size(texture_width: int) -> int:
	if _ground_item_star_ratio > 1.0:
		return 0
	return roundi(_ground_item_star_ratio * texture_width / GROUND_ITEM_STAR_CYCLE)


func _draw_object_row(depth: int, y: int, x0: int, x1: int, view_x: int, view_y: int) -> void:
	if depth < 0 or depth >= world_resource.objects.size():
		return
	var depth_objects: Dictionary = world_resource.objects[depth]
	for x in range(x0, x1 + 1):
		for object_data in depth_objects.get(x + y * map_width, []):
			var texture_id: int = object_data[0]
			var flags: int = object_data[1]
			var frame_count: int = object_data[3]
			if flags & 1 and frame_count > 0:
				var tick_type: int = clampi(object_data[2], 0, ANIMATION_DELAYS.size() - 1)
				texture_id += floori(float(Time.get_ticks_msec()) / ANIMATION_DELAYS[tick_type]) % frame_count
			var texture: Texture2D = world_resource.texture(texture_id)
			if texture == null:
				continue
			var alpha := 96.0 / 255.0 if flags & 2 else 1.0
			var position := Vector2(x * GRID_XP - view_x, (y + 1) * GRID_YP - view_y - texture.get_height())
			draw_texture(texture, position, Color(1.0, 1.0, 1.0, alpha))


func _draw_strike_row(y: int, x0: int, x1: int, view_x: int, view_y: int, now: int) -> void:
	var to_remove: Array[String] = []
	for x in range(x0, x1 + 1):
		var key := "%d,%d" % [x, y]
		if not game_state.strike_grids.has(key):
			continue
		var age: int = now - game_state.strike_grids[key]
		if age > 1000:
			to_remove.append(key)
			continue
		var alpha := 1.0 - float(age) / 1000.0
		draw_rect(Rect2(x * GRID_XP - view_x, y * GRID_YP - view_y, GRID_XP, GRID_YP), Color(1, 0.2, 0.2, alpha * 0.5))
	for key in to_remove:
		game_state.strike_grids.erase(key)


func _resolve_magic_effects(now: int) -> Array:
	var active: Array = []
	var pending: Array = []
	for effect_value in game_state.magic_effects:
		var effect: Dictionary = effect_value
		var resolved := _resolve_magic_effect(effect, now)
		if resolved.is_empty():
			continue
		active.append(resolved)
		pending.append(effect)
	game_state.magic_effects = pending
	return active


func _resolve_attached_magic(now: int) -> Dictionary:
	var active: Dictionary = {}
	var pending: Array = []
	for effect_value in game_state.attached_magic_effects:
		var effect: Dictionary = effect_value
		var target_uid: int = effect.get("target_uid", 0)
		if target_uid != game_state.player_uid and not game_state.creatures.has(target_uid):
			continue
		var magic_id: int = effect.get("magicID", 0)
		var stage: int = effect.get("stage", MAGIC_STAGE_RUN)
		var meta: PackedInt32Array = actor_resource.magic_layout(magic_id, stage)
		if meta.is_empty() or meta[2] <= 0:
			continue
		var speed := maxi(1, meta[4])
		var cycle_duration := maxi(100, roundi(meta[2] * 1000.0 / (10.0 * speed / 100.0)))
		var elapsed := maxi(0, now - int(effect.get("start_time", now)))
		var cycles := maxi(1, effect.get("cycles", 1))
		if elapsed >= cycle_duration * cycles:
			if effect.get("kind", "") != "shield_hit":
				continue
			effect["stage"] = MAGIC_STAGE_RUN
			effect["kind"] = "shield"
			effect["start_time"] = int(effect.get("start_time", now)) + cycle_duration
			effect["cycles"] = 2
			stage = MAGIC_STAGE_RUN
			meta = actor_resource.magic_layout(magic_id, stage)
			if meta.is_empty() or meta[2] <= 0:
				continue
			speed = maxi(1, meta[4])
			cycle_duration = maxi(100, roundi(meta[2] * 1000.0 / (10.0 * speed / 100.0)))
			elapsed = maxi(0, now - int(effect.start_time))
			cycles = 2
		var cycle := floori(float(elapsed) / cycle_duration)
		var cycle_elapsed := elapsed % cycle_duration
		var absolute_frame := floori(float(cycle_elapsed) / 1000.0 * 10.0 * speed / 100.0)
		var alpha_mod := 1.0
		if effect.get("kind", "") in ["shield", "shield_hit"]:
			alpha_mod = 240.0 / 255.0
		elif effect.get("kind", "") == "yin_yang_ring" and cycle == 1:
			alpha_mod = maxf(absf(cos(float(cycle_elapsed) / 800.0)), 32.0 / 255.0)
		var resolved := {
			"magic_id": magic_id,
			"kind": effect.get("kind", ""),
			"meta": meta,
			"frame": mini(absolute_frame, meta[2] - 1),
			"direction": 0,
			"alpha_mod": alpha_mod,
			"mirror_vertical": effect.get("kind", "") == "thunderbolt" and absolute_frame <= 3,
		}
		var target_effects: Array = active.get(target_uid, [])
		target_effects.append(resolved)
		active[target_uid] = target_effects
		pending.append(effect)
		if effect.get("play_seff", false) and not effect.get("_seff_played", false):
			effect["_seff_played"] = true
			var target_grid := _attached_target_grid(target_uid)
			AudioService.play_seff_at(actor_resource.magic_seff(magic_id, stage), roundi(target_grid.x), roundi(target_grid.y), game_state.player_x, game_state.player_y)
	game_state.attached_magic_effects = pending
	return active


func _attached_target_grid(uid: int) -> Vector2:
	if uid == game_state.player_uid:
		return Vector2(game_state.player_x, game_state.player_y)
	var creature: Dictionary = game_state.creatures.get(uid, {})
	return Vector2(creature.get("x", 0), creature.get("y", 0))


func _resolve_magic_effect(effect: Dictionary, now: int) -> Dictionary:
	var magic_id: int = effect.get("magicID", 0)
	if magic_id <= 0:
		return {}
	var stages := [MAGIC_STAGE_RUN, MAGIC_STAGE_EXPLODE] if effect.get("source", "") == "cast" else [MAGIC_STAGE_SPELL, MAGIC_STAGE_RUN, MAGIC_STAGE_EXPLODE]
	var special_kind := _special_magic_kind(magic_id)
	var elapsed := maxi(0, now - int(effect.get("start_time", now)))
	var projectile_kind := _projectile_action_magic_kind(magic_id)
	if not projectile_kind.is_empty() and effect.get("source", "") != "cast":
		return _resolve_projectile_action_magic(effect, magic_id, projectile_kind, elapsed)
	var attachment_policy := _action_attachment_policy(magic_id)
	if not attachment_policy.is_empty() and effect.get("source", "") != "cast":
		return _resolve_target_attached_action_magic(effect, magic_id, attachment_policy, elapsed)
	var fixed_action_kind := _fixed_action_magic_kind(magic_id)
	if not fixed_action_kind.is_empty() and effect.get("source", "") != "cast":
		return _resolve_fixed_action_magic(effect, magic_id, fixed_action_kind, elapsed)
	if not special_kind.is_empty() and effect.get("source", "") != "cast":
		return _resolve_special_action_magic(effect, magic_id, special_kind, elapsed)
	for stage in stages:
		var meta: PackedInt32Array = actor_resource.magic_layout(magic_id, stage)
		if meta.is_empty():
			continue
		var duration := _magic_stage_duration(meta, effect)
		if elapsed < duration:
			var resolved := _make_resolved_magic(effect, meta, stage, elapsed, duration)
			_play_magic_stage_seff(effect, magic_id, stage, resolved.position)
			return resolved
		elapsed -= duration
	return {}


func _projectile_action_magic_kind(magic_id: int) -> String:
	var magic_name: String = actor_resource.magic_names.get(magic_id, "")
	if magic_name in ["月魂断玉", "月魂灵波", "冰月震天"]:
		return "fixed_gfx"
	if magic_name in ["火球术", "大火球", "霹雳掌", "风掌", "灵魂火符", "冰月神掌", "幽灵盾", "神圣战甲术", "强魔震法", "猛虎强势", "集体隐身术"]:
		return "directional"
	return ""


func _resolve_projectile_action_magic(effect: Dictionary, magic_id: int, kind: String, elapsed: int) -> Dictionary:
	var speed := clampi(effect.get("speed", 100), 20, 500)
	var trigger_delay := roundi(4.0 * 100.0 * 100.0 / speed)
	var resolved := {"special_kind": "follow_projectile", "components": [], "underlays": [], "on_ground": false}
	var startup_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_SPELL)
	if not startup_meta.is_empty():
		var startup_duration := _magic_stage_duration(startup_meta, effect)
		if elapsed < startup_duration:
			var startup := _make_resolved_magic(effect, startup_meta, MAGIC_STAGE_SPELL, elapsed, startup_duration)
			_play_magic_stage_seff(effect, magic_id, MAGIC_STAGE_SPELL, startup.position)
			resolved.components.append(_resolved_component(startup.meta, startup.frame, startup.direction, startup.position))
	if elapsed < trigger_delay:
		return resolved
	if effect.get("_projectile_done", false):
		return resolved if not resolved.components.is_empty() else {}
	var run_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_RUN)
	if run_meta.is_empty() or run_meta[2] <= 0:
		return resolved if not resolved.components.is_empty() else {}
	if not effect.has("_projectile_position"):
		var source_pixel := Vector2(effect.get("x", 0) * GRID_XP, effect.get("y", 0) * GRID_YP)
		var target_pixel: Variant = _projectile_target_pixel(effect.get("aimUID", 0))
		var fly_direction: int = _projectile_direction16(source_pixel, target_pixel, effect.get("direction", 1))
		effect["_projectile_start"] = source_pixel
		effect["_projectile_position"] = source_pixel
		effect["_projectile_fly_direction"] = fly_direction
		effect["_projectile_gfx_direction"] = 0 if kind == "fixed_gfx" else fly_direction
	var position: Vector2 = effect.get("_projectile_position", Vector2.ZERO)
	var gfx_direction: int = effect.get("_projectile_gfx_direction", 0)
	var target_uid: int = effect.get("aimUID", 0)
	var live_target: Variant = _projectile_target_pixel(target_uid)
	var target_offset: Vector2 = Vector2(actor_resource.magic_target_offset(magic_id, MAGIC_STAGE_RUN, gfx_direction))
	var head_position: Vector2 = position + target_offset
	var move_offset: Vector2
	var previous_distance2: float = INF
	if live_target != null:
		var target_position: Vector2 = live_target
		var difference := target_position - head_position
		previous_distance2 = difference.length_squared()
		move_offset = Vector2.ZERO if difference == Vector2.ZERO else _projectile_move_offset(difference)
	else:
		move_offset = effect.get("_projectile_last_fly_offset", _projectile_direction_offset(effect.get("_projectile_fly_direction", 0)))
	position += move_offset
	effect["_projectile_position"] = position
	effect["_projectile_last_fly_offset"] = move_offset
	head_position = position + target_offset
	var done := _projectile_out_of_range(effect)
	if live_target != null:
		var remaining: Vector2 = Vector2(live_target) - head_position
		done = done or (absf(remaining.x) < 24.0 and absf(remaining.y) < 16.0) or remaining.length_squared() > previous_distance2
	if done:
		effect["_projectile_done"] = true
		if live_target != null and not effect.get("_projectile_impact_spawned", false):
			effect["_projectile_impact_spawned"] = true
			game_state.attached_magic_effects.append({
				"magicID": magic_id,
				"target_uid": target_uid,
				"start_time": int(effect.get("start_time", 0)) + elapsed,
				"cycles": 1,
				"kind": "projectile_impact",
				"stage": MAGIC_STAGE_EXPLODE,
				"play_seff": true,
			})
			game_state.state_changed.emit()
		return resolved if not resolved.components.is_empty() else {}
	var run_elapsed := elapsed - trigger_delay
	var absolute_frame := _magic_absolute_frame(run_meta, run_elapsed)
	var frame := absolute_frame % run_meta[2] if run_meta[7] & 1 else absolute_frame
	var grid_position := Vector2(position.x / GRID_XP, position.y / GRID_YP)
	_play_magic_stage_seff(effect, magic_id, MAGIC_STAGE_RUN, grid_position)
	resolved.components.append(_resolved_component(run_meta, frame, gfx_direction, grid_position))
	return resolved


func _projectile_target_pixel(uid: int) -> Variant:
	if uid == 0:
		return null
	var target: Dictionary = _actor_target_rects.get(uid, {})
	if not target.is_empty() and target.has("world_center"):
		return target.world_center
	if uid == game_state.player_uid:
		if game_state.player_action_type == DEAD_ACTION:
			return null
		return Vector2(game_state.player_x * GRID_XP, game_state.player_y * GRID_YP)
	var creature: Dictionary = game_state.creatures.get(uid, {})
	if creature.is_empty() or creature.get("action_type", 2) == DEAD_ACTION:
		return null
	return Vector2(creature.get("x", 0) * GRID_XP, creature.get("y", 0) * GRID_YP)


func _projectile_direction16(source: Vector2, target: Variant, network_direction: int) -> int:
	if target == null or Vector2(target) == source:
		return (clampi(network_direction, 1, 8) - 1) * 2
	var difference := Vector2(target) - source
	var angle := fposmod(atan2(difference.x * GRID_YP, -difference.y * GRID_XP), TAU)
	return roundi(angle / TAU * 16.0) % 16


func _projectile_move_offset(difference: Vector2) -> Vector2:
	var scale := 20.0 / difference.length()
	return Vector2(roundi(difference.x * scale), roundi(difference.y * scale))


func _projectile_direction_offset(direction: int) -> Vector2:
	var angle := float(posmod(direction, 16)) * TAU / 16.0
	return Vector2(roundi(sin(angle) * 20.0), roundi(-cos(angle) * 20.0))


func _projectile_out_of_range(effect: Dictionary) -> bool:
	var start: Vector2 = effect.get("_projectile_start", Vector2.ZERO)
	var current: Vector2 = effect.get("_projectile_position", start)
	var start_grid := Vector2(floori(start.x / GRID_XP), floori(start.y / GRID_YP))
	var current_grid := Vector2(floori(current.x / GRID_XP), floori(current.y / GRID_YP))
	return start_grid.distance_to(current_grid) > 255.0


func _action_attachment_policy(magic_id: int) -> String:
	var magic_name: String = actor_resource.magic_names.get(magic_id, "")
	if magic_name == "治愈术":
		return "heal_fallback"
	if magic_name in ["乾坤大挪移", "圣言术", "云寂术", "回生术", "施毒术", "诱惑之光", "移花接玉"]:
		return "target_only"
	return ""


func _resolve_target_attached_action_magic(effect: Dictionary, magic_id: int, policy: String, elapsed: int) -> Dictionary:
	var speed := clampi(effect.get("speed", 100), 20, 500)
	var trigger_delay := roundi(3.0 * 100.0 * 100.0 / speed)
	var resolved := {"special_kind": "target_attachment", "components": [], "underlays": [], "on_ground": false}
	var startup_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_SPELL)
	if not startup_meta.is_empty():
		var startup_duration := _magic_stage_duration(startup_meta, effect)
		if elapsed < startup_duration:
			var startup := _make_resolved_magic(effect, startup_meta, MAGIC_STAGE_SPELL, elapsed, startup_duration)
			_play_magic_stage_seff(effect, magic_id, MAGIC_STAGE_SPELL, startup.position)
			resolved.components.append(_resolved_component(startup.meta, startup.frame, startup.direction, startup.position))
	if elapsed < trigger_delay:
		return resolved
	if not effect.get("_attachment_spawned", false):
		effect["_attachment_spawned"] = true
		var target_uid: int = effect.get("aimUID", 0)
		if not _attached_target_exists(target_uid):
			target_uid = effect.get("uid", 0) if policy == "heal_fallback" else 0
		if _attached_target_exists(target_uid):
			game_state.attached_magic_effects.append({
				"magicID": magic_id,
				"target_uid": target_uid,
				"start_time": int(effect.get("start_time", 0)) + trigger_delay,
				"cycles": 1,
				"kind": "action_attachment",
			})
			game_state.state_changed.emit()
	return resolved if not resolved.components.is_empty() else {}


func _attached_target_exists(uid: int) -> bool:
	return uid != 0 and (uid == game_state.player_uid or game_state.creatures.has(uid))


func _fixed_action_magic_kind(magic_id: int) -> String:
	var magic_name: String = actor_resource.magic_names.get(magic_id, "")
	if magic_name == "击风":
		return "run_explode"
	if magic_name in ["冰咆哮", "龙卷风", "爆裂火焰", "地狱雷光", "怒神霹雳", "群体治愈术"]:
		return "run"
	return ""


func _resolve_fixed_action_magic(effect: Dictionary, magic_id: int, kind: String, elapsed: int) -> Dictionary:
	var speed := clampi(effect.get("speed", 100), 20, 500)
	var trigger_delay := roundi(3.0 * 100.0 * 100.0 / speed)
	var resolved := {"special_kind": "fixed_action", "components": [], "underlays": [], "on_ground": false}
	var startup_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_SPELL)
	if not startup_meta.is_empty():
		var startup_duration := _magic_stage_duration(startup_meta, effect)
		if elapsed < startup_duration:
			var startup := _make_resolved_magic(effect, startup_meta, MAGIC_STAGE_SPELL, elapsed, startup_duration)
			_play_magic_stage_seff(effect, magic_id, MAGIC_STAGE_SPELL, startup.position)
			resolved.components.append(_resolved_component(startup.meta, startup.frame, startup.direction, startup.position))
	if elapsed < trigger_delay:
		return resolved
	if not effect.has("_fixed_position"):
		effect["_fixed_position"] = _effect_target_grid(effect)
	var position: Vector2 = effect.get("_fixed_position", Vector2.ZERO)
	var stage := MAGIC_STAGE_RUN
	var meta: PackedInt32Array = actor_resource.magic_layout(magic_id, stage)
	var stage_elapsed := elapsed - trigger_delay
	if meta.is_empty():
		return resolved if not resolved.components.is_empty() else {}
	var run_duration := _magic_frame_duration(meta)
	if kind == "run_explode" and stage_elapsed >= run_duration:
		stage = MAGIC_STAGE_EXPLODE
		stage_elapsed -= run_duration
		meta = actor_resource.magic_layout(magic_id, stage)
	if not meta.is_empty() and stage_elapsed < _magic_frame_duration(meta):
		_play_magic_stage_seff(effect, magic_id, stage, position)
		var frame := mini(_magic_absolute_frame(meta, stage_elapsed), meta[2] - 1)
		resolved.components.append(_resolved_component(meta, frame, 0, position))
	return resolved if not resolved.components.is_empty() else {}


func _special_magic_kind(magic_id: int) -> String:
	if magic_id == actor_resource.magic_id("地狱火"):
		return "hellfire"
	if magic_id == actor_resource.magic_id("冰沙掌"):
		return "ice_thrust"
	if magic_id == actor_resource.magic_id("风震天"):
		return "wind_chain"
	if magic_id == actor_resource.magic_id("疾光电影"):
		return "laser"
	return ""


func _resolve_special_action_magic(effect: Dictionary, magic_id: int, kind: String, elapsed: int) -> Dictionary:
	var run_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_RUN)
	if run_meta.is_empty():
		return {}
	var speed := clampi(effect.get("speed", 100), 20, 500)
	var trigger_frame := 4 if kind == "wind_chain" else 3
	var trigger_delay := roundi(float(trigger_frame) * 100.0 * 100.0 / speed)
	var run_elapsed := elapsed - trigger_delay
	var resolved: Dictionary = {}
	match kind:
		"hellfire", "ice_thrust":
			resolved = _resolve_special_magic(effect, magic_id, kind, run_meta, run_elapsed)
		"wind_chain":
			resolved = _resolve_wind_chain(effect, magic_id, run_meta, run_elapsed)
		"laser":
			resolved = _resolve_caster_laser(effect, magic_id, run_meta, run_elapsed)
	var startup_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_SPELL)
	if not startup_meta.is_empty():
		var startup_duration := _magic_stage_duration(startup_meta, effect)
		if elapsed < startup_duration:
			var startup := _make_resolved_magic(effect, startup_meta, MAGIC_STAGE_SPELL, elapsed, startup_duration)
			_play_magic_stage_seff(effect, magic_id, MAGIC_STAGE_SPELL, startup.position)
			if resolved.is_empty():
				resolved = {"special_kind": kind, "components": [], "underlays": [], "on_ground": false}
			var components: Array = resolved.get("components", [])
			components.append(_resolved_component(startup.meta, startup.frame, startup.direction, startup.position))
			resolved["components"] = components
	return resolved


func _resolve_wind_chain(effect: Dictionary, magic_id: int, run_meta: PackedInt32Array, elapsed: int) -> Dictionary:
	var explode_meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_EXPLODE)
	if explode_meta.is_empty():
		return {}
	var run_duration := _magic_frame_duration(run_meta)
	var explode_duration := _magic_frame_duration(explode_meta)
	var total_duration := 7 * run_duration + explode_duration
	if elapsed >= total_duration:
		return {}
	var components: Array = []
	if elapsed >= 0:
		var source := Vector2(effect.get("x", 0), effect.get("y", 0))
		var step := Vector2(_direction_step(effect.get("direction", 1)))
		var segment := floori(float(elapsed) / run_duration)
		if segment < 7:
			var segment_elapsed := elapsed - segment * run_duration
			var position := source + step * (segment + 1)
			_play_propagated_seff(effect, magic_id, segment, MAGIC_STAGE_RUN, position)
			components.append(_resolved_component(run_meta, mini(_magic_absolute_frame(run_meta, segment_elapsed), run_meta[2] - 1), 0, position))
		else:
			var explode_elapsed := elapsed - 7 * run_duration
			var position := source + step * 8
			_play_propagated_seff(effect, magic_id, 7, MAGIC_STAGE_EXPLODE, position)
			components.append(_resolved_component(explode_meta, mini(_magic_absolute_frame(explode_meta, explode_elapsed), explode_meta[2] - 1), 0, position))
	return {"special_kind": "wind_chain", "components": components, "underlays": [], "on_ground": false}


func _resolve_caster_laser(effect: Dictionary, magic_id: int, run_meta: PackedInt32Array, elapsed: int) -> Dictionary:
	if elapsed >= _magic_frame_duration(run_meta):
		return {}
	var components: Array = []
	if elapsed >= 0:
		var position := Vector2(effect.get("x", 0), effect.get("y", 0))
		_play_propagated_seff(effect, magic_id, 0, MAGIC_STAGE_RUN, position)
		components.append(_resolved_component(run_meta, mini(_magic_absolute_frame(run_meta, elapsed), run_meta[2] - 1), clampi(effect.get("direction", 1), 1, 8) - 1, position))
	return {"special_kind": "laser", "components": components, "underlays": [], "on_ground": false}


func _play_propagated_seff(effect: Dictionary, magic_id: int, segment: int, stage: int, position: Vector2) -> void:
	var mask: int = effect.get("_propagated_seff_mask", 0)
	var bit := 1 << segment
	if mask & bit:
		return
	effect["_propagated_seff_mask"] = mask | bit
	AudioService.play_seff_at(actor_resource.magic_seff(magic_id, stage), roundi(position.x), roundi(position.y), game_state.player_x, game_state.player_y)


func _resolve_special_magic(effect: Dictionary, magic_id: int, kind: String, run_meta: PackedInt32Array, run_elapsed: int) -> Dictionary:
	var auxiliary_name := "魔法特效_火焰灰烬" if kind == "hellfire" else "魔法特效_冰刺"
	var auxiliary_meta: PackedInt32Array = actor_resource.magic_layout(actor_resource.magic_id(auxiliary_name), MAGIC_STAGE_RUN)
	if auxiliary_meta.is_empty() or auxiliary_meta[2] <= 0:
		return {}
	var wave_lifetime := 0
	if kind == "hellfire":
		var ash_trigger := _magic_frame_reach_duration(run_meta, 10)
		wave_lifetime = maxi(_magic_frame_duration(run_meta), ash_trigger + FIRE_ASH_FADE_IN_MS + FIRE_ASH_HOLD_MS + FIRE_ASH_FADE_OUT_MS)
	else:
		wave_lifetime = _magic_frame_reach_duration(auxiliary_meta, ICE_SLAG_FADE_IN_FRAMES + ICE_SLAG_HOLD_FRAMES + ICE_SLAG_FADE_OUT_FRAMES)
	if run_elapsed >= SPECIAL_WAVE_COUNT * SPECIAL_WAVE_DELAY_MS + wave_lifetime:
		return {}
	_ensure_special_variants(effect)
	var direction := clampi(effect.get("direction", 1), 1, 8)
	var step := _direction_step(direction)
	var source := Vector2(effect.get("x", 0), effect.get("y", 0))
	var components: Array = []
	var underlays: Array = []
	for distance in range(1, SPECIAL_WAVE_COUNT + 1):
		var wave_elapsed := run_elapsed - distance * SPECIAL_WAVE_DELAY_MS
		if wave_elapsed < 0:
			continue
		var wave_position := source + Vector2(step * distance)
		if not can_walk(roundi(wave_position.x), roundi(wave_position.y)):
			continue
		_play_special_wave_seff(effect, magic_id, distance, wave_position)
		var variants: Array = effect.get("_special_variants", [])
		var variant: Dictionary = variants[distance - 1]
		if kind == "hellfire":
			_resolve_hellfire_wave(run_meta, auxiliary_meta, direction, wave_position, wave_elapsed, variant, components, underlays)
		else:
			_resolve_ice_wave(auxiliary_meta, direction, wave_position, wave_elapsed, variant, components, underlays)
	return {
		"special_kind": kind,
		"components": components,
		"underlays": underlays,
		"on_ground": false,
	}


func _play_special_wave_seff(effect: Dictionary, magic_id: int, distance: int, position: Vector2) -> void:
	var mask: int = effect.get("_special_seff_mask", 0)
	var bit := 1 << (distance - 1)
	if mask & bit:
		return
	effect["_special_seff_mask"] = mask | bit
	AudioService.play_seff_at(actor_resource.magic_seff(magic_id, MAGIC_STAGE_RUN), roundi(position.x), roundi(position.y), game_state.player_x, game_state.player_y)


func _ensure_special_variants(effect: Dictionary) -> void:
	if effect.has("_special_variants"):
		return
	var variants: Array = []
	for _index in SPECIAL_WAVE_COUNT:
		variants.append({
			"ash_direction": randi_range(0, 4),
			"frame_offset": randi_range(0, 9),
			"rotation": randi_range(0, 359),
			"slag_indices": [randi_range(0, 1), randi_range(0, 1)],
			"ice_rotations": [randi_range(0, 359), randi_range(0, 359)],
		})
	effect["_special_variants"] = variants


func _direction_step(direction: int) -> Vector2i:
	const STEPS := [
		Vector2i(0, -1), Vector2i(1, -1), Vector2i(1, 0), Vector2i(1, 1),
		Vector2i(0, 1), Vector2i(-1, 1), Vector2i(-1, 0), Vector2i(-1, -1),
	]
	return STEPS[clampi(direction, 1, 8) - 1]


func _magic_absolute_frame(meta: PackedInt32Array, elapsed: int) -> int:
	return roundi(float(maxi(0, elapsed)) / 1000.0 * 10.0 * maxi(1, meta[4]) / 100.0)


func _magic_frame_duration(meta: PackedInt32Array) -> int:
	return _magic_frame_reach_duration(meta, meta[2])


func _magic_frame_reach_duration(meta: PackedInt32Array, frame_count: int) -> int:
	if frame_count <= 0:
		return 0
	return maxi(1, ceili((float(frame_count) - 0.5) * 1000.0 / (10.0 * maxi(1, meta[4]) / 100.0)))


func _resolved_component(meta: PackedInt32Array, frame: int, direction: int, position: Vector2, alpha_mod := 1.0) -> Dictionary:
	return {"meta": meta, "frame": frame, "direction": direction, "position": position, "alpha_mod": alpha_mod, "on_ground": bool(meta[7] & 2)}


func _resolve_hellfire_wave(run_meta: PackedInt32Array, ash_meta: PackedInt32Array, direction: int, position: Vector2, elapsed: int, variant: Dictionary, components: Array, underlays: Array) -> void:
	var primary_frame := _magic_absolute_frame(run_meta, elapsed)
	if elapsed < _magic_frame_duration(run_meta):
		components.append(_resolved_component(run_meta, mini(primary_frame, run_meta[2] - 1), direction % 2, position))
		components.append(_resolved_component(run_meta, mini(primary_frame, run_meta[2] - 1), (direction + 1) % 2, position - Vector2(_direction_step(direction)) * 0.5))
	var ash_trigger := _magic_frame_reach_duration(run_meta, 10)
	var ash_elapsed := elapsed - ash_trigger
	var ash_duration := FIRE_ASH_FADE_IN_MS + FIRE_ASH_HOLD_MS + FIRE_ASH_FADE_OUT_MS
	if ash_elapsed < 0 or ash_elapsed >= ash_duration:
		return
	var alpha := _fire_ash_alpha(ash_elapsed)
	var ash_frame := _magic_absolute_frame(ash_meta, ash_elapsed) + int(variant.get("frame_offset", 0))
	if ash_meta[7] & 1:
		ash_frame %= ash_meta[2]
	components.append(_resolved_component(ash_meta, ash_frame, int(variant.get("ash_direction", 0)), position, alpha))
	underlays.append({
		"texture_id": FIRE_ASH_TEXTURE_ID,
		"crop": Vector2i(102, 72),
		"position": position,
		"rotation": int(variant.get("rotation", 0)),
		"alpha_mod": alpha,
		"meta": ash_meta,
	})


func _fire_ash_alpha(elapsed: int) -> float:
	if elapsed < FIRE_ASH_FADE_IN_MS:
		return float(elapsed) / FIRE_ASH_FADE_IN_MS
	if elapsed < FIRE_ASH_FADE_IN_MS + FIRE_ASH_HOLD_MS:
		return 1.0
	return 1.0 - float(elapsed - FIRE_ASH_FADE_IN_MS - FIRE_ASH_HOLD_MS) / FIRE_ASH_FADE_OUT_MS


func _resolve_ice_wave(child_meta: PackedInt32Array, direction: int, position: Vector2, elapsed: int, variant: Dictionary, components: Array, underlays: Array) -> void:
	var absolute_frame := _magic_absolute_frame(child_meta, elapsed)
	var total_frames := ICE_SLAG_FADE_IN_FRAMES + ICE_SLAG_HOLD_FRAMES + ICE_SLAG_FADE_OUT_FRAMES
	if absolute_frame >= total_frames:
		return
	var shifted_position := position - Vector2(_direction_step(direction)) * 0.5
	if absolute_frame < child_meta[2]:
		components.append(_resolved_component(child_meta, absolute_frame, direction % 2, position))
		components.append(_resolved_component(child_meta, absolute_frame, (direction + 1) % 2, shifted_position))
	var alpha := _ice_slag_alpha(absolute_frame)
	var slag_indices: Array = variant.get("slag_indices", [0, 0])
	var ice_rotations: Array = variant.get("ice_rotations", [0, 0])
	var ground_positions := [position, shifted_position]
	for child_index in 2:
		var slag_index := clampi(slag_indices[child_index], 0, 1)
		var crop := Vector2i(76, 43) if slag_index == 0 else Vector2i(83, 53)
		underlays.append({
			"texture_id": ICE_SLAG_TEXTURE_IDS[slag_index],
			"crop": crop,
			"position": ground_positions[child_index],
			"rotation": int(ice_rotations[child_index]),
			"alpha_mod": alpha,
			"meta": child_meta,
		})


func _ice_slag_alpha(frame: int) -> float:
	if frame < ICE_SLAG_FADE_IN_FRAMES:
		return float(frame) / ICE_SLAG_FADE_IN_FRAMES
	if frame < ICE_SLAG_FADE_IN_FRAMES + ICE_SLAG_HOLD_FRAMES:
		return 1.0
	return 1.0 - float(frame - ICE_SLAG_FADE_IN_FRAMES - ICE_SLAG_HOLD_FRAMES) / ICE_SLAG_FADE_OUT_FRAMES


func _play_magic_stage_seff(effect: Dictionary, magic_id: int, stage: int, position: Vector2) -> void:
	var stage_mask: int = effect.get("_seff_stage_mask", 0)
	var stage_bit := 1 << stage
	if stage_mask & stage_bit:
		return
	effect["_seff_stage_mask"] = stage_mask | stage_bit
	var seff_id: int = actor_resource.magic_seff(magic_id, stage)
	AudioService.play_seff_at(seff_id, roundi(position.x), roundi(position.y), game_state.player_x, game_state.player_y)


func _magic_stage_duration(meta: PackedInt32Array, effect: Dictionary) -> int:
	var speed := maxi(1, meta[4])
	var fps := 10.0 * speed / 100.0
	var duration := maxi(100, roundi(meta[2] * 1000.0 / fps))
	if meta[5] == MAGIC_TYPE_FOLLOW:
		var source := Vector2(effect.get("x", 0) * GRID_XP, effect.get("y", 0) * GRID_YP)
		var target_grid := _effect_target_grid(effect)
		var target := Vector2(target_grid.x * GRID_XP, target_grid.y * GRID_YP)
		duration = maxi(200, roundi(source.distance_to(target) * 5.0))
	elif meta[7] & 1:
		duration = 1200
	return duration


func _make_resolved_magic(effect: Dictionary, meta: PackedInt32Array, stage: int, elapsed: int, duration: int) -> Dictionary:
	var source_grid := Vector2(effect.get("x", 0), effect.get("y", 0))
	var target_grid := _effect_target_grid(effect)
	var position := target_grid
	if stage == MAGIC_STAGE_SPELL:
		position = source_grid
	elif meta[5] == MAGIC_TYPE_FOLLOW:
		position = source_grid.lerp(target_grid, clampf(float(elapsed) / duration, 0.0, 1.0))
	elif meta[5] == MAGIC_TYPE_BOUND and effect.get("aimUID", 0) == 0:
		position = source_grid
	var direction_index := _magic_direction_index(meta[6], source_grid, target_grid, effect.get("direction", 1))
	var absolute_frame := floori(float(elapsed) / 1000.0 * 10.0 * meta[4] / 100.0)
	var frame := absolute_frame % meta[2] if meta[7] & 1 else mini(absolute_frame, meta[2] - 1)
	return {
		"meta": meta,
		"stage": stage,
		"position": position,
		"frame": frame,
		"direction": direction_index,
		"on_ground": bool(meta[7] & 2),
	}


func _effect_target_grid(effect: Dictionary) -> Vector2:
	var aim_uid: int = effect.get("aimUID", 0)
	if aim_uid == game_state.player_uid:
		return Vector2(game_state.player_x, game_state.player_y)
	if aim_uid != 0:
		var creature: Dictionary = game_state.creatures.get(aim_uid, {})
		if not creature.is_empty():
			return Vector2(creature.get("x", effect.get("aimX", effect.get("x", 0))), creature.get("y", effect.get("aimY", effect.get("y", 0))))
	return Vector2(effect.get("aimX", effect.get("x", 0)), effect.get("aimY", effect.get("y", 0)))


func _magic_direction_index(dir_type: int, source: Vector2, target: Vector2, network_direction: int) -> int:
	if dir_type <= 1:
		return 0
	if dir_type <= 8:
		return clampi(network_direction, 1, dir_type) - 1
	var delta := target - source
	if delta == Vector2.ZERO:
		return ((clampi(network_direction, 1, 8) - 1) * 2) % dir_type
	var angle := fposmod(atan2(delta.x * GRID_YP, -delta.y * GRID_XP), TAU)
	return roundi(angle / TAU * dir_type) % dir_type


func _draw_magic_row(active: Array, y: int, on_ground: bool, view_x: int, view_y: int) -> void:
	for magic_value in active:
		var magic: Dictionary = magic_value
		if magic.has("components"):
			for component_value in magic.get("components", []):
				var component: Dictionary = component_value
				if component.get("on_ground", false) != on_ground:
					continue
				var component_position: Vector2 = component.position
				if on_ground and floori(component_position.y) != y:
					continue
				_draw_magic_frame(component.meta, component.frame, component.direction, component.position, view_x, view_y, component.get("alpha_mod", 1.0))
			continue
		if magic.on_ground != on_ground:
			continue
		var position: Vector2 = magic.position
		if on_ground and floori(position.y) != y:
			continue
		_draw_magic_frame(magic.meta, magic.frame, magic.direction, position, view_x, view_y)


func _draw_special_ground_underlays(active: Array, view_x: int, view_y: int) -> void:
	for magic_value in active:
		var magic: Dictionary = magic_value
		for underlay_value in magic.get("underlays", []):
			var underlay: Dictionary = underlay_value
			_draw_rotated_magic_region(underlay, view_x, view_y)


func _draw_rotated_magic_region(underlay: Dictionary, view_x: int, view_y: int) -> void:
	var sprite: Dictionary = actor_resource.frame("magic", underlay.get("texture_id", 0))
	var texture := sprite.get("texture") as Texture2D
	if texture == null:
		return
	var requested_crop: Vector2i = underlay.get("crop", Vector2i(texture.get_width(), texture.get_height()))
	var crop := Vector2i(mini(requested_crop.x, texture.get_width()), mini(requested_crop.y, texture.get_height()))
	var position: Vector2 = underlay.get("position", Vector2.ZERO)
	var offset: Vector2i = sprite.get("offset", Vector2i.ZERO)
	var draw_position := Vector2(position.x * GRID_XP - view_x + offset.x, position.y * GRID_YP - view_y + offset.y)
	var center := Vector2(crop) * 0.5
	var meta: PackedInt32Array = underlay.get("meta", PackedInt32Array())
	var color := _magic_mod_color(meta, float(underlay.get("alpha_mod", 1.0)) * 150.0 / 255.0)
	draw_set_transform(draw_position + center, deg_to_rad(float(underlay.get("rotation", 0))), Vector2.ONE)
	draw_texture_rect_region(texture, Rect2(-center, Vector2(crop)), Rect2(Vector2.ZERO, Vector2(crop)), color)
	draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)


func _draw_firewall_row(y: int, x0: int, x1: int, view_x: int, view_y: int, now: int) -> void:
	var fire_wall_id: int = actor_resource.magic_id("火墙")
	var meta: PackedInt32Array = actor_resource.magic_layout(fire_wall_id, MAGIC_STAGE_RUN)
	if meta.is_empty():
		return
	var frame_step := maxi(1, roundi(10000.0 / maxi(1, meta[4])))
	for firewall_value in game_state.firewalls:
		var firewall: Dictionary = firewall_value
		var x: int = firewall.get("x", -1)
		if firewall.get("y", -1) != y or x < x0 or x > x1:
			continue
		for index in range(maxi(0, firewall.get("count", 0))):
			var frame := int(now / frame_step + index * 2) % meta[2]
			_draw_magic_frame(meta, frame, 0, Vector2(x, y), view_x, view_y)


func _draw_magic_frame(meta: PackedInt32Array, frame: int, direction: int, grid_position: Vector2, view_x: int, view_y: int, alpha_mod := 1.0, mirror_vertical := false) -> void:
	if meta.size() < 8 or meta[7] & 4:
		return
	var texture_id: int = meta[0] + direction * meta[3] + frame
	var sprite: Dictionary = actor_resource.frame("magic", texture_id)
	if sprite.is_empty():
		return
	var color := _magic_mod_color(meta, alpha_mod)
	var offset: Vector2i = sprite.offset
	var draw_position := Vector2(grid_position.x * GRID_XP - view_x + offset.x, grid_position.y * GRID_YP - view_y + offset.y)
	draw_texture(sprite.texture, draw_position, color)
	if mirror_vertical:
		draw_set_transform(draw_position, 0.0, Vector2(1.0, -1.0))
		draw_texture(sprite.texture, Vector2.ZERO, color)
		draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)


func _magic_mod_color(meta: PackedInt32Array, alpha_mod: float) -> Color:
	if meta.size() < 2:
		return Color(1, 1, 1, alpha_mod)
	var packed_color: int = meta[1]
	return Color(
		float(packed_color & 0xFF) / 255.0,
		float((packed_color >> 8) & 0xFF) / 255.0,
		float((packed_color >> 16) & 0xFF) / 255.0,
		float((packed_color >> 24) & 0xFF) / 255.0 * alpha_mod,
	)


func _draw_attached_magic(uid: int, start_x: int, start_y: int) -> void:
	for magic_value in _active_attached_magic.get(uid, []):
		var magic: Dictionary = magic_value
		_draw_magic_frame(magic.meta, magic.frame, magic.direction, Vector2(float(start_x) / GRID_XP, float(start_y) / GRID_YP), 0, 0, magic.alpha_mod, magic.mirror_vertical)


func _draw_hero_attached_magic(uid: int, start_x: int, start_y: int, actor_direction: int, overlay: bool) -> void:
	for magic_value in _active_attached_magic.get(uid, []):
		var magic: Dictionary = magic_value
		var policy := _hero_attached_magic_draw_policy(magic.get("magic_id", 0), magic.get("kind", ""), actor_direction, overlay, magic.get("alpha_mod", 1.0))
		if not policy.draw:
			continue
		_draw_magic_frame(magic.meta, magic.frame, magic.direction, Vector2(float(start_x) / GRID_XP, float(start_y) / GRID_YP), 0, 0, policy.alpha_mod, magic.mirror_vertical)


func _hero_attached_magic_draw_policy(magic_id: int, kind: String, actor_direction: int, overlay: bool, base_alpha: float) -> Dictionary:
	if not overlay:
		return {"draw": true, "alpha_mod": base_alpha}
	var magic_name: String = actor_resource.magic_names.get(magic_id, "")
	if magic_name == "灵魂火符":
		return {"draw": actor_direction >= 5 and actor_direction <= 8, "alpha_mod": base_alpha}
	if kind in ["shield", "shield_hit"]:
		return {"draw": true, "alpha_mod": 240.0 / 255.0}
	return {"draw": true, "alpha_mod": base_alpha * 240.0 / 255.0}


func _draw_player(view_x: int, view_y: int) -> void:
	var draw_grid := _action_draw_grid(game_state.player_x, game_state.player_y, game_state.player_action_from_x, game_state.player_action_from_y, game_state.player_action_type, game_state.player_action_started_ms, game_state.player_action_speed)
	var px: int = roundi(draw_grid.x * GRID_XP) - view_x
	var py: int = roundi(draw_grid.y * GRID_YP) - view_y
	var center := Vector2(px + GRID_XP * 0.5, py + GRID_YP * 0.5)

	_draw_hero_attached_magic(game_state.player_uid, px, py, game_state.player_direction, false)
	if not _draw_hero_sprite(game_state.player_gender, game_state.player_direction, game_state.player_action_type, game_state.player_desp, px, py, game_state.player_action_started_ms, game_state.player_action_speed, game_state.player_action_magic_id, game_state.player_uid, game_state.player_y):
		draw_circle(Vector2(center.x + 2, center.y + 14), 12, Color(0, 0, 0, 0.3))
		draw_circle(center, 14, Color(0.3, 0.5, 0.9, 1.0))
	_draw_hero_attached_magic(game_state.player_uid, px, py, game_state.player_direction, true)
	_draw_player_say(game_state.player_uid, px, py)
	
	# The C++ client only enables actor HP/name overlays through debug/runtime flags.


func _draw_creature(c: Dictionary, view_x: int, view_y: int, body_alpha := 1.0) -> void:
	var draw_grid := _action_draw_grid(c.get("x", 0), c.get("y", 0), c.get("action_from_x", c.get("x", 0)), c.get("action_from_y", c.get("y", 0)), c.get("action_type", 2), c.get("action_started_ms", 0), c.get("action_speed", 100))
	var cx: int = roundi(draw_grid.x * GRID_XP) - view_x
	var cy: int = roundi(draw_grid.y * GRID_YP) - view_y
	var center := Vector2(cx + GRID_XP * 0.5, cy + GRID_YP * 0.5)
	
	# Cull if off-screen
	if cx < -GRID_XP or cx > SCREEN_W or cy < -GRID_YP or cy > SCREEN_H:
		return
	
	var c_type: int = c.get("type", 0)
	var sprite_drawn := false
	var uid: int = c.get("uid", 0)
	if c_type == 2:
		_draw_hero_attached_magic(uid, cx, cy, c.get("direction", 5), false)
	match c_type:
		1: sprite_drawn = _draw_monster_sprite(c, cx, cy, body_alpha)
		2: sprite_drawn = _draw_hero_sprite(c.get("gender", 0), c.get("direction", 5), c.get("action_type", 2), c.get("desp", {}), cx, cy, c.get("action_started_ms", 0), c.get("action_speed", 100), c.get("action_magic_id", 0), uid, c.get("y", 0))
		3: sprite_drawn = _draw_npc_sprite(c, cx, cy)
	if not sprite_drawn:
		draw_circle(Vector2(center.x + 2, center.y + 14), 10, Color(0, 0, 0, 0.3 * body_alpha))
		draw_circle(center, 12, Color(0.7, 0.2, 0.2, 0.9 * body_alpha))
	if c_type == 2:
		_draw_hero_attached_magic(uid, cx, cy, c.get("direction", 5), true)
	else:
		_draw_attached_magic(uid, cx, cy)
	
	if c_type == 2:
		_draw_player_say(uid, cx, cy)


func _draw_player_say(uid: int, start_x: int, start_y: int) -> void:
	var font := get_theme_default_font()
	if font == null:
		return
	var layout := _player_say_layout(uid, font, Time.get_ticks_msec())
	if layout.is_empty():
		return
	var board_x := float(start_x + GRID_XP / 2) - float(layout.width) / 2.0
	var draw_y := float(start_y - 70 - layout.height)
	var background := StyleBoxFlat.new()
	background.bg_color = Color(0, 0, 0, 128.0 / 255.0)
	background.set_corner_radius_all(3)
	for message_value in layout.messages:
		var message: Dictionary = message_value
		var box_rect := Rect2(board_x, draw_y, message.width, message.height)
		draw_style_box(background, box_rect)
		var baseline := draw_y + PLAYER_SAY_MARGIN + font.get_ascent(PLAYER_SAY_FONT_SIZE)
		for line_value in message.lines:
			font.draw_string(get_canvas_item(), Vector2(board_x + PLAYER_SAY_MARGIN, baseline), str(line_value), HORIZONTAL_ALIGNMENT_LEFT, -1, PLAYER_SAY_FONT_SIZE, Color.WHITE)
			baseline += message.line_height
		draw_y += message.height


func _player_say_layout(uid: int, font: Font, now: int) -> Dictionary:
	var active_messages: Array = []
	for message_value in game_state.player_say_messages.get(uid, []):
		var message: Dictionary = message_value
		if now - int(message.get("start_time", now)) < PLAYER_SAY_SHOW_TIME:
			active_messages.append(message)
	if active_messages.is_empty():
		game_state.player_say_messages.erase(uid)
		return {}
	game_state.player_say_messages[uid] = active_messages
	var result_messages: Array = []
	var board_width := 0
	var board_height := 0
	var line_height := ceili(font.get_height(PLAYER_SAY_FONT_SIZE))
	for message in active_messages:
		var lines := _wrap_player_say(str(message.get("text", "")), font)
		var text_width := 0
		for line in lines:
			text_width = maxi(text_width, ceili(font.get_string_size(line, HORIZONTAL_ALIGNMENT_LEFT, -1, PLAYER_SAY_FONT_SIZE).x))
		var box_width := text_width + PLAYER_SAY_MARGIN * 2
		var box_height := lines.size() * line_height + PLAYER_SAY_MARGIN * 2
		result_messages.append({
			"lines": lines,
			"line_height": line_height,
			"width": box_width,
			"height": box_height,
		})
		board_width = maxi(board_width, box_width)
		board_height += box_height
	return {"messages": result_messages, "width": board_width, "height": board_height}


func _wrap_player_say(text: String, font: Font) -> Array[String]:
	var result: Array[String] = []
	for paragraph in text.split("\n", true):
		if paragraph.is_empty():
			result.append("")
			continue
		var line := ""
		for character in paragraph:
			var candidate := line + character
			if not line.is_empty() and font.get_string_size(candidate, HORIZONTAL_ALIGNMENT_LEFT, -1, PLAYER_SAY_FONT_SIZE).x > PLAYER_SAY_WIDTH:
				result.append(line)
				line = character
			else:
				line = candidate
		result.append(line)
	return result


func _draw_hero_sprite(gender: int, direction: int, action_type: int, desp: Dictionary, start_x: int, start_y: int, action_started_ms := 0, action_speed := 100, magic_id := 0, uid := 0, map_y := 0) -> bool:
	var direction_index := clampi(direction, 1, 8) - 1
	var motion_data := _hero_motion(action_type, magic_id, desp)
	var frame_index := _motion_frame(action_type, motion_data[1], action_started_ms, action_speed)
	if action_type in [7, 14]:
		var magic_name: String = actor_resource.magic_names.get(magic_id, "") if action_type == 7 else ""
		var primary_speed := 150 if magic_name == "十方斩" else 100
		var attack_step := _motion_step(action_started_ms, primary_speed)
		if attack_step >= motion_data[1]:
			var primary_ms := float(motion_data[1]) * 100.0 * 100.0 / float(clampi(primary_speed, 20, 500))
			var elapsed_ms := maxi(0, Time.get_ticks_msec() - action_started_ms)
			motion_data = PackedInt32Array([7, 3])
			frame_index = mini(floori((float(elapsed_ms) - primary_ms) / 100.0), 2)
		else:
			frame_index = attack_step
	var wear: Dictionary = desp.get("wear", {})
	var dress_shape := _wear_shape(wear, 1)
	var gfx_id: int = (dress_shape << 9) | (motion_data[0] << 3) | direction_index
	var body_key := (gender << 22) | ((gfx_id & 0x1FFFF) << 5) | frame_index
	var shadow: Dictionary = actor_resource.frame("hero", body_key | (1 << 23))
	var body: Dictionary = actor_resource.frame("hero", body_key)
	_record_actor_target(uid, 2, map_y, action_type, body, start_x, start_y)
	_draw_sprite_frame(shadow, start_x, start_y, 0.5)
	var weapon_shape := _wear_shape(wear, 3)
	var weapon_key := 0
	if weapon_shape > 0:
		var weapon_gfx: int = ((weapon_shape - 1) << 9) | (motion_data[0] << 3) | direction_index
		weapon_key = (gender << 22) | ((weapon_gfx & 0x1FFFF) << 5) | frame_index
		_draw_sprite_frame(actor_resource.frame("weapon", weapon_key | (1 << 23)), start_x, start_y, 0.5)
	_draw_sprite_frame(body, start_x, start_y, 1.0)
	var layer: Dictionary = actor_resource.frame("hero", body_key | (1 << 24))
	_draw_sprite_frame(layer, start_x, start_y, 1.0)
	var helmet_shape := _wear_shape(wear, 2)
	if helmet_shape > 0:
		var helmet_gfx: int = ((helmet_shape - 1) << 9) | (motion_data[0] << 3) | direction_index
		var helmet_key: int = (gender << 22) | ((helmet_gfx & 0x1FFFF) << 5) | frame_index
		_draw_sprite_frame(actor_resource.frame("helmet", helmet_key), start_x, start_y, 1.0)
	else:
		var hair: int = desp.get("hair", 0)
		if hair > 0:
			var hair_gfx: int = ((hair - 1) << 9) | (motion_data[0] << 3) | direction_index
			var hair_key: int = (gender << 22) | ((hair_gfx & 0x1FFFF) << 5) | frame_index
			_draw_sprite_frame(actor_resource.frame("hair", hair_key), start_x, start_y, 1.0)
	if weapon_key != 0:
		_draw_sprite_frame(actor_resource.frame("weapon", weapon_key), start_x, start_y, 1.0)
	if action_type == 7:
		_draw_attack_motion_effect(magic_id, direction, action_started_ms, start_x, start_y)
	return not body.is_empty()


func _draw_attack_motion_effect(magic_id: int, direction: int, started_ms: int, start_x: int, start_y: int) -> void:
	var effect := _attack_motion_effect_state(magic_id, direction, started_ms)
	if effect.is_empty() or not effect.get("visible", false):
		return
	_draw_magic_frame(effect.meta, effect.frame, effect.direction, Vector2(float(start_x) / GRID_XP, float(start_y) / GRID_YP), 0, 0)


func _attack_motion_effect_state(magic_id: int, direction: int, started_ms: int) -> Dictionary:
	var magic_name: String = actor_resource.magic_names.get(magic_id, "")
	if magic_name not in ["烈火剑法", "翔空剑法", "莲月剑法", "半月弯刀", "十方斩", "攻杀剑术", "刺杀剑术"]:
		return {}
	var meta: PackedInt32Array = actor_resource.magic_layout(magic_id, MAGIC_STAGE_RUN)
	if meta.is_empty() or meta[2] <= 0:
		return {}
	var motion_speed := 150 if magic_name == "十方斩" else 100
	var lag_frame := 3 if magic_name == "十方斩" else 0
	var elapsed := Time.get_ticks_msec() if started_ms <= 0 else maxi(0, Time.get_ticks_msec() - started_ms)
	var absolute_frame := roundi(float(elapsed) * motion_speed / 10000.0)
	var motion_frame_count := 10 if magic_name in ["翔空剑法", "莲月剑法", "十方斩"] else 6
	var effect_frame_count := mini(meta[2] + lag_frame, motion_frame_count)
	if absolute_frame >= effect_frame_count:
		return {}
	var gfx_frame := absolute_frame - lag_frame
	return {
		"meta": meta,
		"frame": gfx_frame,
		"direction": clampi(direction, 1, 8) - 1 if meta[6] > 1 else 0,
		"visible": gfx_frame >= 0,
	}


func _wear_shape(wear: Dictionary, location: int) -> int:
	var item: Dictionary = wear.get(location, {})
	return actor_resource.item_shape(item.get("itemID", 0))


func _hero_motion(action_type: int, magic_id := 0, desp: Dictionary = {}) -> PackedInt32Array:
	match action_type:
		3, 5: return PackedInt32Array([21, 6])
		7:
			var magic_name: String = actor_resource.magic_names.get(magic_id, "")
			if magic_name in ["翔空剑法", "莲月剑法"]:
				return PackedInt32Array([17, 10])
			if magic_name == "十方斩":
				return PackedInt32Array([16, 10])
			var double_handed := _hero_double_handed(desp)
			if magic_name == "半月弯刀":
				return PackedInt32Array([12 if double_handed else 11, 6])
			return PackedInt32Array([10 if double_handed else 9, 6])
		8: return PackedInt32Array([8, 2])
		9: return PackedInt32Array([2, 5])
		11: return PackedInt32Array([15, 3])
		12: return PackedInt32Array([18, 10])
		13: return PackedInt32Array([19, 10])
		14: return PackedInt32Array([10, 6])
		_: return PackedInt32Array([0, 4])


func _hero_double_handed(desp: Dictionary) -> bool:
	var wear: Dictionary = desp.get("wear", {})
	var weapon: Dictionary = wear.get(3, {})
	return bool(actor_resource.item_attribute(weapon.get("itemID", 0)).get("double_hand", false))


func _draw_monster_sprite(creature: Dictionary, start_x: int, start_y: int, alpha := 1.0) -> bool:
	var monster_id: int = creature.get("monster_id", 0)
	var look_id: int = creature.get("monster_stand_look", actor_resource.monster_look(monster_id))
	var direction_index := clampi(creature.get("direction", 5), 1, 8) - 1
	var motion_data := _monster_motion(creature.get("action_type", 2))
	var frame_index := _motion_frame(creature.get("action_type", 2), motion_data[1], creature.get("action_started_ms", 0), creature.get("action_speed", 100))
	var body_key: int = (look_id << 12) | (motion_data[0] << 8) | (direction_index << 5) | frame_index
	if actor_resource.monster_has_shadow(monster_id):
		_draw_sprite_frame(actor_resource.frame("monster", body_key | (1 << 23)), start_x, start_y, alpha * 0.5)
	var body: Dictionary = actor_resource.frame("monster", body_key)
	_record_actor_target(creature.get("uid", 0), 1, creature.get("y", 0), creature.get("action_type", 2), body, start_x, start_y)
	_draw_sprite_frame(body, start_x, start_y, alpha)
	_draw_focus_overlays(body, start_x, start_y, creature.get("uid", 0), alpha)
	return not body.is_empty()


func _record_actor_target(uid: int, creature_type: int, map_y: int, action_type: int, body: Dictionary, start_x: int, start_y: int) -> void:
	if uid == 0 or action_type == 13 or body.is_empty():
		return
	var texture: Texture2D = body.texture
	var offset: Vector2i = body.offset
	var texture_size := texture.get_size()
	var target_size := Vector2(minf(texture_size.x, 58.0), minf(texture_size.y, 40.0))
	var target_position := Vector2(start_x + offset.x, start_y + offset.y)
	target_position += (texture_size - target_size) * 0.5
	_actor_target_rects[uid] = {
		"rect": Rect2(target_position, target_size),
		"world_center": target_position + target_size * 0.5 + Vector2(int(game_state.view_x), int(game_state.view_y)),
		"type": creature_type,
		"map_y": map_y,
	}


func focus_uid_at_screen(screen_position: Vector2, allow_player := false) -> int:
	var best_uid := 0
	var best_y := -2147483648
	for uid_value in _actor_target_rects:
		var uid: int = uid_value
		if not allow_player and uid == game_state.player_uid:
			continue
		var target: Dictionary = _actor_target_rects[uid]
		if target.rect.has_point(screen_position) and int(target.map_y) >= best_y:
			best_uid = uid
			best_y = int(target.map_y)
	return best_uid


func focus_color(channel: int) -> Color:
	return FOCUS_COLORS[channel] if channel >= 0 and channel < FOCUS_COLORS.size() else Color.WHITE


func _draw_focus_overlays(body: Dictionary, start_x: int, start_y: int, uid: int, alpha: float) -> void:
	if body.is_empty() or uid == 0:
		return
	var channel_uids := [_mouse_focus_uid, _magic_focus_uid, _follow_focus_uid, _attack_focus_uid]
	var offset: Vector2i = body.offset
	for index in range(channel_uids.size()):
		if int(channel_uids[index]) != uid:
			continue
		var color := focus_color(index + 1)
		color.a = alpha
		draw_texture(body.texture, Vector2(start_x + offset.x, start_y + offset.y), color)


func _draw_npc_sprite(creature: Dictionary, start_x: int, start_y: int) -> bool:
	var npc_id: int = creature.get("npc_id", 0)
	# NPC resources store the first/second/third available view, not eight compass directions.
	var direction_index := clampi(creature.get("direction", 1), 1, 3) - 1
	var frame_index := floori(float(Time.get_ticks_msec()) / 200.0) % 4
	var body_key: int = (npc_id << 12) | (direction_index << 5) | frame_index
	_draw_sprite_frame(actor_resource.frame("npc", body_key | (1 << 23)), start_x, start_y, 0.5)
	var body: Dictionary = actor_resource.frame("npc", body_key)
	_record_actor_target(creature.get("uid", 0), 3, creature.get("y", 0), creature.get("action_type", 2), body, start_x, start_y)
	_draw_sprite_frame(body, start_x, start_y, 1.0)
	_draw_focus_overlays(body, start_x, start_y, creature.get("uid", 0), 1.0)
	return not body.is_empty()


func _draw_sprite_frame(sprite: Dictionary, start_x: int, start_y: int, alpha: float) -> void:
	if sprite.is_empty():
		return
	var offset: Vector2i = sprite.offset
	draw_texture(sprite.texture, Vector2(start_x + offset.x, start_y + offset.y), Color(1.0, 1.0, 1.0, alpha))


func _monster_motion(action_type: int) -> PackedInt32Array:
	match action_type:
		3, 5: return PackedInt32Array([1, 6])
		7: return PackedInt32Array([2, 6])
		11: return PackedInt32Array([3, 2])
		13: return PackedInt32Array([4, 10])
		9: return PackedInt32Array([6, 10])
		1: return PackedInt32Array([8, 10])
		_: return PackedInt32Array([0, 4])


func _motion_frame(action_type: int, frame_count: int, started_ms: int, speed: int) -> int:
	var frame := _motion_step(started_ms, speed)
	if action_type == 2:
		return frame % frame_count
	return mini(frame, frame_count - 1)


func _motion_step(started_ms: int, speed: int) -> int:
	var frame_delay := 100.0 * 100.0 / float(clampi(speed, 20, 500))
	var elapsed := Time.get_ticks_msec() if started_ms <= 0 else maxi(0, Time.get_ticks_msec() - started_ms)
	return floori(float(elapsed) / frame_delay)


func _action_draw_grid(end_x: int, end_y: int, from_x: int, from_y: int, action_type: int, started_ms: int, speed: int) -> Vector2:
	if action_type not in [3, 5] or started_ms <= 0:
		return Vector2(end_x, end_y)
	var duration_ms := 600.0 * 100.0 / float(clampi(speed, 20, 500))
	var ratio := clampf(float(Time.get_ticks_msec() - started_ms) / duration_ms, 0.0, 1.0)
	return Vector2(from_x, from_y).lerp(Vector2(end_x, end_y), ratio)


func grid_from_screen(screen_x: int, screen_y: int) -> Vector2i:
	if game_state == null:
		return Vector2i.ZERO
	var view_x: int = int(game_state.view_x)
	var view_y: int = int(game_state.view_y)
	return Vector2i((screen_x + view_x) / GRID_XP, (screen_y + view_y) / GRID_YP)
