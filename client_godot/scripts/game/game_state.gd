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
var player_action_type: int = 2
var player_hp: int = 0
var player_hp_max: int = 0
var player_mp: int = 0
var player_mp_max: int = 0
var player_map_uid: int = 0
var player_map_id: int = 0
var player_map_name: String = ""
var player_desp: Dictionary = {}

# Combat stats
var ac_min: int = 0
var ac_max: int = 0
var dc_min: int = 0
var dc_max: int = 0
var ac_magic: bool = false  # show MA instead of AC
var dc_magic: bool = false  # show MC instead of DC

# Inventory (10x10 grid, 38px cells)
var inventory: Array = []  # compact list of SDItem; the panel computes grid placement
var belt: Array = []  # 6 slots, each is SDItem or null
var wear: Dictionary = {}  # wear location -> SDItem
var grabbed_item: Dictionary = {}
var secured_items: Array = []

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
var runtime_config: Dictionary = {}

# Panel-backed gameplay state
var team_leader: int = 0
var team_members: Array = []
var team_candidates: Array = []
var quests: Dictionary = {}
var npc_dialog: Dictionary = {}
var npc_sell: Dictionary = {}
var pending_input: Dictionary = {}
var firewalls: Array = []

# Camera position (pixel coordinates)
var view_x: float = 0.0
var view_y: float = 0.0

# Strike grids (recently attacked grid cells, for red flash overlay)
var strike_grids: Dictionary = {}  # "x,y" -> timestamp_msec

# Ascend strings (floating damage/heal/exp text)
var ascend_strings: Array = []  # list of {x, y, text, color, start_time}

# System constants
const GRID_XP := 48
const GRID_YP := 32
const SCREEN_W := 800
const SCREEN_H := 600


func _ready() -> void:
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
	player_uid = scene_data.get("uid", player_uid)
	player_map_uid = scene_data.get("mapUID", player_map_uid)
	player_map_id = _map_id_from_uid(player_map_uid)
	player_name = scene_data.get("name", player_name)
	player_x = scene_data.get("x", player_x)
	player_y = scene_data.get("y", player_y)
	player_direction = scene_data.get("direction", player_direction)
	player_desp = scene_data.get("desp", player_desp)
	wear = player_desp.get("wear", wear)
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


func update_item(item: Dictionary) -> void:
	var item_id: int = item.get("itemID", 0)
	var seq_id: int = item.get("seqID", 0)
	for index in range(inventory.size()):
		if inventory[index].get("itemID", 0) == item_id and inventory[index].get("seqID", 0) == seq_id:
			if item.get("count", 0) > 0:
				inventory[index] = item
			else:
				inventory.remove_at(index)
			state_changed.emit()
			return
	if item_id != 0 and item.get("count", 0) > 0:
		inventory.append(item)
		state_changed.emit()


func remove_item(item_id: int, seq_id: int, count: int) -> void:
	for index in range(inventory.size()):
		var item: Dictionary = inventory[index]
		if item.get("itemID", 0) != item_id or item.get("seqID", 0) != seq_id:
			continue
		var remaining: int = maxi(0, item.get("count", 0) - count)
		if remaining == 0:
			inventory.remove_at(index)
		else:
			item["count"] = remaining
			inventory[index] = item
		state_changed.emit()
		return


func update_entity_health(data: Dictionary) -> void:
	var uid: int = data.get("uid", 0)
	if uid == player_uid:
		update_health(data.get("hp", 0), data.get("maxHP", 0), data.get("mp", 0), data.get("maxMP", 0))
		return
	var creature: Dictionary = creatures.get(uid, {})
	if creature.is_empty():
		return
	creature["hp"] = data.get("hp", 0)
	creature["mp"] = data.get("mp", 0)
	creature["hp_max"] = data.get("maxHP", 0)
	creature["mp_max"] = data.get("maxMP", 0)
	creatures[uid] = creature
	state_changed.emit()


func _center_camera_on_player() -> void:
	view_x = float(player_x) * GRID_XP - SCREEN_W * 0.5
	view_y = float(player_y) * GRID_YP - SCREEN_H * 0.5


func _map_id_from_uid(map_uid: int) -> int:
	# UID layout matches uidf::getUID(): the 24-bit record ID starts at bit 35.
	return (map_uid >> 35) & 0xFFFFFF


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


func add_ascend_string(grid_x: int, grid_y: int, text: String, color: Color = Color(1, 0.3, 0.3, 1)) -> void:
	ascend_strings.append({
		"x": grid_x * GRID_XP + GRID_XP / 2,
		"y": grid_y * GRID_YP,
		"text": text,
		"color": color,
		"start_time": Time.get_ticks_msec(),
	})
	if ascend_strings.size() > 50:
		ascend_strings.pop_front()


func update_ascend_strings() -> void:
	var now := Time.get_ticks_msec()
	var to_keep: Array = []
	for s in ascend_strings:
		var age: int = now - s.get("start_time", 0)
		if age < 1500:  # 1.5 seconds lifetime
			to_keep.append(s)
	ascend_strings = to_keep
