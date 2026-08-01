extends Control

# Game main scene - corresponds to ProcessRun in C++ client
# Manages: world rendering, HUD, input, network message handling

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const WorldPathfinderScript = preload("res://scripts/game/world_pathfinder.gd")
const MINIMAP_PANEL_PATH := "res://scenes/game/panels/minimap.tscn"

@onready var world_renderer: Control = $WorldRenderer
@onready var inventory_panel: Control = %InventoryPanel
@onready var player_state_panel: Control = %PlayerStatePanel
@onready var skill_panel: Control = %SkillPanel
@onready var quick_bar: Control = %QuickBar
@onready var location_label: Label = $Location
@onready var control_panel: Control = $ControlPanel
@onready var grabbed_item_icon: TextureRect = $GrabbedItemIcon
@onready var skill_buff_hud: Control = $SkillBuffHUD

var game_state: Node = null
var protocol: RefCounted = null

var _ping_timer: float = 0.0
var _ping_tick: int = 0

const EXTRA_PANELS := {
	KEY_H: "res://scenes/game/panels/horse.tscn",
	KEY_G: "res://scenes/game/panels/guild.tscn",
	KEY_Q: "res://scenes/game/panels/quest.tscn",
	KEY_T: "res://scenes/game/panels/team.tscn",
	KEY_L: "res://scenes/game/panels/secured_items.tscn",
	KEY_P: "res://scenes/game/panels/purchase.tscn",
	KEY_I: "res://scenes/game/panels/input_string.tscn",
	KEY_A: "res://scenes/game/panels/auction.tscn",
	KEY_F: "res://scenes/game/panels/friend_chat.tscn",
	KEY_O: "res://scenes/game/panels/runtime_config.tscn",
	KEY_N: "res://scenes/game/panels/npc_chat.tscn",
	KEY_M: MINIMAP_PANEL_PATH,
}

var _extra_panel_nodes: Dictionary = {}
var _pending_purchase: Dictionary = {}
var _resources: RefCounted = ActorResourceScript.new()
var _pathfinder: RefCounted = WorldPathfinderScript.new()
var _next_strike := false
var _swing_magic: Dictionary = {}
var _magic_focus_uid := 0
var _move_path: Array[Vector2i] = []
var _move_step_timer := 0.0
var _chase_target_uid := 0
var _follow_focus_uid := 0
var _attack_focus_uid := 0
var _pickup_target := Vector2i(-1, -1)
var _mine_target := Vector2i(-1, -1)
var _pickup_action_timer := -1.0
var _player_action_timer := -1.0

# C++ walk motion is six frames at SYS_DEFSPEED (100 ms per frame).
# Keep a small network margin before sending the next one-hop action.
const MOVE_STEP_SECONDS := 0.75

const SWING_MAGIC_NAMES := ["烈火剑法", "翔空剑法", "莲月剑法", "半月弯刀", "十方斩"]
const TARGET_MAGIC_NAMES := [
	"圣言术", "治愈术", "困魔咒", "施毒术", "云寂术", "回生术", "雷电术", "火球术", "大火球", "冰咆哮",
	"龙卷风", "霹雳掌", "风掌", "击风", "月魂断玉", "月魂灵波", "斗转星移", "爆裂火焰", "地狱雷光", "怒神霹雳",
	"灵魂火符", "冰月神掌", "冰月震天", "乾坤大挪移", "群体治愈术", "幽灵盾", "神圣战甲术", "强魔震法", "猛虎强势",
	"集体隐身术", "移花接玉", "诱惑之光",
]
const GROUND_MAGIC_NAMES := ["火墙", "风震天", "地狱火", "冰沙掌", "魄冰刺", "疾光电影", "焰天火雨", "瞬息移动", "异形换位"]
const SELF_MAGIC_NAMES := ["隐身术", "凝血离魂", "妙影无踪", "魔法盾", "铁布衫", "阴阳法环", "抗拒火环", "破血狂杀", "召唤骷髅", "超强召唤骷髅", "召唤神兽"]


func _ready() -> void:
	game_state = get_node("/root/GameState")
	protocol = Protocol.new()
	_resources.configure_default()
	world_renderer.actor_resource = _resources
	world_renderer.game_state = game_state
	
	inventory_panel.hide()
	player_state_panel.hide()
	skill_panel.hide()
	quick_bar.hide()
	
	# C++ location format: "mapName: x y", font 10 size 15, white, at {4, localBaseY+110}
	# localBaseY = screenH - 133 = 600 - 133 = 467, so y = 467 + 110 = 577
	location_label.position = Vector2(4, 577)
	location_label.add_theme_font_size_override("font_size", 15)
	location_label.add_theme_color_override("font_color", Color(1, 1, 1, 1))
	
	NetworkClient.message_received.connect(_on_server_message)
	game_state.state_changed.connect(_refresh_grabbed_item_icon)
	control_panel.connect("minimized_changed", _on_control_panel_minimized_changed)
	_refresh_grabbed_item_icon()
	_ensure_extra_panel(MINIMAP_PANEL_PATH)
	
	if OS.has_environment("MIR2X_GAME_SCREENSHOT"):
		inventory_panel.show()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GAME_SCREENSHOT"))
		get_tree().quit()


func _process(delta: float) -> void:
	_process_player_action(delta)
	_process_pickup_action(delta)
	_process_movement(delta)
	# Update camera
	game_state.scroll_camera()
	
	# Update location label
	var map_name: String = game_state.player_map_name
	if map_name.is_empty():
		map_name = "未知地图"
	location_label.text = "%s: %d %d" % [map_name, game_state.player_x, game_state.player_y]
	
	# Ping server every 10 seconds (C++ sends CM_PING)
	_ping_timer += delta
	if _ping_timer >= 10.0:
		_ping_timer = 0.0
		_ping_tick += 1
		NetworkClient.send_ping(_ping_tick)
	
	# Redraw world
	world_renderer.set_focus_channels(_magic_focus_uid, _follow_focus_uid, _attack_focus_uid)
	world_renderer.queue_redraw()
	if grabbed_item_icon.visible:
		grabbed_item_icon.position = get_viewport().get_mouse_position() - grabbed_item_icon.size * 0.5


func _refresh_grabbed_item_icon() -> void:
	var item: Dictionary = game_state.grabbed_item
	if item.is_empty():
		grabbed_item_icon.hide()
		grabbed_item_icon.texture = null
		return
	var icon: Dictionary = _resources.item_icon(int(item.get("itemID", 0)))
	if icon.is_empty():
		grabbed_item_icon.hide()
		grabbed_item_icon.texture = null
		return
	grabbed_item_icon.texture = icon.texture
	grabbed_item_icon.size = icon.texture.get_size()
	grabbed_item_icon.show()


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	if quick_bar.visible and event is InputEventKey:
		var quick_slot: int = event.keycode - KEY_1
		if quick_slot >= 0 and quick_slot < 6:
			quick_bar.call("activate_slot", quick_slot, MOUSE_BUTTON_RIGHT)
			get_viewport().set_input_as_handled()
			return
	
	# C++ key bindings: ESC=center hero, TAB=pickup, Alt+E=exit, Alt+F=fullscreen
	if event.keycode == KEY_ESCAPE:
		_center_hero()
	elif event.keycode == KEY_TAB:
		_request_pickup()
	elif event is InputEventKey and event.alt_pressed:
		if event.keycode == KEY_E:
			get_tree().quit()
		elif event.keycode == KEY_F:
			var mode := DisplayServer.window_get_mode()
			if mode == DisplayServer.WINDOW_MODE_FULLSCREEN:
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			else:
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	elif event.keycode == KEY_B:
		_toggle_panel(inventory_panel)
	elif event.keycode == KEY_C:
		_toggle_panel(player_state_panel)
	elif event.keycode == KEY_S:
		_toggle_panel(skill_panel)
	elif EXTRA_PANELS.has(event.keycode):
		_toggle_extra_panel(EXTRA_PANELS[event.keycode])
	elif event is InputEventKey:
		_try_magic_key(event)
	
	# Mouse click handling
	if event is InputEventMouseButton and event.pressed:
		_handle_mouse_click(event)


