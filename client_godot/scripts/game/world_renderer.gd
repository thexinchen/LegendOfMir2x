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
	for grid_key in game_state.ground_items:
		var parts: PackedStringArray = grid_key.split(",")
		var gx := int(parts[0])
		var gy := int(parts[1])
		var sx := gx * GRID_XP - view_x + GRID_XP / 2 - 8
		var sy := gy * GRID_YP - view_y + GRID_YP / 2 - 8
		draw_rect(Rect2(sx, sy, 16, 16), Color(1, 0.85, 0.3, 0.7))

	# Overground objects and actors are interleaved one map row at a time.
	var creatures_by_row: Dictionary = {}
	for uid in game_state.creatures:
		var creature: Dictionary = game_state.creatures[uid]
		var row: int = creature.get("y", 0)
		var row_creatures: Array = creatures_by_row.get(row, [])
		row_creatures.append(creature)
		creatures_by_row[row] = row_creatures
	var now := Time.get_ticks_msec()
	for gy in range(y0, y1 + 1):
		_draw_object_row(1, gy, x0, x1, view_x, view_y)
		_draw_strike_row(gy, x0, x1, view_x, view_y, now)
		for creature in creatures_by_row.get(gy, []):
			_draw_creature(creature, view_x, view_y)
		if game_state.player_y == gy:
			_draw_player(view_x, view_y)
		_draw_object_row(2, gy, x0, x1, view_x, view_y)
	_draw_object_depth(3, x0, y0, x1, y1, view_x, view_y)

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


func _draw_player(view_x: int, view_y: int) -> void:
	var px: int = game_state.player_x * GRID_XP - view_x
	var py: int = game_state.player_y * GRID_YP - view_y
	var center := Vector2(px + GRID_XP * 0.5, py + GRID_YP * 0.5)
	
	if not _draw_hero_sprite(game_state.player_gender, game_state.player_direction, px, py):
		draw_circle(Vector2(center.x + 2, center.y + 14), 12, Color(0, 0, 0, 0.3))
		draw_circle(center, 14, Color(0.3, 0.5, 0.9, 1.0))
	
	# The C++ client only enables actor HP/name overlays through debug/runtime flags.


func _draw_creature(c: Dictionary, view_x: int, view_y: int) -> void:
	var cx: int = int(c.get("x", 0)) * GRID_XP - view_x
	var cy: int = int(c.get("y", 0)) * GRID_YP - view_y
	var center := Vector2(cx + GRID_XP * 0.5, cy + GRID_YP * 0.5)
	
	# Cull if off-screen
	if cx < -GRID_XP or cx > SCREEN_W or cy < -GRID_YP or cy > SCREEN_H:
		return
	
	var c_type: int = c.get("type", 0)
	var sprite_drawn := false
	match c_type:
		1: sprite_drawn = _draw_monster_sprite(c, cx, cy)
		2: sprite_drawn = _draw_hero_sprite(c.get("gender", 0), c.get("direction", 5), cx, cy)
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


func _draw_hero_sprite(gender: int, direction: int, start_x: int, start_y: int) -> bool:
	var direction_index := clampi(direction, 1, 8) - 1
	var frame_index := floori(float(Time.get_ticks_msec()) / 150.0) % 4
	var body_key := (gender << 22) | (direction_index << 5) | frame_index
	var shadow: Dictionary = actor_resource.frame("hero", body_key | (1 << 23))
	var body: Dictionary = actor_resource.frame("hero", body_key)
	_draw_sprite_frame(shadow, start_x, start_y, 0.5)
	_draw_sprite_frame(body, start_x, start_y, 1.0)
	var layer: Dictionary = actor_resource.frame("hero", body_key | (1 << 24))
	_draw_sprite_frame(layer, start_x, start_y, 1.0)
	return not body.is_empty()


func _draw_monster_sprite(creature: Dictionary, start_x: int, start_y: int) -> bool:
	var monster_id: int = creature.get("monster_id", 0)
	var look_id: int = actor_resource.monster_look(monster_id)
	var direction_index := clampi(creature.get("direction", 5), 1, 8) - 1
	var motion_data := _monster_motion(creature.get("action_type", 2))
	var frame_index := floori(float(Time.get_ticks_msec()) / 150.0) % motion_data[1]
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
		3, 4, 5: return PackedInt32Array([1, 6])
		7: return PackedInt32Array([2, 6])
		11: return PackedInt32Array([3, 2])
		13: return PackedInt32Array([4, 10])
		9: return PackedInt32Array([6, 10])
		1: return PackedInt32Array([8, 10])
		_: return PackedInt32Array([0, 4])


func grid_from_screen(screen_x: int, screen_y: int) -> Vector2i:
	if game_state == null:
		return Vector2i.ZERO
	var view_x: int = int(game_state.view_x)
	var view_y: int = int(game_state.view_y)
	return Vector2i((screen_x + view_x) / GRID_XP, (screen_y + view_y) / GRID_YP)
