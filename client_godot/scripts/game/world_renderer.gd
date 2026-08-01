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
const MAGIC_TYPE_FIXED := 1
const MAGIC_TYPE_BOUND := 2
const MAGIC_TYPE_FOLLOW := 3

var game_state: Node = null

# Map data
var map_width: int = 0
var map_height: int = 0
var world_resource: RefCounted = WorldResourceScript.new()
var actor_resource: RefCounted = ActorResourceScript.new()


func _ready() -> void:
	pass


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


func _draw() -> void:
	if game_state == null:
		return
	
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

	# Ground items precede living actors in the original renderer.
	_draw_ground_items(x0, y0, x1, y1, view_x, view_y)

	# Overground objects and actors are interleaved one map row at a time.
	var creatures_by_row: Dictionary = {}
	for uid in game_state.creatures:
		var creature: Dictionary = game_state.creatures[uid]
		var row: int = creature.get("y", 0)
		var row_creatures: Array = creatures_by_row.get(row, [])
		row_creatures.append(creature)
		creatures_by_row[row] = row_creatures
	var now := Time.get_ticks_msec()
	var active_magic := _resolve_magic_effects(now)
	for gy in range(y0, y1 + 1):
		_draw_object_row(1, gy, x0, x1, view_x, view_y)
		_draw_firewall_row(gy, x0, x1, view_x, view_y, now)
		_draw_magic_row(active_magic, gy, true, view_x, view_y)
		_draw_strike_row(gy, x0, x1, view_x, view_y, now)
		for creature in creatures_by_row.get(gy, []):
			_draw_creature(creature, view_x, view_y)
		if game_state.player_y == gy:
			_draw_player(view_x, view_y)
		_draw_object_row(2, gy, x0, x1, view_x, view_y)
	_draw_object_depth(3, x0, y0, x1, y1, view_x, view_y)
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


func _resolve_magic_effect(effect: Dictionary, now: int) -> Dictionary:
	var magic_id: int = effect.get("magicID", 0)
	if magic_id <= 0:
		return {}
	var stages := [MAGIC_STAGE_RUN, MAGIC_STAGE_EXPLODE] if effect.get("source", "") == "cast" else [MAGIC_STAGE_SPELL, MAGIC_STAGE_RUN, MAGIC_STAGE_EXPLODE]
	var elapsed := maxi(0, now - int(effect.get("start_time", now)))
	for stage in stages:
		var meta: PackedInt32Array = actor_resource.magic_layout(magic_id, stage)
		if meta.is_empty():
			continue
		var duration := _magic_stage_duration(meta, effect)
		if elapsed < duration:
			return _make_resolved_magic(effect, meta, stage, elapsed, duration)
		elapsed -= duration
	return {}


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
		if magic.on_ground != on_ground:
			continue
		var position: Vector2 = magic.position
		if on_ground and floori(position.y) != y:
			continue
		_draw_magic_frame(magic.meta, magic.frame, magic.direction, position, view_x, view_y)


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


func _draw_magic_frame(meta: PackedInt32Array, frame: int, direction: int, grid_position: Vector2, view_x: int, view_y: int) -> void:
	var texture_id: int = meta[0] + direction * meta[3] + frame
	var sprite: Dictionary = actor_resource.frame("magic", texture_id)
	if sprite.is_empty():
		return
	var packed_color: int = meta[1]
	var color := Color(
		float(packed_color & 0xFF) / 255.0,
		float((packed_color >> 8) & 0xFF) / 255.0,
		float((packed_color >> 16) & 0xFF) / 255.0,
		float((packed_color >> 24) & 0xFF) / 255.0,
	)
	var offset: Vector2i = sprite.offset
	draw_texture(sprite.texture, Vector2(grid_position.x * GRID_XP - view_x + offset.x, grid_position.y * GRID_YP - view_y + offset.y), color)


func _draw_player(view_x: int, view_y: int) -> void:
	var draw_grid := _action_draw_grid(game_state.player_x, game_state.player_y, game_state.player_action_from_x, game_state.player_action_from_y, game_state.player_action_type, game_state.player_action_started_ms, game_state.player_action_speed)
	var px: int = roundi(draw_grid.x * GRID_XP) - view_x
	var py: int = roundi(draw_grid.y * GRID_YP) - view_y
	var center := Vector2(px + GRID_XP * 0.5, py + GRID_YP * 0.5)
	
	if not _draw_hero_sprite(game_state.player_gender, game_state.player_direction, game_state.player_action_type, game_state.player_desp, px, py, game_state.player_action_started_ms, game_state.player_action_speed, game_state.player_action_magic_id):
		draw_circle(Vector2(center.x + 2, center.y + 14), 12, Color(0, 0, 0, 0.3))
		draw_circle(center, 14, Color(0.3, 0.5, 0.9, 1.0))
	
	# The C++ client only enables actor HP/name overlays through debug/runtime flags.