func _handle_mouse_click(event: InputEventMouseButton) -> void:
	var grid: Vector2i = world_renderer.grid_from_screen(int(event.position.x), int(event.position.y))
	var focus_uid: int = world_renderer.focus_uid_at_screen(event.position, event.button_index == MOUSE_BUTTON_LEFT)
	if event.button_index == MOUSE_BUTTON_RIGHT:
		_attack_focus_uid = 0
		_follow_focus_uid = 0
		if focus_uid != 0:
			_cancel_movement()
			_follow_focus_uid = focus_uid
		else:
			_start_move_to(grid)
	elif event.button_index == MOUSE_BUTTON_LEFT:
		if focus_uid != 0:
			var creature: Dictionary = game_state.get_creature(focus_uid)
			if creature.get("type", 0) == 3:
				_cancel_movement()
				NetworkClient.send_npc_event(focus_uid, "", "_RSVD_NAME_ENTER_90360178872")
			elif creature.get("type", 0) == 1:
				_start_chase(focus_uid)
			return
		if not game_state.grabbed_item.is_empty():
			var grabbed: Dictionary = game_state.grabbed_item
			if NetworkClient.send_drop_item(grabbed.get("itemID", 0), grabbed.get("seqID", 0), grabbed.get("count", 0)) == OK:
				game_state.grabbed_item = {}
				game_state.state_changed.emit()
			return
		var ground_key := "%d,%d" % [grid.x, grid.y]
		if game_state.ground_items.has(ground_key):
			_start_pickup_at(grid)
		else:
			var weapon: Dictionary = game_state.wear.get(3, {})
			if _resources.item_can_mine(int(weapon.get("itemID", 0))):
				_start_mining(grid)


func _start_move_to(destination: Vector2i) -> void:
	_cancel_movement()
	_move_path = _find_path([destination])


func _start_chase(target_uid: int) -> void:
	_cancel_movement()
	_attack_focus_uid = target_uid
	_chase_target_uid = target_uid
	_plan_chase_path()


func _start_pickup_at(target: Vector2i) -> void:
	_cancel_movement()
	_pickup_target = target
	if target == Vector2i(game_state.player_x, game_state.player_y):
		_pickup_target = Vector2i(-1, -1)
		_begin_pickup_action()
		return
	_move_path = _find_path([target])
	if _move_path.is_empty():
		_pickup_target = Vector2i(-1, -1)


func _cancel_movement() -> void:
	_move_path.clear()
	_move_step_timer = 0.0
	_chase_target_uid = 0
	_pickup_target = Vector2i(-1, -1)
	_mine_target = Vector2i(-1, -1)


func _try_magic_key(event: InputEventKey) -> bool:
	var key := int(event.unicode)
	if key == 0:
		key = int(event.keycode)
	if key >= 65 and key <= 90:
		key += 32
	var magic_id := 0
	for configured_magic in game_state.magic_keys:
		if int(game_state.magic_keys[configured_magic]) == key:
			magic_id = int(configured_magic)
			break
	if magic_id <= 0 or not _has_learned_magic(magic_id):
		return false
	var layout: PackedInt32Array = _resources.skill_layout(magic_id)
	if layout.size() >= 5 and (layout[4] & 1) != 0:
		return false
	return _cast_magic(magic_id, _mouse_grid())


func _has_learned_magic(magic_id: int) -> bool:
	for magic_value in game_state.learned_magic:
		if magic_value is Dictionary and int(magic_value.get("magicID", 0)) == magic_id:
			return true
		if magic_value is int and int(magic_value) == magic_id:
			return true
	return false


func _mouse_grid() -> Vector2i:
	var mouse := get_viewport().get_mouse_position()
	return world_renderer.grid_from_screen(int(mouse.x), int(mouse.y))


func _update_magic_focus(_mouse_grid: Vector2i) -> int:
	var focus_uid: int = world_renderer.focus_uid_at_screen(get_viewport().get_mouse_position())
	if focus_uid != 0:
		_magic_focus_uid = focus_uid
		return _magic_focus_uid
	if _magic_focus_uid != 0:
		var focus: Dictionary = game_state.get_creature(_magic_focus_uid)
		if focus.is_empty() or focus.get("action_type", 0) == 13:
			_magic_focus_uid = 0
	return _magic_focus_uid


func _cast_magic(magic_id: int, mouse_grid: Vector2i) -> bool:
	if not _magic_ready(magic_id):
		game_state.add_chat_log("%s尚未冷却" % _resources.magic_names.get(magic_id, "技能"), 2)
		return false
	var magic_name: String = _resources.magic_names.get(magic_id, "")
	if magic_name in SWING_MAGIC_NAMES:
		_swing_magic[magic_id] = not bool(_swing_magic.get(magic_id, false))
		game_state.add_chat_log("%s%s" % ["开启" if _swing_magic[magic_id] else "关闭", magic_name], 0)
		return true
	var focus_uid := _update_magic_focus(mouse_grid)
	if magic_name == "空拳刀法":
		return _send_spell_action(12, magic_id, mouse_grid, focus_uid)
	if magic_name in SELF_MAGIC_NAMES:
		return _send_spell_action(9, magic_id, Vector2i(game_state.player_x, game_state.player_y), game_state.player_uid)
	if magic_name in GROUND_MAGIC_NAMES:
		return _send_spell_action(9, magic_id, mouse_grid, 0)
	if magic_name in TARGET_MAGIC_NAMES:
		return _send_spell_action(9, magic_id, mouse_grid, focus_uid)
	return false


func _send_spell_action(action_type: int, magic_id: int, aim_grid: Vector2i, aim_uid: int) -> bool:
	_cancel_movement()
	var target_grid := aim_grid
	if aim_uid == game_state.player_uid:
		target_grid = Vector2i(game_state.player_x, game_state.player_y)
	elif aim_uid != 0:
		var target: Dictionary = game_state.get_creature(aim_uid)
		if target.is_empty():
			aim_uid = 0
		else:
			target_grid = Vector2i(target.get("x", aim_grid.x), target.get("y", aim_grid.y))
	var direction := _direction_to(game_state.player_x, game_state.player_y, target_grid.x, target_grid.y)
	if direction == 0:
		direction = game_state.player_direction
	if direction != game_state.player_direction:
		var stand := {
			"type": 2, "speed": 100, "direction": direction,
			"x": game_state.player_x, "y": game_state.player_y,
			"aimX": game_state.player_x, "aimY": game_state.player_y, "aimUID": 0,
		}
		NetworkClient.send_action(Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, stand))
		game_state.player_direction = direction
	var action := {
		"type": action_type, "speed": 100, "direction": direction,
		"x": game_state.player_x, "y": game_state.player_y,
		"aimX": target_grid.x, "aimY": target_grid.y, "aimUID": aim_uid,
		"magicID": magic_id,
	}
	NetworkClient.send_action(Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action))
	game_state.magic_cast_times[magic_id] = Time.get_ticks_msec()
	if action_type == 9:
		var effect := action.duplicate(true)
		effect["uid"] = game_state.player_uid
		game_state.add_magic_effect(effect, "local_action")
	_set_player_action(action_type, action.speed, magic_id)
	_player_action_timer = _action_duration(action_type, action.speed, 2, magic_id)
	return true


func _plan_chase_path() -> void:
	var creature: Dictionary = game_state.get_creature(_chase_target_uid)
	if creature.is_empty() or creature.get("action_type", 0) == 13:
		_chase_target_uid = 0
		_move_path.clear()
		return
	var target := Vector2i(creature.get("x", 0), creature.get("y", 0))
	if _grid_distance(Vector2i(game_state.player_x, game_state.player_y), target) <= 1:
		_move_path.clear()
		_send_attack_action(_chase_target_uid)
		return
	var goals: Array[Vector2i] = []
	for direction in WorldPathfinderScript.DIRECTIONS:
		goals.append(target + direction)
	_move_path = _find_path(goals)
	_move_step_timer = 0.0 if not _move_path.is_empty() else 0.25


