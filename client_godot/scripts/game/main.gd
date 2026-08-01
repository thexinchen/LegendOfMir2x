extends Control

# Game main scene - corresponds to ProcessRun in C++ client
# Manages: world rendering, HUD, input, network message handling

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")

@onready var world_renderer: Control = $WorldRenderer
@onready var inventory_panel: Control = %InventoryPanel
@onready var player_state_panel: Control = %PlayerStatePanel
@onready var skill_panel: Control = %SkillPanel
@onready var quick_bar: Control = %QuickBar
@onready var location_label: Label = $Location
@onready var control_panel: Control = $ControlPanel

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
	KEY_M: "res://scenes/game/panels/minimap.tscn",
}

var _extra_panel_nodes: Dictionary = {}
var _pending_purchase: Dictionary = {}


func _ready() -> void:
	game_state = get_node("/root/GameState")
	protocol = Protocol.new()
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
	
	if OS.has_environment("MIR2X_GAME_SCREENSHOT"):
		inventory_panel.show()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GAME_SCREENSHOT"))
		get_tree().quit()


func _process(delta: float) -> void:
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
	world_renderer.queue_redraw()


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
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
	
	# Mouse click handling
	if event is InputEventMouseButton and event.pressed:
		_handle_mouse_click(event)


func _handle_mouse_click(event: InputEventMouseButton) -> void:
	var grid: Vector2i = world_renderer.grid_from_screen(int(event.position.x), int(event.position.y))
	
	if event.button_index == MOUSE_BUTTON_RIGHT:
		# Right click: move toward grid (C++ emplaces ActionMove)
		_send_move_action(grid.x, grid.y)
	elif event.button_index == MOUSE_BUTTON_LEFT:
		# Left click: check for creatures/items at grid
		# C++: if monster -> attack, if NPC -> interact, if ground item -> pickup
		var found_creature := false
		for uid in game_state.creatures:
			var c: Dictionary = game_state.creatures[uid]
			var cx: int = c.get("x", -1)
			var cy: int = c.get("y", -1)
			if cx == grid.x and cy == grid.y:
				found_creature = true
				if c.get("type", 0) == 3:
					NetworkClient.send_npc_event(uid, "", "_RSVD_NAME_ENTER_90360178872")
				elif c.get("type", 0) == 1:
					_send_attack_action(uid)
				break
		if not found_creature:
			var ground_key := "%d,%d" % [grid.x, grid.y]
			if game_state.ground_items.has(ground_key):
				if grid == Vector2i(game_state.player_x, game_state.player_y):
					_request_pickup()
				else:
					_send_move_action(grid.x, grid.y)


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
	}
	var action_data := Protocol.encode_cm_action(game_state.player_uid, game_state.player_map_uid, action)
	NetworkClient.send_action(action_data)


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
	game_state.view_x = float(game_state.player_x) * 48 - 400
	game_state.view_y = float(game_state.player_y) * 32 - 300


func _request_pickup() -> void:
	NetworkClient.send_pickup(game_state.player_x, game_state.player_y, game_state.player_map_uid)


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_STARTGAMESCENE:
			_handle_start_game_scene(payload)
		NetworkClient.SM_ACTION:
			_handle_action(payload)
		NetworkClient.SM_HEALTH:
			_handle_health(payload)
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
			var uid := Protocol.decode_sm_notify_dead(payload)
			if uid == game_state.player_uid:
				game_state.add_chat_log("你已死亡", 3)
		NetworkClient.SM_OFFLINE:
			var data := Protocol.decode_sm_offline(payload)
			game_state.remove_creature(data.get("uid", 0))
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
			var uid := _decode_u64_payload(payload, 0)
			game_state.remove_creature(uid)
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
		NetworkClient.SM_GRABWEAR:
			_handle_grab_wear(payload)
		NetworkClient.SM_EQUIPBELT:
			_handle_equip_belt(payload)
		NetworkClient.SM_GRABBELT:
			_handle_grab_belt(payload)
		NetworkClient.SM_UPDATEITEM:
			var reader := CerealReader.new(payload)
			var item := reader.read_sd_update_item()
			if _reader_ok(reader, "SM_UPDATEITEM"):
				game_state.update_item(item)
		NetworkClient.SM_SHOWSECUREDITEMLIST:
			_handle_secured_items(payload)
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


