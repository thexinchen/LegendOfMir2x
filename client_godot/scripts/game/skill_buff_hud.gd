extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const BUFF_SIZE := 30.0

var game_state: Node
var _resources: RefCounted = ActorResourceScript.new()


func _ready() -> void:
	game_state = get_node("/root/GameState")
	_resources.configure_default()
	game_state.state_changed.connect(queue_redraw)
	queue_redraw()


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	_draw_magic_keys()
	_draw_buffs()


func _draw_magic_keys() -> void:
	if not game_state.magic_key_hud_visible:
		return
	var offset_x := 0.0
	var magic_ids: Array = game_state.magic_keys.keys()
	magic_ids.sort()
	for magic_value in magic_ids:
		var magic_id := int(magic_value)
		var layout: PackedInt32Array = _resources.skill_layout(magic_id)
		if layout.size() < 5:
			continue
		var icon: Dictionary = _resources.frame("proguse", layout[0] + 0x1000)
		if icon.is_empty():
			continue
		var texture := icon.texture as Texture2D
		var icon_size := texture.get_size()
		draw_texture(texture, Vector2(offset_x, 0.0))
		var angle := cooldown_angle(magic_id)
		if angle < 360.0:
			_draw_cooldown(Vector2(offset_x, 0.0), icon_size, angle)
		offset_x += icon_size.x


func _draw_buffs() -> void:
	var offset_x := size.x - BUFF_SIZE - _minimap_width()
	for index in range(game_state.buff_list.size() - 1, -1, -1):
		var value = game_state.buff_list[index]
		var buff_id := int(value if value is int else value.get("id", 0))
		var layout: PackedInt32Array = _resources.buff_layout(buff_id)
		if layout.size() != 2:
			continue
		var icon: Dictionary = _resources.frame("proguse", layout[0])
		if icon.is_empty():
			continue
		draw_texture_rect(icon.texture, Rect2(offset_x, 0.0, BUFF_SIZE, BUFF_SIZE), false)
		offset_x -= BUFF_SIZE


func cooldown_angle(magic_id: int) -> float:
	var layout: PackedInt32Array = _resources.skill_layout(magic_id)
	var cool_down := layout[5] if layout.size() >= 6 else 0
	if cool_down <= 0 or not game_state.magic_cast_times.has(magic_id):
		return 360.0
	var elapsed := Time.get_ticks_msec() - int(game_state.magic_cast_times[magic_id])
	return clampf(360.0 * float(elapsed) / float(cool_down), 0.0, 360.0)


func magic_icon_count() -> int:
	var count := 0
	for magic_value in game_state.magic_keys:
		var layout: PackedInt32Array = _resources.skill_layout(int(magic_value))
		if layout.size() >= 5 and not _resources.frame("proguse", layout[0] + 0x1000).is_empty():
			count += 1
	return count


func buff_icon_count() -> int:
	var count := 0
	for value in game_state.buff_list:
		var buff_id := int(value if value is int else value.get("id", 0))
		var layout: PackedInt32Array = _resources.buff_layout(buff_id)
		if layout.size() == 2 and not _resources.frame("proguse", layout[0]).is_empty():
			count += 1
	return count


func _draw_cooldown(position: Vector2, icon_size: Vector2, angle: float) -> void:
	var center := position + icon_size * 0.5
	var points := PackedVector2Array([center])
	var steps := maxi(2, ceili(angle / 12.0))
	for index in range(steps + 1):
		var radians := deg_to_rad(-90.0 + angle * float(index) / float(steps))
		var direction := Vector2(cos(radians), sin(radians))
		var reach_x := INF if absf(direction.x) < 0.0001 else icon_size.x * 0.5 / absf(direction.x)
		var reach_y := INF if absf(direction.y) < 0.0001 else icon_size.y * 0.5 / absf(direction.y)
		points.append(center + direction * minf(reach_x, reach_y))
	var ratio := pow(angle / 360.0, 4.0)
	var color := Color(1.0, 0.2, 0.2, 1.0).lerp(Color(0.0, 1.0, 0.0, 80.0 / 255.0), ratio)
	var colors := PackedColorArray([color])
	for _index in range(points.size() - 1):
		colors.append(Color(color.r, color.g, color.b, 0.0))
	draw_polygon(points, colors)


func _minimap_width() -> float:
	var parent := get_parent()
	return float(parent.call("minimap_hud_width")) if parent != null and parent.has_method("minimap_hud_width") else 0.0
