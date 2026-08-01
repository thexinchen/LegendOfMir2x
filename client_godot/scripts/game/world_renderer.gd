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

var game_state: Node = null

# Map data
var map_width: int = 0
var map_height: int = 0
var map_tiles: Dictionary = {}  # "x,y" -> texture_id
var map_objects: Dictionary = {}  # "x,y" -> list of {tex_id, depth, animated, alpha}

# Placeholder tile colors (checkerboard pattern)
var _tile_colors: Array = [
	Color(0.25, 0.30, 0.20, 1.0),
	Color(0.30, 0.35, 0.25, 1.0),
]


func _ready() -> void:
	pass


func set_map_data(w: int, h: int) -> void:
	map_width = w
	map_height = h
	_generate_placeholder_tiles()
	queue_redraw()


func _generate_placeholder_tiles() -> void:
	map_tiles.clear()
	map_objects.clear()
	for y in range(map_height):
		for x in range(map_width):
			if x % 2 == 0 and y % 2 == 0:
				var color_idx := ((x / 2) + (y / 2)) % 2
				map_tiles["%d,%d" % [x, y]] = color_idx


func _draw() -> void:
	if game_state == null:
		return
	
	var view_x: int = int(game_state.view_x)
	var view_y: int = int(game_state.view_y)
	
	# Compute visible grid range (same as C++ draw())
	var x0 := maxi(0, (view_x / GRID_XP) - OBJMAXW)
	var y0 := maxi(0, (view_y / GRID_YP) - OBJMAXH)
	var x1 := ((view_x + SCREEN_W) / GRID_XP) + OBJMAXW
	var y1 := ((view_y + SCREEN_H) / GRID_YP) + OBJMAXH
	if map_width > 0:
		x1 = mini(x1, map_width - 1)
	if map_height > 0:
		y1 = mini(y1, map_height - 1)
	
	# 1. Draw tiles (every 2x2 grid, same as C++)
	for gy in range(y0, y1 + 1):
		for gx in range(x0, x1 + 1):
			if gy % 2 != 0 or gx % 2 != 0:
				continue
			var sx := gx * GRID_XP - view_x
			var sy := gy * GRID_YP - view_y
			var color_idx: int = map_tiles.get("%d,%d" % [gx, gy], 0)
			var color: Color = _tile_colors[color_idx % _tile_colors.size()]
			draw_rect(Rect2(sx, sy, GRID_XP, GRID_YP), color)
	
	# 2. Draw creatures sorted by Y (row order, same as C++)
	var creatures_to_draw: Array = []
	for uid in game_state.creatures:
		var c: Dictionary = game_state.creatures[uid]
		creatures_to_draw.append(c)
	# Sort by Y
	creatures_to_draw.sort_custom(func(a, b): return a.get("y", 0) < b.get("y", 0))
	
	for c in creatures_to_draw:
		_draw_creature(c, view_x, view_y)
	
	# 3. Draw player (always on top of same-row creatures)
	_draw_player(view_x, view_y)
	
	# 4. Draw ground items
	for grid_key in game_state.ground_items:
		var parts := grid_key.split(",")
		var gx := int(parts[0])
		var gy := int(parts[1])
		var sx := gx * GRID_XP - view_x + GRID_XP / 2 - 8
		var sy := gy * GRID_YP - view_y + GRID_YP / 2 - 8
		draw_rect(Rect2(sx, sy, 16, 16), Color(1, 0.85, 0.3, 0.7))