func _draw_creature(c: Dictionary, view_x: int, view_y: int) -> void:
	var draw_grid := _action_draw_grid(c.get("x", 0), c.get("y", 0), c.get("action_from_x", c.get("x", 0)), c.get("action_from_y", c.get("y", 0)), c.get("action_type", 2), c.get("action_started_ms", 0), c.get("action_speed", 100))
	var cx: int = roundi(draw_grid.x * GRID_XP) - view_x
	var cy: int = roundi(draw_grid.y * GRID_YP) - view_y
	var center := Vector2(cx + GRID_XP * 0.5, cy + GRID_YP * 0.5)
	
	# Cull if off-screen
	if cx < -GRID_XP or cx > SCREEN_W or cy < -GRID_YP or cy > SCREEN_H:
		return
	
	var c_type: int = c.get("type", 0)
	var sprite_drawn := false
	match c_type:
		1: sprite_drawn = _draw_monster_sprite(c, cx, cy)
		2: sprite_drawn = _draw_hero_sprite(c.get("gender", 0), c.get("direction", 5), c.get("action_type", 2), c.get("desp", {}), cx, cy, c.get("action_started_ms", 0), c.get("action_speed", 100), c.get("action_magic_id", 0))
		3: sprite_drawn = _draw_npc_sprite(c, cx, cy)
	if not sprite_drawn:
		draw_circle(Vector2(center.x + 2, center.y + 14), 10, Color(0, 0, 0, 0.3))
		draw_circle(center, 12, Color(0.7, 0.2, 0.2, 0.9))
	
	# Name
	var font := get_theme_default_font()
	if font:
		var c_name: String = c.get("name", "")
		if not c_name.is_empty():
			var tw := font.get_string_size(c_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 11)
			font.draw_string(get_canvas_item(), Vector2(center.x - tw.x * 0.5, cy - 4), c_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 11, Color(1, 1, 1, 0.9))


func _draw_hero_sprite(gender: int, direction: int, action_type: int, desp: Dictionary, start_x: int, start_y: int, action_started_ms := 0, action_speed := 100, magic_id := 0) -> bool:
	var direction_index := clampi(direction, 1, 8) - 1
	var motion_data := _hero_motion(action_type, magic_id, desp)
	var frame_index := _motion_frame(action_type, motion_data[1], action_started_ms, action_speed)
	if action_type in [7, 14]:
		var magic_name: String = actor_resource.magic_names.get(magic_id, "") if action_type == 7 else ""
		var primary_speed := 150 if magic_name == "十方斩" else action_speed
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
	return not body.is_empty()


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


func _draw_monster_sprite(creature: Dictionary, start_x: int, start_y: int) -> bool:
	var monster_id: int = creature.get("monster_id", 0)
	var look_id: int = actor_resource.monster_look(monster_id)
	var direction_index := clampi(creature.get("direction", 5), 1, 8) - 1
	var motion_data := _monster_motion(creature.get("action_type", 2))
	var frame_index := _motion_frame(creature.get("action_type", 2), motion_data[1], creature.get("action_started_ms", 0), creature.get("action_speed", 100))
	var body_key: int = (look_id << 12) | (motion_data[0] << 8) | (direction_index << 5) | frame_index
	if actor_resource.monster_has_shadow(monster_id):
		_draw_sprite_frame(actor_resource.frame("monster", body_key | (1 << 23)), start_x, start_y, 0.5)
	var body: Dictionary = actor_resource.frame("monster", body_key)
	_draw_sprite_frame(body, start_x, start_y, 1.0)
	return not body.is_empty()


func _draw_npc_sprite(creature: Dictionary, start_x: int, start_y: int) -> bool:
	var npc_id: int = creature.get("npc_id", 0)
	# NPC resources store the first/second/third available view, not eight compass directions.
	var direction_index := clampi(creature.get("direction", 1), 1, 3) - 1
	var frame_index := floori(float(Time.get_ticks_msec()) / 200.0) % 4
	var body_key: int = (npc_id << 12) | (direction_index << 5) | frame_index
	_draw_sprite_frame(actor_resource.frame("npc", body_key | (1 << 23)), start_x, start_y, 0.5)
	var body: Dictionary = actor_resource.frame("npc", body_key)
	_draw_sprite_frame(body, start_x, start_y, 1.0)
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