func _start_mining(target: Vector2i) -> void:
	_cancel_movement()
	_mine_target = target
	_plan_mine_path()


func _plan_mine_path() -> void:
	var player_position := Vector2i(game_state.player_x, game_state.player_y)
	var distance := _grid_distance(player_position, _mine_target)
	if distance == 0:
		_mine_target = Vector2i(-1, -1)
		_move_path.clear()
		return
	if distance == 1:
		_move_path.clear()
		_send_mine_action()
		return
	var goals: Array[Vector2i] = []
	for direction in WorldPathfinderScript.DIRECTIONS:
		goals.append(_mine_target + direction)
	_move_path = _find_path(goals)
	if not _move_path.is_empty():
		_move_step_timer = 0.0
		return
	var unobstructed_path: Array[Vector2i] = _pathfinder.find_path(player_position, goals, world_renderer.can_walk, {})
	if unobstructed_path.is_empty():
		_mine_target = Vector2i(-1, -1)
	else:
		_move_step_timer = 0.25


func _find_path(goals: Array[Vector2i]) -> Array[Vector2i]:
	var occupied := {}
	var player_position := Vector2i(game_state.player_x, game_state.player_y)
	for uid in game_state.creatures:
		if int(uid) == game_state.player_uid:
			continue
		var creature: Dictionary = game_state.creatures[uid]
		var creature_position := Vector2i(creature.get("x", -1), creature.get("y", -1))
		if creature_position != player_position:
			occupied[creature_position] = true
	return _pathfinder.find_path(player_position, goals, world_renderer.can_walk, occupied)


func _process_movement(delta: float) -> void:
	if _pickup_action_timer >= 0.0 or _player_action_timer >= 0.0:
		return
	if _move_path.is_empty():
		if _pickup_target.x >= 0:
			_move_step_timer -= delta
			if _move_step_timer > 0.0:
				return
			var target := _pickup_target
			_pickup_target = Vector2i(-1, -1)
			if target == Vector2i(game_state.player_x, game_state.player_y):
				_begin_pickup_action()
			return
		if _mine_target.x >= 0:
			_move_step_timer -= delta
			if _move_step_timer > 0.0:
				return
			_set_player_action(2)
			_plan_mine_path()
			return
		if _chase_target_uid == 0:
			if game_state.player_action_type == 3:
				_move_step_timer -= delta
				if _move_step_timer <= 0.0:
					_set_player_action(2)
			return
		_move_step_timer -= delta
		if _move_step_timer > 0.0:
			return
		_set_player_action(2)
		_plan_chase_path()
		return
	_move_step_timer -= delta
	if _move_step_timer > 0.0:
		return
	var next: Vector2i = _move_path.pop_front()
	_send_move_action(next.x, next.y)
	_move_step_timer = MOVE_STEP_SECONDS


func _send_move_action(aim_x: int, aim_y: int) -> void:
	if not world_renderer.can_walk(aim_x, aim_y):
		return
	# ACTION_MOVE = 3
	var action := {
		"type": 3,  # ACTION_MOVE
		"speed": 100,  # SYS_DEFSPEED
		"direction": _direction_to(game_state.player_x, game_state.player_y, aim_x, aim_y),
		"x": game_state.player_x,
		"y": game_state.player_y,
		"aimX": aim_x,
		"aimY": aim_y,
		"aimUID": 0,
	}
	var action_data := Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action)
	NetworkClient.send_action(action_data)
	game_state.player_direction = action.direction
	game_state.player_action_from_x = game_state.player_x
	game_state.player_action_from_y = game_state.player_y
	game_state.player_x = aim_x
	game_state.player_y = aim_y
	_player_action_timer = -1.0
	_set_player_action(3, action.speed)


func _grid_distance(from: Vector2i, to: Vector2i) -> int:
	return maxi(absi(from.x - to.x), absi(from.y - to.y))


func _send_attack_action(target_uid: int) -> void:
	# ACTION_ATTACK = 7
	var creature: Dictionary = game_state.get_creature(target_uid)
	var tx: int = creature.get("x", game_state.player_x)
	var ty: int = creature.get("y", game_state.player_y)
	var action := {
		"type": 7,  # ACTION_ATTACK
		"speed": 100,  # SYS_DEFSPEED
		"direction": _direction_to(game_state.player_x, game_state.player_y, tx, ty),
		"x": game_state.player_x,
		"y": game_state.player_y,
		"aimX": tx,
		"aimY": ty,
		"aimUID": target_uid,
		"magicID": _consume_attack_magic_id(),
	}
	var action_data := Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action)
	NetworkClient.send_action(action_data)
	if action.magicID > 0:
		game_state.magic_cast_times[action.magicID] = Time.get_ticks_msec()
	game_state.player_direction = action.direction
	_set_player_action(7, action.speed, action.magicID)
	_player_action_timer = _action_duration(7, action.speed, 2, action.magicID)


func _send_mine_action() -> void:
	var direction := _direction_to(game_state.player_x, game_state.player_y, _mine_target.x, _mine_target.y)
	var action := {
		"type": 14,
		"speed": 100,
		"direction": direction,
		"x": _mine_target.x,
		"y": _mine_target.y,
	}
	NetworkClient.send_action(Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action))
	game_state.player_direction = direction
	_set_player_action(14, action.speed)
	_player_action_timer = _action_duration(14, action.speed, 2)


func _consume_attack_magic_id() -> int:
	if _next_strike:
		_next_strike = false
		var next_magic: int = _resources.magic_id("攻杀剑术")
		if next_magic > 0:
			return next_magic
	for magic_name in ["莲月剑法", "翔空剑法", "烈火剑法", "十方斩", "半月弯刀"]:
		var swing_magic: int = _resources.magic_id(magic_name)
		if swing_magic > 0 and bool(_swing_magic.get(swing_magic, false)):
			if magic_name in ["莲月剑法", "翔空剑法", "烈火剑法"]:
				_swing_magic[swing_magic] = false
			return swing_magic
	var physical_magic: int = _resources.magic_id("物理攻击")
	return physical_magic


func _magic_ready(magic_id: int) -> bool:
	var layout: PackedInt32Array = _resources.skill_layout(magic_id)
	var cool_down := layout[5] if layout.size() >= 6 else 0
	if cool_down <= 0 or not game_state.magic_cast_times.has(magic_id):
		return true
	return Time.get_ticks_msec() - int(game_state.magic_cast_times[magic_id]) >= cool_down


func _direction_to(from_x: int, from_y: int, to_x: int, to_y: int) -> int:
	var dx := signi(to_x - from_x)
	var dy := signi(to_y - from_y)
	if dx == 0 and dy == 0:
		return 0
	if dy < 0:
		return 8 if dx < 0 else (2 if dx > 0 else 1)
	if dy > 0:
		return 6 if dx < 0 else (4 if dx > 0 else 5)
	return 7 if dx < 0 else 3


func _center_hero() -> void:
	# Center camera on player immediately
	game_state.center_camera_on_player()


func _on_control_panel_minimized_changed(minimized: bool) -> void:
	game_state.hud_minimized = minimized
	location_label.visible = not minimized


func _request_pickup() -> void:
	NetworkClient.send_pickup(game_state.player_x, game_state.player_y, game_state.player_map_uid)


func _begin_pickup_action() -> void:
	if _pickup_action_timer >= 0.0:
		return
	var action := {
		"type": 8,
		"speed": 100,
		"direction": game_state.player_direction,
		"x": game_state.player_x,
		"y": game_state.player_y,
	}
	NetworkClient.send_action(Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action))
	_player_action_timer = -1.0
	_set_player_action(8, action.speed)
	_pickup_action_timer = 0.2


func _process_pickup_action(delta: float) -> void:
	if _pickup_action_timer < 0.0:
		return
	_pickup_action_timer -= delta
	if _pickup_action_timer > 0.0:
		return
	_pickup_action_timer = -1.0
	NetworkClient.send_pickup(game_state.player_x, game_state.player_y, game_state.player_map_uid)
	_set_player_action(2)