func _handle_start_game_scene(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_start_game_scene()
	if not _reader_ok(reader, "SM_STARTGAMESCENE"):
		return
	if data.get("uid", 0) != game_state.player_uid or data.get("mapUID", 0) != game_state.player_map_uid:
		push_error("SM_STARTGAMESCENE identity mismatch")
		return
	game_state.start_game_scene(data)
	world_renderer.load_map(game_state.player_map_id)
	_center_hero()


func _handle_action(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_action(payload)
	var uid: int = data.get("uid", 0)
	var action: Dictionary = data.get("action", {})
	var x: int = action.get("x", 0)
	var y: int = action.get("y", 0)
	var action_type: int = action.get("type", 0)
	var direction: int = action.get("direction", 0)
	
	if uid == game_state.player_uid:
		# Update player position and direction
		game_state.player_x = x
		game_state.player_y = y
		game_state.player_action_type = action_type
		if direction >= 1:
			game_state.player_direction = direction
	else:
		# Update or create creature
		var creature: Dictionary = game_state.get_creature(uid)
		if creature.is_empty():
			var inferred_type := _creature_type_from_uid(uid)
			creature = {
				"uid": uid,
				"x": x,
				"y": y,
				"type": inferred_type,
				"name": "",
				"action_type": action_type,
				"direction": direction if direction >= 1 else (1 if inferred_type == 3 else _direction_to(x, y, action.get("aimX", x), action.get("aimY", y))),
			}
			if inferred_type == 1:
				creature["monster_id"] = (uid >> 35) & 0xFFFFFF
			elif inferred_type == 3:
				creature["npc_id"] = (uid >> 35) & 0xFFFFFF
		else:
			creature["x"] = x
			creature["y"] = y
			creature["action_type"] = action_type
			if direction >= 1:
				creature["direction"] = direction
		game_state.update_creature(uid, creature)


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
			"x": action.get("x", 0),
			"y": action.get("y", 0),
			"type": c_type,
			"name": "",
			"action_type": action.get("type", 0),
			"direction": action.get("direction", 0),
		}
	else:
		creature["x"] = action.get("x", 0)
		creature["y"] = action.get("y", 0)
		creature["type"] = c_type
		creature["action_type"] = action.get("type", 0)
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
	var name := "未知"
	if uid == game_state.player_uid:
		name = game_state.player_name
	else:
		var c: Dictionary = game_state.get_creature(uid)
		if not c.get("name", "").is_empty():
			name = c.get("name")
	game_state.add_chat_log("%s: %s" % [name, content], 0)


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
	var uid: int = data.get("uid", 0)
	var magic_id: int = data.get("magic", 0)
	var x: int = data.get("x", 0)
	var y: int = data.get("y", 0)
	# Show magic effect at target location
	game_state.add_ascend_string(x, y, "*", Color(0.5, 0.8, 1, 1))


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
		game_state.grabbed_item = data.get("item", {})
		game_state.state_changed.emit()


func _handle_equip_belt(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_equip_belt()
	if not _reader_ok(reader, "SM_EQUIPBELT"):
		return
	var slot: int = data.get("slot", -1)
	if slot >= 0 and slot < game_state.belt.size():
		game_state.belt[slot] = data.get("item", {})
		game_state.state_changed.emit()


func _handle_grab_belt(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_grab_belt()
	if not _reader_ok(reader, "SM_GRABBELT"):
		return
	var slot: int = data.get("slot", -1)
	if slot >= 0 and slot < game_state.belt.size():
		game_state.belt[slot] = {}
		game_state.grabbed_item = data.get("item", {})
		game_state.state_changed.emit()


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
		game_state.team_candidates.append(candidate)
		game_state.state_changed.emit()


func _handle_team_members(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var team := reader.read_sd_team_member_list()
	if _reader_ok(reader, "SM_TEAMMEMBERLIST"):
		game_state.team_leader = team.get("teamLeader", 0)
		game_state.team_members = team.get("members", [])
		game_state.state_changed.emit()


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
	panel.position = (size - panel.size) * 0.5
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
