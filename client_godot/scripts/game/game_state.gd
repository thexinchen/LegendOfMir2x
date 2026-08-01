extends Node

# Game state manager - tracks all client-side game state
# Corresponds to ProcessRun in the C++ client

signal state_changed

# Player state
var player_uid: int = 0
var player_name: String = ""
var player_gender: int = 0
var player_job: int = 0
var player_level: int = 1
var player_exp: int = 0
var player_gold: int = 0
var player_x: int = 0
var player_y: int = 0
var player_direction: int = 0
var player_hp: int = 0
var player_hp_max: int = 0
var player_mp: int = 0
var player_mp_max: int = 0
var player_map_uid: int = 0
var player_map_id: int = 0
var player_map_name: String = ""

# Combat stats
var ac_min: int = 0
var ac_max: int = 0
var dc_min: int = 0
var dc_max: int = 0
var ac_magic: bool = false  # show MA instead of AC
var dc_magic: bool = false  # show MC instead of DC

# Inventory (10x10 grid, 38px cells)
var inventory: Array = []  # list of SDItem
var belt: Array = []  # 6 slots, each is SDItem or null

# Buffs
var buff_list: Array = []  # list of {id, type, state}

# All visible creatures (players, monsters, NPCs)
var creatures: Dictionary = {}  # uid -> CreatureData

# Ground items
var ground_items: Dictionary = {}  # "x,y" -> list of item IDs

# Chat log
var chat_log: Array = []  # list of {type, text, color}
const CHAT_LOG_MAX := 100

# Magic/skill list
var learned_magic: Array = []  # list of magic IDs
var magic_keys: Dictionary = {}  # magicID -> key char

# Camera position (pixel coordinates)
var view_x: float = 0.0
var view_y: float = 0.0

# System constants
const GRID_XP := 48
const GRID_YP := 32
const SCREEN_W := 800
const SCREEN_H := 600


func _ready() -> void:
	# Initialize inventory grid
	inventory.resize(100)
	for i in range(100):
		inventory[i] = null
	belt.resize(6)
	for i in range(6):
		belt[i] = null


func set_player_online(online_data: Dictionary) -> void:
	player_uid = online_data.get("uid", 0)
	player_name = online_data.get("name", "")
	player_gender = online_data.get("gender", 0)
	player_job = online_data.get("job", 0)
	player_map_uid = online_data.get("map_uid", 0)
	player_x = online_data.get("x", 0)
	player_y = online_data.get("y", 0)
	player_direction = online_data.get("direction", 0)
	player_map_id = _map_id_from_uid(player_map_uid)
	_center_camera_on_player()
	state_changed.emit()


func start_game_scene(scene_data: Dictionary) -> void:
	player_name = scene_data.get("name", player_name)
	player_x = scene_data.get("x", player_x)
	player_y = scene_data.get("y", player_y)
	player_direction = scene_data.get("direction", player_direction)
	_center_camera_on_player()
	state_changed.emit()


func update_health(hp: int, hp_max: int, mp: int, mp_max: int) -> void:
	player_hp = hp
	player_hp_max = hp_max
	player_mp = mp
	player_mp_max = mp_max
	state_changed.emit()


func update_exp(exp: int) -> void:
	player_exp = exp
	player_level = _level_from_exp(exp)
	state_changed.emit()


func update_gold(gold: int) -> void:
	player_gold = gold
	state_changed.emit()


func add_chat_log(text: String, log_type: int = 0) -> void:
	var color := Color.WHITE
	match log_type:
		0: color = Color(1, 1, 1, 1)      # white
		1: color = Color(0, 1, 0, 1)       # green
		2: color = Color(0.25, 0.5, 1, 1)  # blue
		3: color = Color(1, 0.25, 0.25, 1) # red
	chat_log.append({"type": log_type, "text": text, "color": color})
	if chat_log.size() > CHAT_LOG_MAX:
		chat_log.pop_front()
	state_changed.emit()


func update_creature(uid: int, data: Dictionary) -> void:
	creatures[uid] = data
	state_changed.emit()


func remove_creature(uid: int) -> void:
	creatures.erase(uid)
	state_changed.emit()


func get_creature(uid: int) -> Dictionary:
	return creatures.get(uid, {})


func update_inventory(items: Array) -> void:
	inventory = items
	state_changed.emit()


func update_belt(items: Array) -> void:
	belt = items
	state_changed.emit()


func _center_camera_on_player() -> void:
	view_x = float(player_x) * GRID_XP - SCREEN_W * 0.5
	view_y = float(player_y) * GRID_YP - SCREEN_H * 0.5


func _map_id_from_uid(map_uid: int) -> int:
	# Map UID format: high bits contain map ID
	# Avoid signed overflow by masking
	return (map_uid >> 32) & 0xFFFF


func _level_from_exp(exp: int) -> int:
	var level := 0
	while true:
		var sum_exp := 100 * level * level * level + 100 * level * level + 100 * level + 1000
		if sum_exp > exp:
			return level
		level += 1
	return 0


func scroll_camera() -> void:
	# Smoothly scroll camera toward player
	var target_x := float(player_x) * GRID_XP - SCREEN_W * 0.5
	var target_y := float(player_y) * GRID_YP - SCREEN_H * 0.5
	var dx := target_x - view_x
	var dy := target_y - view_y
	if abs(dx) > 0.5:
		view_x += sign(dx) * min(abs(dx), 3.0)
	if abs(dy) > 0.5:
		view_y += sign(dy) * min(abs(dy), 2.0)