func _process_player_action(delta: float) -> void:
	if _player_action_timer < 0.0:
		return
	_player_action_timer -= delta
	if _player_action_timer <= 0.0:
		_player_action_timer = -1.0
		_set_player_action(2)


func _set_player_action(action_type: int, speed := 100, magic_id := 0) -> void:
	game_state.player_action_type = action_type
	game_state.player_action_speed = speed
	game_state.player_action_magic_id = magic_id
	game_state.player_action_started_ms = Time.get_ticks_msec()
	game_state.state_changed.emit()


func _action_duration(action_type: int, speed: int, creature_type: int, magic_id := 0) -> float:
	var frame_count := 0
	match action_type:
		3, 5: frame_count = 6
		6: return 0.01
		7:
			if creature_type == 1:
				frame_count = 6
			else:
				var magic_name: String = _resources.magic_names.get(magic_id, "")
				var primary_frames := 10 if magic_name in ["翔空剑法", "莲月剑法", "十方斩"] else 6
				var primary_speed := 150 if magic_name == "十方斩" else 100
				return float(primary_frames) * 0.1 * 100.0 / float(clampi(primary_speed, 20, 500)) + 0.3
		8: frame_count = 2
		9: frame_count = 10 if creature_type == 1 else 5
		11: frame_count = 2 if creature_type == 1 else 3
		12: frame_count = 10
		14: frame_count = 9
		_: return -1.0
	return float(frame_count) * 0.1 * 100.0 / float(clampi(speed, 20, 500))


func _play_action_seff(uid: int, action: Dictionary, creature: Dictionary) -> void:
	var action_type: int = action.get("type", 0)
	var creature_type: int = creature.get("type", 2 if uid == game_state.player_uid else 0)
	var source_x: int = action.get("x", creature.get("x", game_state.player_x))
	var source_y: int = action.get("y", creature.get("y", game_state.player_y))
	if creature_type == 1:
		var monster_id: int = creature.get("monster_id", 0)
		_play_seff(_resources.monster_seff(monster_id, action_type), source_x, source_y)
		return
	if creature_type != 2:
		return
	match action_type:
		3:
			_schedule_hero_step_seff(uid, action, creature, 1, 0x01000001)
			_schedule_hero_step_seff(uid, action, creature, 4, 0x01000002)
		7, 14:
			var weapon_id := _wear_item_id(creature, 3)
			_play_seff(0x01010032 + _resources.item_weapon_sound(weapon_id), source_x, source_y)
		11:
			_play_seff(0x01030000 + (138 if int(creature.get("gender", 0)) != 0 else 139), source_x, source_y)
			_play_seff(_hero_hit_seff(action.get("aimUID", 0), _wear_item_id(creature, 1) > 0), source_x, source_y)


func _schedule_hero_step_seff(uid: int, action: Dictionary, creature: Dictionary, frame: int, seff_id: int) -> void:
	var speed := clampi(action.get("speed", 100), 20, 500)
	var token: int = creature.get("action_started_ms", game_state.player_action_started_ms)
	var delay := float(frame) * 0.1 * 100.0 / float(speed)
	get_tree().create_timer(delay).timeout.connect(func() -> void:
		if uid == game_state.player_uid:
			if game_state.player_action_type != 3 or game_state.player_action_started_ms != token:
				return
		else:
			var current: Dictionary = game_state.get_creature(uid)
			if current.get("action_type", 0) != 3 or current.get("action_started_ms", -1) != token:
				return
		var ratio := float(frame) / 6.0
		var x := roundi(lerpf(float(action.get("x", 0)), float(action.get("aimX", action.get("x", 0))), ratio))
		var y := roundi(lerpf(float(action.get("y", 0)), float(action.get("aimY", action.get("y", 0))), ratio))
		_play_seff(seff_id, x, y)
	)


func _hero_hit_seff(from_uid: int, has_dress: bool) -> int:
	var bare_id := 83 if has_dress else 73
	match (from_uid >> 59) & 0xF:
		4:
			return 0x01010000 + bare_id
		5:
			var attacker: Dictionary = game_state.get_creature(from_uid)
			if from_uid == game_state.player_uid:
				attacker = {"desp": game_state.player_desp}
			var weapon_sound: int = _resources.item_weapon_sound(_wear_item_id(attacker, 3))
			var impact_ids := [80, 82, 80, 80, 81, 80, 83, 83] if has_dress else [70, 72, 70, 70, 71, 70, 73, 73]
			return 0x01010000 + impact_ids[clampi(weapon_sound, 0, 7)]
	return 0x01010000 + bare_id


func _wear_item_id(creature: Dictionary, location: int) -> int:
	var desp: Dictionary = creature.get("desp", game_state.player_desp if creature.get("uid", game_state.player_uid) == game_state.player_uid else {})
	var wear: Dictionary = desp.get("wear", {})
	return wear.get(location, {}).get("itemID", 0)