func _draw_player(view_x: int, view_y: int) -> void:
	var px: int = game_state.player_x * GRID_XP - view_x
	var py: int = game_state.player_y * GRID_YP - view_y
	var center := Vector2(px + GRID_XP * 0.5, py + GRID_YP * 0.5)
	
	# Shadow
	draw_circle(Vector2(center.x + 2, center.y + 14), 12, Color(0, 0, 0, 0.3))
	
	# Body - color by job
	var body_color: Color
	match game_state.player_job:
		1: body_color = Color(0.2, 0.7, 0.3, 1.0)  # warrior - green
		2: body_color = Color(0.5, 0.3, 0.8, 1.0)  # taoist - purple
		4: body_color = Color(0.3, 0.5, 0.9, 1.0)  # wizard - blue
		_: body_color = Color(0.5, 0.5, 0.5, 1.0)
	
	draw_circle(center, 14, body_color)
	draw_arc(center, 14, 0, TAU, 32, Color(1, 1, 1, 0.8), 1.5)
	
	# Direction indicator
	var dir: int = game_state.player_direction
	var dir_angle := 0.0
	match dir:
		1: dir_angle = -PI / 2    # up
		2: dir_angle = -PI / 4    # up-right
		3: dir_angle = 0           # right
		4: dir_angle = PI / 4      # down-right
		5: dir_angle = PI / 2      # down
		6: dir_angle = 3 * PI / 4  # down-left
		7: dir_angle = PI          # left
		8: dir_angle = -3 * PI / 4 # up-left
	var dir_x := center.x + cos(dir_angle) * 18
	var dir_y := center.y + sin(dir_angle) * 18
	draw_line(center, Vector2(dir_x, dir_y), Color(1, 1, 0, 0.9), 2.0)
	
	# Name
	var font := get_theme_default_font()
	if font:
		var name: String = game_state.player_name
		if not name.is_empty():
			var tw := font.get_string_size(name, HORIZONTAL_ALIGNMENT_CENTER, -1, 12)
			font.draw_string(get_canvas_item(), Vector2(center.x - tw.x * 0.5, py - 4), name, HORIZONTAL_ALIGNMENT_CENTER, -1, 12, Color(1, 1, 0, 1))
	
	# HP bar above player
	if game_state.player_hp_max > 0:
		var hp_ratio := float(game_state.player_hp) / float(game_state.player_hp_max)
		var bar_w := 30.0
		var bar_x := center.x - bar_w * 0.5
		var bar_y := py - 18.0
		draw_rect(Rect2(bar_x, bar_y, bar_w, 4), Color(0, 0, 0, 0.7))
		draw_rect(Rect2(bar_x, bar_y, bar_w * hp_ratio, 4), Color(0.8, 0.2, 0.2, 1.0))


func _draw_creature(c: Dictionary, view_x: int, view_y: int) -> void:
	var cx: int = int(c.get("x", 0)) * GRID_XP - view_x
	var cy: int = int(c.get("y", 0)) * GRID_YP - view_y
	var center := Vector2(cx + GRID_XP * 0.5, cy + GRID_YP * 0.5)
	
	# Cull if off-screen
	if cx < -GRID_XP or cx > SCREEN_W or cy < -GRID_YP or cy > SCREEN_H:
		return
	
	# Shadow
	draw_circle(Vector2(center.x + 2, center.y + 14), 10, Color(0, 0, 0, 0.3))
	
	# Body color by type
	var c_type: int = c.get("type", 0)
	var body_color: Color
	match c_type:
		1: body_color = Color(0.8, 0.2, 0.2, 0.9)  # monster - red
		2: body_color = Color(0.3, 0.5, 0.9, 0.9)  # player - blue
		3: body_color = Color(0.8, 0.6, 0.2, 0.9)  # NPC - yellow
		_: body_color = Color(0.5, 0.5, 0.5, 0.9)
	
	draw_circle(center, 12, body_color)
	draw_arc(center, 12, 0, TAU, 24, Color(1, 1, 1, 0.6), 1.0)
	
	# Direction indicator
	var dir: int = c.get("direction", 0)
	if dir > 0:
		var dir_angle := 0.0
		match dir:
			1: dir_angle = -PI / 2    # up
			2: dir_angle = -PI / 4    # up-right
			3: dir_angle = 0           # right
			4: dir_angle = PI / 4      # down-right
			5: dir_angle = PI / 2      # down
			6: dir_angle = 3 * PI / 4  # down-left
			7: dir_angle = PI          # left
			8: dir_angle = -3 * PI / 4 # up-left
		var dir_x := center.x + cos(dir_angle) * 16
		var dir_y := center.y + sin(dir_angle) * 16
		draw_line(center, Vector2(dir_x, dir_y), Color(1, 1, 0.5, 0.7), 1.5)
	
	# Attack indicator: if action_type is ACTION_ATTACK(7), draw red flash
	var action_type: int = c.get("action_type", 0)
	if action_type == 7:  # ACTION_ATTACK
		draw_arc(center, 16, 0, TAU, 24, Color(1, 0.3, 0.3, 0.8), 2.0)
	
	# Name
	var font := get_theme_default_font()
	if font:
		var c_name: String = c.get("name", "")
		if not c_name.is_empty():
			var tw := font.get_string_size(c_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 11)
			font.draw_string(get_canvas_item(), Vector2(center.x - tw.x * 0.5, cy - 4), c_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 11, Color(1, 1, 1, 0.9))


func grid_from_screen(screen_x: int, screen_y: int) -> Vector2i:
	if game_state == null:
		return Vector2i.ZERO
	var view_x: int = int(game_state.view_x)
	var view_y: int = int(game_state.view_y)
	return Vector2i((screen_x + view_x) / GRID_XP, (screen_y + view_y) / GRID_YP)
