extends "res://scripts/game/closable_panel.gd"

var _state: Node
var _extended := false


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh)
	$AlphaButton.pressed.connect(func(): modulate.a = 0.55 if modulate.a > 0.8 else 1.0)
	$ExtendButton.pressed.connect(_toggle_size)
	$CenterButton.pressed.connect(_center)
	_refresh()


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	var rect: Rect2 = $MapViewport.get_rect()
	var center: Vector2 = rect.position + rect.size * 0.5
	for uid in _state.creatures:
		var creature: Dictionary = _state.creatures[uid]
		var point: Vector2 = center + Vector2(creature.get("x", 0) - _state.player_x, creature.get("y", 0) - _state.player_y) * 2.0
		if rect.has_point(point):
			draw_circle(point, 2.0, Color.RED if creature.get("type", 0) == 1 else Color.YELLOW)
	draw_circle(center, 3.0, Color.LIME_GREEN)


func _refresh() -> void:
	$Caption.text = "%s  %d, %d" % [_state.player_map_name, _state.player_x, _state.player_y]


func _toggle_size() -> void:
	_extended = not _extended
	scale = Vector2(1.5, 1.5) if _extended else Vector2.ONE


func _center() -> void:
	position = (get_viewport_rect().size - size) * 0.5