func _play_seff(seff_id: int, x: int, y: int) -> void:
	AudioService.play_seff_at(seff_id, x, y, game_state.player_x, game_state.player_y)


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_STARTGAMESCENE:
			_handle_start_game_scene(payload)
		NetworkClient.SM_ACTION:
			_handle_action(payload)
		NetworkClient.SM_HEALTH:
			_handle_health(payload)
		NetworkClient.SM_NEXTSTRIKE:
			_next_strike = true
		NetworkClient.SM_PLAYERCONFIG:
			_handle_player_config(payload)
		NetworkClient.SM_PLAYERWLDESP:
			_handle_player_wl_desp(payload)
		NetworkClient.SM_EXP:
			game_state.update_exp(Protocol.decode_sm_exp(payload))
		NetworkClient.SM_GOLD:
			game_state.update_gold(Protocol.decode_sm_gold(payload))
		NetworkClient.SM_PING:
			pass  # Server ping echo
		NetworkClient.SM_TEXT:
			_handle_text(payload)
		NetworkClient.SM_PLAYERSAY:
			_handle_player_say(payload)
		NetworkClient.SM_PLAYERBROADCAST:
			_handle_player_broadcast(payload)
		NetworkClient.SM_NOTIFYDEAD:
			_handle_notify_dead(payload)
		NetworkClient.SM_OFFLINE:
			var data := Protocol.decode_sm_offline(payload)
			if data.get("mapUID", 0) == game_state.player_map_uid:
				game_state.remove_creature(data.get("uid", 0))
		NetworkClient.SM_PICKUPERROR:
			_handle_pickup_error(payload)
		NetworkClient.SM_MISS:
			_handle_miss(payload)
		NetworkClient.SM_BUFF:
			var buff := Protocol.decode_sm_buff(payload)
			# SMBuff: uid(u64) + type(u32) + state(u32)
			# state: 1=ON, 2=OFF
			var buff_uid: int = buff.get("uid", 0)
			var buff_type: int = buff.get("type", 0)
			var buff_state: int = buff.get("state", 0)
			var target_buffs: Array = game_state.buff_list if buff_uid == game_state.player_uid else game_state.get_creature(buff_uid).get("buffs", [])
			if buff_state == 1 and not target_buffs.has(buff_type):
				target_buffs.append(buff_type)
			elif buff_state == 2:
				target_buffs.erase(buff_type)
			if buff_uid == game_state.player_uid:
				game_state.state_changed.emit()
			elif not game_state.get_creature(buff_uid).is_empty():
				var creature: Dictionary = game_state.get_creature(buff_uid)
				creature["buffs"] = target_buffs
				game_state.update_creature(buff_uid, creature)
		NetworkClient.SM_BUFFIDLIST:
			_handle_buff_id_list(payload)
		NetworkClient.SM_INVENTORY:
			_handle_inventory(payload)
		NetworkClient.SM_BELT:
			_handle_belt(payload)
		NetworkClient.SM_STRIKEGRID:
			var sg := Protocol.decode_sm_strike_grid(payload)
			# Flash grid red briefly (C++ stores timestamp, draws red overlay for 1s)
			game_state.strike_grids["%d,%d" % [sg.get("x", 0), sg.get("y", 0)]] = Time.get_ticks_msec()
		NetworkClient.SM_CASTMAGIC:
			_handle_cast_magic(payload)
		NetworkClient.SM_COREORD:
			_handle_corecord(payload)
		NetworkClient.SM_PLAYERNAME:
			_handle_player_name(payload)
		NetworkClient.SM_DEADFADEOUT:
			var data := Protocol.decode_sm_dead_fade_out(payload)
			if data.get("mapUID", 0) == game_state.player_map_uid:
				_request_dead_fade_out(data.get("uid", 0))
		NetworkClient.SM_REMOVEITEM:
			if payload.size() >= 10:
				game_state.remove_item(payload.decode_u32(0), payload.decode_u32(4), payload.decode_u16(8))
		NetworkClient.SM_REMOVEGROUNDITEM:
			# SMRemoveGroundItem: X(u16) + Y(u16) + ID(u32) + DBID(u32)
			if payload.size() >= 12:
				var rx: int = payload.decode_u16(0)
				var ry: int = payload.decode_u16(2)
				var rid: int = payload.decode_u32(4)
				game_state.remove_ground_item(rx, ry, rid)
		NetworkClient.SM_GROUNDITEMIDLIST:
			_handle_ground_item_id_list(payload)
		NetworkClient.SM_EQUIPWEAR:
			_handle_equip_wear(payload)
		NetworkClient.SM_EQUIPWEARERROR:
			_handle_equip_wear_error(payload)
		NetworkClient.SM_GRABWEAR:
			_handle_grab_wear(payload)
		NetworkClient.SM_GRABWEARERROR:
			_handle_grab_wear_error(payload)
		NetworkClient.SM_EQUIPBELT:
			_handle_equip_belt(payload)
		NetworkClient.SM_EQUIPBELTERROR:
			_handle_equip_belt_error(payload)
		NetworkClient.SM_GRABBELT:
			_handle_grab_belt(payload)
		NetworkClient.SM_GRABBELTERROR:
			_handle_grab_belt_error(payload)
		NetworkClient.SM_UPDATEITEM:
			var reader := CerealReader.new(payload)
			var item := reader.read_sd_update_item()
			if _reader_ok(reader, "SM_UPDATEITEM"):
				game_state.update_item(item)
		NetworkClient.SM_SHOWSECUREDITEMLIST:
			_handle_secured_items(payload)
		NetworkClient.SM_REMOVESECUREDITEM:
			if payload.size() >= 8:
				game_state.remove_secured_item(payload.decode_u32(0), payload.decode_u32(4))
		NetworkClient.SM_TEAMCANDIDATE:
			_handle_team_candidate(payload)
		NetworkClient.SM_TEAMMEMBERLIST:
			_handle_team_members(payload)
		NetworkClient.SM_QUESTDESPLIST:
			_handle_quest_list(payload)
		NetworkClient.SM_QUESTDESPUPDATE:
			_handle_quest_update(payload)
		NetworkClient.SM_LEARNEDMAGICLIST:
			_handle_learned_magic(payload)
		NetworkClient.SM_GROUNDFIREWALLLIST:
			_handle_ground_firewalls(payload)
		NetworkClient.SM_NPCXMLLAYOUT:
			_handle_npc_xml(payload)
		NetworkClient.SM_NPCSELL:
			_handle_npc_sell(payload)
		NetworkClient.SM_SELLITEMLIST:
			_handle_sell_item_list(payload)
		NetworkClient.SM_BUYSUCCEED:
			_handle_buy_succeed(payload)
		NetworkClient.SM_BUYERROR:
			_handle_buy_error(payload)
		NetworkClient.SM_STARTINPUT:
			_handle_start_input(payload)
		NetworkClient.SM_STARTINVOP:
			_handle_start_inventory_operation(payload)
		NetworkClient.SM_INVOPCOST:
			_handle_inventory_operation_cost(payload)
		NetworkClient.SM_FRIENDLIST:
			_handle_friend_list(payload)
		NetworkClient.SM_CHATMESSAGELIST:
			_handle_chat_message_list(payload)
		NetworkClient.SM_CREATECHATGROUP:
			_handle_chat_group(payload)
		NetworkClient.SM_ADDFRIENDACCEPTED:
			_handle_friend_result(payload, true)
		NetworkClient.SM_ADDFRIENDREJECTED:
			_handle_friend_result(payload, false)


