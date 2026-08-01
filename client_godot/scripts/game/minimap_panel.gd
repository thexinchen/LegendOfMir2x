extends "res://scripts/game/closable_panel.gd"

const WorldResourceScript = preload("res://scripts/game/world_resource.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")

var _state: Node
var _world: RefCounted = WorldResourceScript.new()
var _actors: RefCounted = ActorResourceScript.new()
var _extended := false
var _alpha_on := false
var _auto_center := true
var _show_creatures := true
var _dragging_map := false
var _zoom := 1.0
var _image_offset := Vector2.ZERO
var _last_map_id := -1
var _hover_position := Vector2(-1, -1)
var _last_marker_signature := 0
var _last_marker_offset := Vector2(INF, INF)
var _last_marker_zoom := -1.0


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$AlphaButton.pressed.connect(_toggle_alpha)
	$ExtendButton.pressed.connect(_toggle_size)
	$CenterButton.pressed.connect(_toggle_center)
	$ConfigButton.pressed.connect(_toggle_creatures)
	$MapViewport.gui_input.connect(_on_map_input)
	position = Vector2(get_viewport_rect().size.x - size.x, 0)
	_refresh()


func _process(_delta: float) -> void:
	if _auto_center:
		_center_on_player()
	var signature := _marker_signature()
	if signature != _last_marker_signature or _image_offset != _last_marker_offset or not is_equal_approx(_zoom, _last_marker_zoom):
		_update_markers()
	_update_tooltip()


func _refresh() -> void:
	if _state.player_map_id != _last_map_id:
		_load_map(_state.player_map_id)
	if _auto_center:
		_center_on_player()
	_update_markers()


func _load_map(map_id: int) -> void:
	_last_map_id = map_id
	$MapViewport/MapTexture.texture = null
	if map_id <= 0 or not _world.load_map(map_id):
		return
	_actors.configure(_world.base_path)
	if _world.minimap_id < 0 or _world.minimap_id == 0xFFFFFFFF:
		return
	var frame: Dictionary = _actors.frame("proguse", _world.minimap_id)
	$MapViewport/MapTexture.texture = frame.get("texture")
	_zoom = 1.0
	_auto_center = true
	_center_on_player()


func _image_size() -> Vector2:
	var texture := $MapViewport/MapTexture.texture as Texture2D
	return Vector2.ZERO if texture == null else Vector2(texture.get_size()) * _zoom


func _center_on_player() -> void:
	var image_size := _image_size()
	if image_size == Vector2.ZERO or _world.width <= 0 or _world.height <= 0:
		return
	var canvas_size: Vector2 = $MapViewport.size
	_image_offset = Vector2(
		canvas_size.x * 0.5 - float(_state.player_x) * image_size.x / _world.width,
		canvas_size.y * 0.5 - float(_state.player_y) * image_size.y / _world.height,
	)
	_fix_image_offset()
	_apply_image_rect()


func _fix_image_offset() -> void:
	var image_size := _image_size()
	var canvas_size: Vector2 = $MapViewport.size
	_image_offset.x = (canvas_size.x - image_size.x) * 0.5 if image_size.x <= canvas_size.x else clampf(_image_offset.x, canvas_size.x - image_size.x, 0.0)
	_image_offset.y = (canvas_size.y - image_size.y) * 0.5 if image_size.y <= canvas_size.y else clampf(_image_offset.y, canvas_size.y - image_size.y, 0.0)


func _apply_image_rect() -> void:
	$MapViewport/MapTexture.position = _image_offset
	$MapViewport/MapTexture.size = _image_size()
	$MapViewport/MapTexture.modulate.a = 0.5 if _alpha_on else 1.0


func _map_to_canvas(x: int, y: int) -> Vector2:
	var image_size := _image_size()
	if _world.width <= 0 or _world.height <= 0:
		return Vector2(-1, -1)
	return _image_offset + Vector2(float(x) * image_size.x / _world.width, float(y) * image_size.y / _world.height)


func _canvas_to_map(point: Vector2) -> Vector2i:
	var image_size := _image_size()
	if image_size.x <= 0.0 or image_size.y <= 0.0:
		return Vector2i(-1, -1)
	return Vector2i(
		roundi((point.x - _image_offset.x) * _world.width / image_size.x),
		roundi((point.y - _image_offset.y) * _world.height / image_size.y),
	)


func _update_markers() -> void:
	for child in $MapViewport/Markers.get_children():
		child.queue_free()
	if $MapViewport/MapTexture.texture == null:
		_remember_marker_state()
		return
	_add_marker(_state.player_x, _state.player_y, Color(1, 0, 1), 6.0)
	if not _show_creatures:
		_remember_marker_state()
		return
	for uid in _state.creatures:
		var creature: Dictionary = _state.creatures[uid]
		var type: int = creature.get("type", 0)
		var color := Color.RED
		var diameter := 2.0
		if type == 2:
			color = Color(0.78, 0, 0.78)
			diameter = 4.0
		elif type == 3:
			color = Color.BLUE
			diameter = 4.0
		_add_marker(creature.get("x", 0), creature.get("y", 0), color, diameter)
	_remember_marker_state()


