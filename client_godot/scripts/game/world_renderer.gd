extends Control

# World renderer for the game scene
# Corresponds to ProcessRun::draw() in the C++ client
# Draws: map tiles, ground objects, creatures, ground items, magic effects, HUD

const GRID_XP := 48
const GRID_YP := 32
const SCREEN_W := 800
const SCREEN_H := 600
const OBJMAXW := 3
const OBJMAXH := 25

var game_state: Node = null
var protocol: RefCounted = null

# Map data
var map_width: int = 0
var map_height: int = 0
var map_tiles: Dictionary = {}  # "x,y" -> texture_id
var map_objects: Dictionary = {}  # "x,y" -> list of {tex_id, depth, animated, alpha}

# Texture cache
var _texture_cache: Dictionary = {}

# Placeholder textures for map tiles
var _placeholder_tile: Texture2D = null


func _ready() -> void:
	# Create a placeholder tile texture
	var img := Image.create(GRID_XP, GRID_YP, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.3, 0.35, 0.25, 1.0))
	_placeholder_tile = ImageTexture.create_from_image(img)


func set_map_data(w: int, h: int, tiles: Dictionary, objects: Dictionary) -> void:
	map_width = w
	map_height = h
	map_tiles = tiles
	map_objects = objects
	queue_redraw()


func _draw() -> void:
	if game_state == null:
		return
	# Compute visible grid range
	var view_x := int(game_state.view_x)
	var view_y := int(game_state.view_y)
	var x0 := maxi(0, view_x / GRID_XP - OBJMAXW)
	var y0 := maxi(0, view_y / GRID_YP - OBJMAXH)
	var x1 := (view_x + SCREEN_W) / GRID_XP + 1
	var y1 := (view_y + SCREEN_H) / GRID_YP + 1
	if map_width > 0:
		x1 = mini(x1, map_width - 1)
	if map_height > 0:
		y1 = mini(y1, map_height - 1)

	# 1. Draw tiles
	for gy in range(y0, y1 + 1):
		for gx in range(x0, x1 + 1):
			var screen_x := gx * GRID_XP - view_x
			var screen_y := gy * GRID_YP - view_y
			var tex_id := map_tiles.get("%d,%d" % [gx, gy], 0)
			if tex_id > 0:
				var tex := _get_texture(tex_id)
				if tex:
					draw_texture(tex, Vector2(screen_x, screen_y))
				else:
					draw_texture(_placeholder_tile, Vector2(screen_x, screen_y))
			else:
				draw_texture(_placeholder_tile, Vector2(screen_x, screen_y))

	# 2. Draw ground objects (depth 0)
	_draw_objects(x0, y0, x1, y1, view_x, view_y, 0)

	# 3. Draw creatures
	_draw_creatures(view_x, view_y)

	# 4. Draw ground items
	_draw_ground_items(view_x, view_y)

	# 5. Draw over-ground objects (depth 1, 2)
	_draw_objects(x0, y0, x1, y1, view_x, view_y, 1)
	_draw_objects(x0, y0, x1, y1, view_x, view_y, 2)

	# 6. Draw ascend strings (floating damage/heal text)
	_draw_ascend_strings(view_x, view_y)


func _draw_objects(x0: int, y0: int, x1: int, y1: int, view_x: int, view_y: int, depth: int) -> void:
	for gy in range(y0, y1 + 1):
		for gx in range(x0, x1 + 1):
			var key := "%d,%d" % [gx, gy]
			var objs := map_objects.get(key, [])
			for obj in objs:
				if obj.get("depth", 0) != depth:
					continue
				var screen_x := gx * GRID_XP - view_x
				var screen_y := (gy + 1) * GRID_YP - view_y
				var tex := _get_texture(obj.get("tex_id", 0))
				if tex:
					var h := tex.get_height()
					draw_texture(tex, Vector2(screen_x, screen_y - h))


func _draw_creatures(view_x: int, view_y: int) -> void:
	if game_state == null:
		return
	# Draw player
	var px := game_state.player_x * GRID_XP - view_x
	var py := game_state.player_y * GRID_YP - view_y
	# Draw player as a colored circle placeholder
	var center := Vector2(px + GRID_XP * 0.5, py + GRID_YP * 0.5)
	draw_arc(center, 16, 0, TAU, 32, Color(0.2, 0.8, 0.3, 1.0), 2.0)
	draw_circle(center, 14, Color(0.2, 0.8, 0.3, 0.5))

	# Draw player name
	var font := get_theme_default_font()
	if font:
		font.draw_string(get_canvas_item(), Vector2(px + GRID_XP * 0.5 - font.get_string_size(game_state.player_name).x * 0.5, py - 4), game_state.player_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 12, Color(1, 1, 0, 1))

	# Draw other creatures
	for uid in game_state.creatures:
		var c := game_state.creatures[uid]
		var cx: int = int(c.get("x", 0)) * GRID_XP - view_x
		var cy: int = int(c.get("y", 0)) * GRID_YP - view_y
		var c_center := Vector2(cx + GRID_XP * 0.5, cy + GRID_YP * 0.5)
		var c_color: Color
		var c_type: int = c.get("type", 0)
		match c_type:
			1: c_color = Color(0.8, 0.2, 0.2, 0.5)  # monster - red
			2: c_color = Color(0.2, 0.6, 0.8, 0.5)  # player - blue
			3: c_color = Color(0.8, 0.6, 0.2, 0.5)  # NPC - yellow
			_: c_color = Color(0.5, 0.5, 0.5, 0.5)
		draw_circle(c_center, 14, c_color)
		draw_arc(c_center, 16, 0, TAU, 32, c_color, 1.0)
		# Draw name
		var c_name: String = c.get("name", "")
		if c_name.length() > 0 and font:
			font.draw_string(get_canvas_item(), Vector2(cx + GRID_XP * 0.5 - font.get_string_size(c_name).x * 0.5, cy - 4), c_name, HORIZONTAL_ALIGNMENT_CENTER, -1, 12, Color(1, 1, 1, 1))


func _draw_ground_items(view_x: int, view_y: int) -> void:
	if game_state == null:
		return
	for grid_key in game_state.ground_items:
		var parts := grid_key.split(",")
		var gx := int(parts[0])
		var gy := int(parts[1])
		var sx := gx * GRID_XP - view_x
		var sy := gy * GRID_YP - view_y
		# Draw item placeholder
		draw_rect(Rect2(sx + 16, sy + 8, 16, 16), Color(1, 0.85, 0.3, 0.7))


func _draw_ascend_strings(view_x: int, view_y: int) -> void:
	# Placeholder for floating damage/heal text
	pass


func _get_texture(tex_id: int) -> Texture2D:
	if tex_id == 0:
		return null
	if _texture_cache.has(tex_id):
		return _texture_cache[tex_id]
	# Try to load from assets
	var hex_str := "%08x" % tex_id
	var path := "res://assets/ui/game/control_panel/%s.png" % hex_str
	if ResourceLoader.exists(path):
		var tex := load(path) as Texture2D
		_texture_cache[tex_id] = tex
		return tex
	return null


func grid_from_screen(screen_x: int, screen_y: int) -> Vector2i:
	if game_state == null:
		return Vector2i.ZERO
	var view_x := int(game_state.view_x)
	var view_y := int(game_state.view_y)
	return Vector2i((screen_x + view_x) / GRID_XP, (screen_y + view_y) / GRID_YP)