func _handle_start_game_scene(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_start_game_scene()
	if not _reader_ok(reader, "SM_STARTGAMESCENE"):
		return
	if data.get("uid", 0) != game_state.player_uid or data.get("mapUID", 0) != game_state.player_map_uid:
		push_error("SM_STARTGAMESCENE identity mismatch")
		return
	_cancel_movement()
	_pickup_action_timer = -1.0
	_player_action_timer = -1.0
	game_state.start_game_scene(data)
	if world_renderer.load_map(game_state.player_map_id):
		game_state.player_map_name = world_renderer.world_resource.map_name
		AudioService.play_map_bgm(world_renderer.world_resource.bgm_id)
	else:
		AudioService.stop_bgm()
	_center_hero()


func _handle_action(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_action(payload)
	if data.get("mapUID", 0) != game_state.player_map_uid:
		return
	var uid: int = data.get("uid", 0)
	var action: Dictionary = data.get("action", {})
	var x: int = action.get("x", 0)
	var y: int = action.get("y", 0)
	var action_type: int = action.get("type", 0)
	var direction: int = action.get("direction", 0)
	if action_type == 6:
		var space_magic_id: int = _resources.magic_id("瞬息移动")
		if space_magic_id > 0:
			var space_effect := action.duplicate(true)
			space_effect["uid"] = uid
			space_effect["magicID"] = space_magic_id
			game_state.add_magic_effect(space_effect, "action")
	if action_type == 9 and action.get("magicID", 0) > 0:
		if uid != game_state.player_uid or not _has_pending_local_magic(action.get("magicID", 0)):
			var effect := action.duplicate(true)
			effect["uid"] = uid
			game_state.add_magic_effect(effect, "action")
	if action_type == 11:
		game_state.trigger_shield_hit(uid)
	
	if uid == game_state.player_uid:
		# Update player position and direction
		game_state.player_action_from_x = x
		game_state.player_action_from_y = y
		game_state.player_x = action.get("aimX", x) if _action_uses_aim_position(action_type) else x
		game_state.player_y = action.get("aimY", y) if _action_uses_aim_position(action_type) else y
		if direction >= 1:
			game_state.player_direction = direction
		_set_player_action(action_type, action.get("speed", 100), action.get("magicID", 0))
		_player_action_timer = _action_duration(action_type, action.get("speed", 100), 2, action.get("magicID", 0))
		if action_type == 2 and not _move_path.is_empty():
			_move_path.clear()
			_move_step_timer = 0.25
		elif action_type == 13:
			_move_path.clear()
			_chase_target_uid = 0
		_play_action_seff(uid, action, {"uid": uid, "type": 2, "gender": game_state.player_gender, "desp": game_state.player_desp, "action_started_ms": game_state.player_action_started_ms})
	else:
		# Update or create creature
		var creature: Dictionary = game_state.get_creature(uid)
		if creature.is_empty():
			var inferred_type := _creature_type_from_uid(uid)
			creature = {
				"uid": uid,
				"x": action.get("aimX", x) if _action_uses_aim_position(action_type) else x,
				"y": action.get("aimY", y) if _action_uses_aim_position(action_type) else y,
				"action_from_x": x,
				"action_from_y": y,
				"type": inferred_type,
				"name": "",
				"action_type": action_type,
				"action_started_ms": Time.get_ticks_msec(),
				"action_speed": action.get("speed", 100),
				"action_magic_id": action.get("magicID", 0),
				"direction": direction if direction >= 1 else (1 if inferred_type == 3 else _direction_to(x, y, action.get("aimX", x), action.get("aimY", y))),
			}
			if inferred_type == 1:
				creature["monster_id"] = (uid >> 35) & 0xFFFFFF
			elif inferred_type == 3:
				creature["npc_id"] = (uid >> 35) & 0xFFFFFF
		else:
			creature["action_from_x"] = x
			creature["action_from_y"] = y
			creature["x"] = action.get("aimX", x) if _action_uses_aim_position(action_type) else x
			creature["y"] = action.get("aimY", y) if _action_uses_aim_position(action_type) else y
			creature["action_type"] = action_type
			creature["action_started_ms"] = Time.get_ticks_msec()
			creature["action_speed"] = action.get("speed", 100)
			creature["action_magic_id"] = action.get("magicID", 0)
			if direction >= 1:
				creature["direction"] = direction
		game_state.update_creature(uid, creature)
		var duration := _action_duration(action_type, action.get("speed", 100), creature.get("type", 0), action.get("magicID", 0))
		if duration > 0.0:
			_schedule_creature_idle(uid, action_type, creature.get("action_started_ms", 0), duration)
		_play_action_seff(uid, action, creature)


func _has_pending_local_magic(magic_id: int) -> bool:
	var now := Time.get_ticks_msec()
	for effect_value in game_state.magic_effects:
		var effect: Dictionary = effect_value
		if effect.get("source", "") == "local_action" and int(effect.get("magicID", 0)) == magic_id and now - int(effect.get("start_time", 0)) < 1500:
			return true
	return false


func _action_uses_aim_position(action_type: int) -> bool:
	return action_type in [3, 5, 6]


func _schedule_creature_idle(uid: int, action_type: int, started_ms: int, delay: float) -> void:
	get_tree().create_timer(delay).timeout.connect(func() -> void:
		var creature: Dictionary = game_state.get_creature(uid)
		if not creature.is_empty() and creature.get("action_type", 0) == action_type and creature.get("action_started_ms", -1) == started_ms:
			creature["action_type"] = 2
			creature["action_started_ms"] = Time.get_ticks_msec()
			game_state.update_creature(uid, creature)
	)


func _handle_corecord(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_corecord(payload)
	var uid: int = data.get("uid", 0)
	var action: Dictionary = data.get("action", {})
	
	# Determine creature type from UID type bits
	# UID type is in bits 59-62 (4 bits at offset 59)
	# UID_NPC=3, UID_MON=4, UID_PLY=5
	var c_type := _creature_type_from_uid(uid)
	
	var creature: Dictionary = game_state.get_creature(uid)
	if creature.is_empty():
		creature = {
			"uid": uid,
			"x": action.get("aimX", action.get("x", 0)) if _action_uses_aim_position(action.get("type", 0)) else action.get("x", 0),
			"y": action.get("aimY", action.get("y", 0)) if _action_uses_aim_position(action.get("type", 0)) else action.get("y", 0),
			"type": c_type,
			"name": "",
			"action_type": action.get("type", 0),
			"action_started_ms": Time.get_ticks_msec(),
			"action_speed": action.get("speed", 100),
			"action_magic_id": action.get("magicID", 0),
			"action_from_x": action.get("x", 0),
			"action_from_y": action.get("y", 0),
			"direction": action.get("direction", 0),
		}
	else:
		creature["action_from_x"] = action.get("x", 0)
		creature["action_from_y"] = action.get("y", 0)
		creature["x"] = action.get("aimX", action.get("x", 0)) if _action_uses_aim_position(action.get("type", 0)) else action.get("x", 0)
		creature["y"] = action.get("aimY", action.get("y", 0)) if _action_uses_aim_position(action.get("type", 0)) else action.get("y", 0)
		creature["type"] = c_type
		creature["action_type"] = action.get("type", 0)
		creature["action_started_ms"] = Time.get_ticks_msec()
		creature["action_speed"] = action.get("speed", 100)
		creature["action_magic_id"] = action.get("magicID", 0)
		creature["direction"] = action.get("direction", 0)
	
	# Parse union data for additional info
	var union_data: PackedByteArray = data.get("union_data", PackedByteArray())
	if union_data.size() >= 4:
		match c_type:
			1:  # Monster: MonsterID (u32)
				creature["monster_id"] = union_data.decode_u32(0)
			2:  # Player: gender:1, job:3, Level:32
				var gj: int = union_data[0]
				creature["gender"] = gj & 1
				creature["job"] = (gj >> 1) & 7
				creature["level"] = union_data.decode_u32(1) if union_data.size() >= 5 else 0
			3:  # NPC: NPCID (u32)
				creature["npc_id"] = union_data.decode_u32(0)
	
	game_state.update_creature(uid, creature)
	var duration := _action_duration(action.get("type", 0), action.get("speed", 100), c_type, action.get("magicID", 0))
	if duration > 0.0:
		_schedule_creature_idle(uid, action.get("type", 0), creature.get("action_started_ms", 0), duration)
	_play_action_seff(uid, action, creature)


func _creature_type_from_uid(uid: int) -> int:
	match (uid >> 59) & 0xF:
		3: return 3 # NPC
		4: return 1 # Monster
		5: return 2 # Player
		_: return 0


func _handle_health(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_health()
	if _reader_ok(reader, "SM_HEALTH"):
		game_state.update_entity_health(data)


func _handle_text(payload: PackedByteArray) -> void:
	# SM_TEXT is type-3 (variable), raw UTF-8 text bytes (not cereal)
	var text := payload.get_string_from_utf8()
	game_state.add_chat_log(text, 0)


func _handle_player_say(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_player_say(payload)
	var uid: int = data.get("uid", 0)
	var content: String = data.get("content", "")
	var creature: Dictionary = game_state.get_creature(uid)
	if uid == game_state.player_uid or creature.get("type", 0) == 2:
		game_state.add_player_say(uid, content)
	if uid != game_state.player_uid:
		game_state.add_chat_log(content, 0)


func _handle_player_broadcast(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_player_broadcast(payload)
	var content: String = data.get("content", "")
	game_state.add_chat_log("[广播] %s" % content, 2)


func _handle_inventory(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var items := reader.read_sd_inventory()
	if _reader_ok(reader, "SM_INVENTORY"):
		game_state.update_inventory(items)


func _handle_belt(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var items := reader.read_sd_belt()
	if _reader_ok(reader, "SM_BELT"):
		game_state.update_belt(items)


func _handle_ground_item_id_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_ground_item_id_list()
	var map_uid: int = data.get("mapUID", 0)
	if map_uid != game_state.player_map_uid:
		return
	# The server sends incremental grid snapshots: only replace grids present in this message.
	game_state.update_ground_item_grids(data.get("grids", []))


func _handle_cast_magic(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_cast_magic(payload)
	if not data.is_empty() and data.get("mapUID", 0) == game_state.player_map_uid:
		var magic_name: String = _resources.magic_names.get(data.get("magic", 0), "")
		game_state.add_cast_magic_attachment(data, magic_name)
		game_state.add_chat_log("使用魔法: %s" % magic_name, 0)


func _handle_miss(payload: PackedByteArray) -> void:
	var uid: int = Protocol.decode_sm_miss(payload)
	var c: Dictionary = game_state.get_creature(uid)
	var x: int = c.get("x", game_state.player_x)
	var y: int = c.get("y", game_state.player_y)
	game_state.add_ascend_string(x, y, "Miss", Color(1, 1, 1, 1))


func _handle_player_name(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_player_name()
	var uid: int = data.get("uid", 0)
	var name: String = data.get("name", "")
	if not name.is_empty():
		var c: Dictionary = game_state.get_creature(uid)
		if not c.is_empty():
			c["name"] = name
			c["name_color"] = data.get("nameColor", 0xFFFFFFFF)
			game_state.update_creature(uid, c)


func _handle_player_config(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var config := reader.read_sd_player_config()
	if _reader_ok(reader, "SM_PLAYERCONFIG"):
		game_state.magic_keys = config.get("magicKeys", {})
		game_state.runtime_config = config.get("runtimeConfig", {})
		AudioService.apply_runtime_config(game_state.runtime_config)
		game_state.state_changed.emit()


func _handle_player_wl_desp(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_uid_wldesp()
	if not _reader_ok(reader, "SM_PLAYERWLDESP"):
		return
	var uid: int = data.get("uid", 0)
	if uid == game_state.player_uid:
		game_state.player_desp = data.get("desp", {})
		game_state.wear = game_state.player_desp.get("wear", {})
		game_state.state_changed.emit()
	elif not game_state.get_creature(uid).is_empty():
		var creature: Dictionary = game_state.get_creature(uid)
		creature["desp"] = data.get("desp", {})
		game_state.update_creature(uid, creature)


func _handle_buff_id_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_buff_id_list()
	if not _reader_ok(reader, "SM_BUFFIDLIST"):
		return
	var uid: int = data.get("uid", 0)
	if uid == game_state.player_uid:
		game_state.buff_list = data.get("ids", [])
		game_state.state_changed.emit()
	elif not game_state.get_creature(uid).is_empty():
		var creature: Dictionary = game_state.get_creature(uid)
		creature["buffs"] = data.get("ids", [])
		game_state.update_creature(uid, creature)


func _handle_equip_wear(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_equip_wear()
	if not _reader_ok(reader, "SM_EQUIPWEAR"):
		return
	var uid: int = data.get("uid", 0)
	if uid == game_state.player_uid:
		game_state.wear[data.get("wltype", 0)] = data.get("item", {})
		var equipped: Dictionary = data.get("item", {})
		if game_state.grabbed_item.get("itemID", 0) == equipped.get("itemID", 0) and game_state.grabbed_item.get("seqID", 0) == equipped.get("seqID", 0):
			game_state.grabbed_item = {}
		game_state.state_changed.emit()
	elif not game_state.get_creature(uid).is_empty():
		var creature: Dictionary = game_state.get_creature(uid)
		var desp: Dictionary = creature.get("desp", {})
		var wear_data: Dictionary = desp.get("wear", {})
		wear_data[data.get("wltype", 0)] = data.get("item", {})
		desp["wear"] = wear_data
		creature["desp"] = desp
		game_state.update_creature(uid, creature)


func _handle_grab_wear(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_grab_wear()
	if _reader_ok(reader, "SM_GRABWEAR"):
		game_state.wear.erase(data.get("wltype", 0))
		if not game_state.grabbed_item.is_empty():
			game_state.inventory.append(game_state.grabbed_item)
		game_state.grabbed_item = data.get("item", {})
		game_state.state_changed.emit()


func _handle_equip_belt(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_equip_belt()
	if not _reader_ok(reader, "SM_EQUIPBELT"):
		return
	var slot: int = data.get("slot", -1)
	if slot >= 0 and slot < game_state.belt.size():
		var equipped: Dictionary = data.get("item", {})
		game_state.belt[slot] = equipped
		if game_state.grabbed_item.get("itemID", 0) == equipped.get("itemID", 0) and game_state.grabbed_item.get("seqID", 0) == equipped.get("seqID", 0):
			game_state.grabbed_item = {}
		game_state.state_changed.emit()


func _handle_grab_belt(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_grab_belt()
	if not _reader_ok(reader, "SM_GRABBELT"):
		return
	var slot: int = data.get("slot", -1)
	if slot >= 0 and slot < game_state.belt.size():
		game_state.belt[slot] = {}
		if not game_state.grabbed_item.is_empty():
			game_state.inventory.append(game_state.grabbed_item)
		game_state.grabbed_item = data.get("item", {})
		game_state.state_changed.emit()


func _handle_pickup_error(payload: PackedByteArray) -> void:
	if payload.size() < 4:
		return
	var item_id := payload.decode_u32(0)
	if item_id > 0:
		game_state.add_chat_log("无法捡起%s" % _resources.item_name(item_id), 1)
	else:
		game_state.add_chat_log("当前无法捡起物品，请稍后再试", 1)


func _handle_notify_dead(payload: PackedByteArray) -> void:
	var uid := Protocol.decode_sm_notify_dead(payload)
	if uid == 0:
		return
	if uid == game_state.player_uid:
		_cancel_movement()
		_pickup_action_timer = -1.0
		_player_action_timer = -1.0
		_set_player_action(13)
		game_state.add_chat_log("你已死亡", 3)
		return
	var creature: Dictionary = game_state.get_creature(uid)
	if not creature.is_empty():
		var was_dead: bool = creature.get("action_type", 0) == 13
		creature["action_type"] = 13
		creature["action_started_ms"] = Time.get_ticks_msec()
		creature["action_speed"] = 100
		game_state.update_creature(uid, creature)
		if not was_dead:
			_play_action_seff(uid, {"type": 13, "x": creature.get("x", 0), "y": creature.get("y", 0)}, creature)


func _request_dead_fade_out(uid: int) -> void:
	var creature: Dictionary = game_state.get_creature(uid)
	if creature.get("type", 0) != 1 or creature.get("action_type", 0) != 13:
		return
	if not _resources.monster_dead_fade_out(creature.get("monster_id", 0)):
		return
	creature["dead_fade_requested_ms"] = Time.get_ticks_msec()
	game_state.update_creature(uid, creature)


func _handle_equip_wear_error(payload: PackedByteArray) -> void:
	if payload.size() < 10:
		return
	var item_id := payload.decode_u32(0)
	match payload.decode_u16(8):
		1, 2: game_state.add_chat_log("无效的物品", 3)
		3: game_state.add_chat_log("无法放置：%s" % _resources.item_name(item_id), 3)
		4: game_state.add_chat_log("角色属性不足，无法装备：%s" % _resources.item_name(item_id), 3)


func _handle_grab_wear_error(payload: PackedByteArray) -> void:
	if payload.size() >= 2 and payload.decode_u16(0) == 2:
		game_state.add_chat_log("无法取下装备", 3)


func _handle_equip_belt_error(payload: PackedByteArray) -> void:
	if payload.size() < 10:
		return
	var item_id := payload.decode_u32(0)
	match payload.decode_u16(8):
		1, 2: game_state.add_chat_log("无效的物品", 3)
		3: game_state.add_chat_log("无法装备：%s" % _resources.item_name(item_id), 3)
		4: game_state.add_chat_log("无效的快捷栏位置", 3)


func _handle_grab_belt_error(payload: PackedByteArray) -> void:
	if payload.size() >= 2 and payload.decode_u16(0) == 1:
		game_state.add_chat_log("快捷栏中没有物品", 3)


func _handle_secured_items(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var items := reader.read_sd_show_secured_item_list()
	if _reader_ok(reader, "SM_SHOWSECUREDITEMLIST"):
		game_state.secured_items = items
		game_state.state_changed.emit()


func _handle_team_candidate(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var candidate := reader.read_sd_team_candidate()
	if _reader_ok(reader, "SM_TEAMCANDIDATE"):
		game_state.add_team_candidate(candidate)


func _handle_team_members(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var team := reader.read_sd_team_member_list()
	if _reader_ok(reader, "SM_TEAMMEMBERLIST"):
		game_state.set_team_member_list(team.get("teamLeader", 0), team.get("members", []))


func _handle_quest_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var quests := reader.read_sd_quest_desp_list()
	if _reader_ok(reader, "SM_QUESTDESPLIST"):
		game_state.quests = quests
		game_state.state_changed.emit()


func _handle_quest_update(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var update := reader.read_sd_quest_desp_update()
	if not _reader_ok(reader, "SM_QUESTDESPUPDATE"):
		return
	var quest_name: String = update.get("name", "")
	var state_map: Dictionary = game_state.quests.get(quest_name, {})
	var fsm: String = update.get("fsm", "")
	if update.get("desp") == null:
		state_map.erase(fsm)
	else:
		state_map[fsm] = update.get("desp", "")
	if state_map.is_empty():
		game_state.quests.erase(quest_name)
	else:
		game_state.quests[quest_name] = state_map
	game_state.state_changed.emit()


func _handle_learned_magic(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var magic_list := reader.read_sd_learned_magic_list()
	if _reader_ok(reader, "SM_LEARNEDMAGICLIST"):
		game_state.learned_magic = magic_list
		game_state.state_changed.emit()


func _handle_ground_firewalls(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_ground_firewall_list()
	if _reader_ok(reader, "SM_GROUNDFIREWALLLIST") and data.get("mapUID", 0) == game_state.player_map_uid:
		game_state.firewalls = data.get("firewalls", [])
		game_state.state_changed.emit()


func _handle_npc_xml(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_npc_xml_layout()
	if _reader_ok(reader, "SM_NPCXMLLAYOUT"):
		game_state.npc_dialog = data
		game_state.state_changed.emit()
		_ensure_extra_panel("res://scenes/game/panels/npc_chat.tscn").show()


func _handle_npc_sell(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_npc_sell()
	if _reader_ok(reader, "SM_NPCSELL"):
		game_state.npc_sell = data
		game_state.state_changed.emit()
		_ensure_extra_panel("res://scenes/game/panels/purchase.tscn").show()


func _handle_sell_item_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_sell_item_list()
	if _reader_ok(reader, "SM_SELLITEMLIST"):
		game_state.npc_sell_detail = data
		game_state.state_changed.emit()


func _handle_buy_succeed(payload: PackedByteArray) -> void:
	if payload.size() < 16:
		return
	var npc_uid := payload.decode_u64(0)
	var item_id := payload.decode_u32(8)
	var seq_id := payload.decode_u32(12)
	if game_state.npc_sell_detail.get("npcUID", 0) == npc_uid:
		var list: Array = game_state.npc_sell_detail.get("list", [])
		for index in range(list.size() - 1, -1, -1):
			var item: Dictionary = list[index].get("item", {})
			if item.get("itemID", 0) == item_id and item.get("seqID", 0) == seq_id and seq_id != 0:
				list.remove_at(index)
		game_state.npc_sell_detail["list"] = list
	game_state.add_chat_log("购买成功", 1)
	game_state.state_changed.emit()


func _handle_buy_error(payload: PackedByteArray) -> void:
	if payload.size() < 18:
		return
	game_state.add_chat_log("购买失败，错误码 %d" % payload.decode_u16(16), 3)


func _handle_start_input(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_start_input()
	if _reader_ok(reader, "SM_STARTINPUT"):
		game_state.pending_input = data
		game_state.state_changed.emit()
		var panel := _ensure_extra_panel("res://scenes/game/panels/input_string.tscn")
		panel.configure(data.get("title", ""), data.get("show", false))


func _handle_start_inventory_operation(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var operation := reader.read_sd_start_inv_op()
	if not _reader_ok(reader, "SM_STARTINVOP"):
		return
	game_state.start_inventory_operation(operation)
	inventory_panel.show()
	inventory_panel.position = Vector2(size.x - inventory_panel.size.x, 0)
	inventory_panel.move_to_front()


func _handle_inventory_operation_cost(payload: PackedByteArray) -> void:
	if payload.size() < 16:
		return
	game_state.set_inventory_operation_cost({
		"invOp": payload.decode_u32(0),
		"itemID": payload.decode_u32(4),
		"seqID": payload.decode_u32(8),
		"cost": payload.decode_u32(12),
	})


func _handle_friend_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var friends := reader.read_sd_chat_peer_list()
	if _reader_ok(reader, "SM_FRIENDLIST"):
		game_state.set_chat_friends(friends)


func _handle_chat_message_list(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var messages := reader.read_sd_chat_message_list()
	if not _reader_ok(reader, "SM_CHATMESSAGELIST"):
		return
	for message in messages:
		game_state.add_chat_message(message)


func _handle_chat_group(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var peer := reader.read_sd_chat_peer()
	if _reader_ok(reader, "SM_CREATECHATGROUP"):
		game_state.add_chat_peer(peer, true)


func _handle_friend_result(payload: PackedByteArray, accepted: bool) -> void:
	var reader := CerealReader.new(payload)
	var peer := reader.read_sd_chat_peer()
	if not _reader_ok(reader, "SM_ADDFRIEND"):
		return
	game_state.add_chat_peer(peer, accepted)
	game_state.add_chat_log("%s已%s你的好友申请" % [peer.get("name", "对方"), "通过" if accepted else "拒绝"], 1 if accepted else 3)


func _reader_ok(reader: RefCounted, packet_name: String) -> bool:
	if not reader.valid:
		push_error("%s parse failed: %s" % [packet_name, reader.error])
		return false
	if not reader.at_end():
		push_error("%s has %d unread archive bytes" % [packet_name, reader.remaining()])
		return false
	return true


func _decode_u64_payload(buf: PackedByteArray, offset: int) -> int:
	if buf.size() < offset + 8:
		return 0
	return buf.decode_u32(offset) | (buf.decode_u32(offset + 4) << 32)


func _on_control_panel_panel_requested(scene_path: String) -> void:
	if scene_path.ends_with("/inventory.tscn"):
		_toggle_panel(inventory_panel)
	elif scene_path.ends_with("/player_state.tscn"):
		_toggle_panel(player_state_panel)
	elif scene_path.ends_with("/skill.tscn"):
		_toggle_panel(skill_panel)
	else:
		_toggle_extra_panel(scene_path)


func _on_control_panel_quick_bar_toggled() -> void:
	quick_bar.visible = not quick_bar.visible


func _on_control_panel_magic_key_hud_toggled() -> void:
	game_state.magic_key_hud_visible = not game_state.magic_key_hud_visible
	game_state.state_changed.emit()


func minimap_hud_width() -> float:
	var panel := _extra_panel_nodes.get(MINIMAP_PANEL_PATH) as Control
	if panel == null or not panel.visible:
		return 0.0
	var texture_rect := panel.get_node_or_null("MapViewport/MapTexture") as TextureRect
	return panel.size.x if texture_rect != null and texture_rect.texture != null else 0.0


func _on_quick_bar_close_pressed() -> void:
	quick_bar.hide()


func _toggle_panel(panel: Control) -> void:
	panel.visible = not panel.visible
	if panel.visible:
		panel.move_to_front()


func _toggle_extra_panel(scene_path: String) -> void:
	var panel := _ensure_extra_panel(scene_path)
	if not panel:
		return
	if scene_path == MINIMAP_PANEL_PATH:
		panel.call("toggle_requested_visibility")
		if panel.visible:
			panel.move_to_front()
		return
	_toggle_panel(panel)


func _ensure_extra_panel(scene_path: String) -> Control:
	var panel := _extra_panel_nodes.get(scene_path) as Control
	if panel:
		return panel
	var packed := load(scene_path) as PackedScene
	if not packed:
		return null
	panel = packed.instantiate() as Control
	add_child(panel)
	panel.position = Vector2(size.x - panel.size.x, 0.0) if scene_path.ends_with("/minimap.tscn") else (size - panel.size) * 0.5
	if scene_path == MINIMAP_PANEL_PATH:
		panel.call("set_requested_visible", true)
	else:
		panel.hide()
	_extra_panel_nodes[scene_path] = panel
	if scene_path.ends_with("/input_string.tscn") and panel.has_signal("committed"):
		panel.committed.connect(_on_input_committed)
	if scene_path.ends_with("/purchase.tscn") and panel.has_signal("quantity_requested"):
		panel.quantity_requested.connect(_on_purchase_quantity_requested)
	return panel


func _on_input_committed(value: String) -> void:
	if not _pending_purchase.is_empty():
		var count := value.to_int()
		if count > 0:
			NetworkClient.send_buy(
				_pending_purchase.get("npcUID", 0),
				_pending_purchase.get("itemID", 0),
				0,
				count,
			)
		else:
			game_state.add_chat_log("无效的购买数量：%s" % value, 3)
		_pending_purchase = {}
		return
	var input: Dictionary = game_state.pending_input
	NetworkClient.send_npc_event(input.get("uid", 0), "", input.get("commitTag", ""), value)
	game_state.pending_input = {}


func _on_purchase_quantity_requested(npc_uid: int, item_id: int, item_name: String) -> void:
	_pending_purchase = {"npcUID": npc_uid, "itemID": item_id}
	var panel := _ensure_extra_panel("res://scenes/game/panels/input_string.tscn")
	panel.configure("请输入购买 %s 的数量" % item_name, true)