func _marker_signature() -> int:
	var result := hash(Vector3i(_state.player_x, _state.player_y, int(_show_creatures)))
	if _show_creatures:
		for uid in _state.creatures:
			var creature: Dictionary = _state.creatures[uid]
			result = hash(PackedInt64Array([result, uid, creature.get("x", 0), creature.get("y", 0), creature.get("type", 0)]))
	return result


func _remember_marker_state() -> void:
	_last_marker_signature = _marker_signature()
	_last_marker_offset = _image_offset
	_last_marker_zoom = _zoom


func _add_marker(x: int, y: int, color: Color, diameter: float) -> void:
	var point := _map_to_canvas(x, y)
	if not Rect2(Vector2.ZERO, $MapViewport.size).has_point(point):
		return
	var marker := ColorRect.new()
	marker.color = color
	marker.mouse_filter = Control.MOUSE_FILTER_IGNORE
	marker.position = point - Vector2.ONE * diameter * 0.5
	marker.size = Vector2.ONE * diameter
	$MapViewport/Markers.add_child(marker)


func _on_map_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			_dragging_map = event.pressed
			if event.pressed and _auto_center:
				_auto_center = false
				_update_button_textures()
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			_zoom_at(event.position, _zoom * 1.1)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			_zoom_at(event.position, _zoom / 1.1)
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			var location := _canvas_to_map(event.position)
			if _world.can_walk(location.x, location.y):
				NetworkClient.send_request_space_move(_state.player_map_uid, location.x, location.y)
		accept_event()
	elif event is InputEventMouseMotion:
		_hover_position = event.position
		if _dragging_map:
			_image_offset += event.relative
			_fix_image_offset()
			_apply_image_rect()
		accept_event()


func _zoom_at(canvas_position: Vector2, next_zoom: float) -> void:
	var old_size := _image_size()
	if old_size == Vector2.ZERO:
		return
	var ratio := (canvas_position - _image_offset) / old_size
	_zoom = clampf(next_zoom, 0.1, 10.0)
	var new_size := _image_size()
	_image_offset = canvas_position - ratio * new_size
	_auto_center = false
	_fix_image_offset()
	_apply_image_rect()
	_update_button_textures()


func _update_tooltip() -> void:
	var tooltip := $Coordinate
	if not Rect2(Vector2.ZERO, $MapViewport.size).has_point(_hover_position) or $MapViewport/MapTexture.texture == null:
		tooltip.hide()
		return
	var location := _canvas_to_map(_hover_position)
	tooltip.text = "[%d,%d]  %d%%" % [location.x, location.y, roundi(_zoom * 100.0)]
	tooltip.add_theme_color_override("font_color", Color.YELLOW)
	tooltip.add_theme_color_override("font_outline_color", Color.RED if not _world.can_walk(location.x, location.y) else Color.BLACK)
	tooltip.position = Vector2(clampf(_hover_position.x - 90.0, 0.0, size.x - 90.0), clampf(_hover_position.y - 22.0, 0.0, size.y - 20.0))
	tooltip.show()


func _toggle_alpha() -> void:
	_alpha_on = not _alpha_on
	_apply_image_rect()
	_update_button_textures()


func _toggle_size() -> void:
	_extended = not _extended
	var viewport_size := get_viewport_rect().size
	size = Vector2(roundf(viewport_size.x * 0.8), roundf(viewport_size.y * 0.5)) if _extended else Vector2(200, 200)
	position = (viewport_size - size) * 0.5 if _extended else Vector2(viewport_size.x - size.x, 0)
	_auto_center = true
	_center_on_player()
	_update_button_textures()


func _toggle_center() -> void:
	_auto_center = not _auto_center
	if _auto_center:
		_center_on_player()
	_update_button_textures()


func _toggle_creatures() -> void:
	_show_creatures = not _show_creatures
	_update_markers()
	_update_button_textures()


func _update_button_textures() -> void:
	$AlphaButton.texture_normal = load("res://assets/ui/game/minimap/09000011.png" if _alpha_on else "res://assets/ui/game/minimap/09000010.png")
	$ExtendButton.texture_normal = load("res://assets/ui/game/minimap/09000021.png" if _extended else "res://assets/ui/game/minimap/09000020.png")
	$CenterButton.texture_normal = load("res://assets/ui/game/minimap/09000031.png" if _auto_center else "res://assets/ui/game/minimap/09000030.png")
	$ConfigButton.texture_normal = load("res://assets/ui/game/minimap/09000041.png" if _show_creatures else "res://assets/ui/game/minimap/09000040.png")
