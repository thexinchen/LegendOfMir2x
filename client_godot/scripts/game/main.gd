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


func _ready() -> void:
	game_state = get_node("/root/GameState")
	protocol = Protocol.new()
	world_renderer.game_state = game_state
	world_renderer.protocol = protocol
	
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
	var map_name := game_state.player_map_name
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
	var grid := world_renderer.grid_from_screen(int(event.position.x), int(event.position.y))
	
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
				_send_attack_action(uid)
				break
		if not found_creature:
			# Check for ground items or pickup
			_request_pickup()


func _send_move_action(aim_x: int, aim_y: int) -> void:
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
	var creature := game_state.get_creature(target_uid)
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
	var dx := to_x - from_x
	var dy := to_y - from_y
	if dx == 0 and dy == 0:
		return 0
	if abs(dx) > abs(dy):
		return 3 if dx > 0 else 7  # right : left
	else:
		return 4 if dy > 0 else 1  # down : up


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
			pass  # TODO: update buff
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
			pass  # TODO: remove from inventory
		NetworkClient.SM_REMOVEGROUNDITEM:
			pass  # TODO: remove ground item
		NetworkClient.SM_EQUIPWEAR, NetworkClient.SM_GRABWEAR, NetworkClient.SM_EQUIPBELT, NetworkClient.SM_GRABBELT:
			pass  # TODO: update equipment
		NetworkClient.SM_UPDATEITEM:
			pass  # TODO: update item
		NetworkClient.SM_TEAMCANDIDATE, NetworkClient.SM_TEAMMEMBERLIST:
			pass  # TODO: team
		NetworkClient.SM_QUESTDESPLIST, NetworkClient.SM_QUESTDESPUPDATE:
			pass  # TODO: quest
		NetworkClient.SM_LEARNEDMAGICLIST:
			pass  # TODO: magic list


func _handle_start_game_scene(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_start_game_scene()
	game_state.start_game_scene(data)
	game_state.player_name = data.get("name", game_state.player_name)
	game_state.player_x = data.get("x", game_state.player_x)
	game_state.player_y = data.get("y", game_state.player_y)
	game_state.player_direction = data.get("direction", game_state.player_direction)
	# Set placeholder map size based on known map dimensions
	# C++ loads from mapbin.zsdb, Godot uses placeholder for now
	world_renderer.set_map_data(100, 100)
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
		game_state.player_direction = direction
	else:
		# Update or create creature
		var creature: Dictionary = game_state.get_creature(uid)
		if creature.is_empty():
			creature = {
				"uid": uid,
				"x": x,
				"y": y,
				"type": 0,
				"name": "",
				"action_type": action_type,
				"direction": direction,
			}
		else:
			creature["x"] = x
			creature["y"] = y
			creature["action_type"] = action_type
			creature["direction"] = direction
		game_state.update_creature(uid, creature)


func _handle_corecord(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_corecord(payload)
	var uid: int = data.get("uid", 0)
	var action: Dictionary = data.get("action", {})
	
	# Determine creature type from UID type bits
	# UID type is in bits 59-62 (4 bits at offset 59)
	# UID_NPC=3, UID_MON=4, UID_PLY=5
	var uid_type: int = (uid >> 59) & 0xF
	var c_type: int = 0
	match uid_type:
		3: c_type = 3  # NPC
		4: c_type = 1  # Monster
		5: c_type = 2  # Player
		_: c_type = 0
	
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
				creature["level"] = union_data.decode_u32(4) if union_data.size() >= 8 else 0
			3:  # NPC: NPCID (u32)
				creature["npc_id"] = union_data.decode_u32(0)
	
	game_state.update_creature(uid, creature)


func _handle_health(payload: PackedByteArray) -> void:
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_health()
	game_state.update_health(data.get("hp", 0), data.get("hpMax", 0), data.get("mp", 0), data.get("mpMax", 0))


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
		var c := game_state.get_creature(uid)
		if not c.get("name", "").is_empty():
			name = c.get("name")
	game_state.add_chat_log("%s: %s" % [name, content], 0)


func _handle_player_broadcast(payload: PackedByteArray) -> void:
	var data := Protocol.decode_sm_player_broadcast(payload)
	var content: String = data.get("content", "")
	game_state.add_chat_log("[广播] %s" % content, 2)


func _handle_inventory(payload: PackedByteArray) -> void:
	# TODO: parse cereal SDItemStorage
	pass


func _handle_belt(payload: PackedByteArray) -> void:
	# TODO: parse cereal SDBelt
	pass


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
	var reader := CerealReader.new(payload)
	var data := reader.read_sd_player_name()
	var uid: int = data.get("uid", 0)
	var name: String = data.get("name", "")
	if not name.is_empty():
		var c := game_state.get_creature(uid)
		if not c.is_empty():
			c["name"] = name
			c["name_color"] = data.get("nameColor", 0xFFFFFFFF)
			game_state.update_creature(uid, c)


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
	var panel := _extra_panel_nodes.get(scene_path) as Control
	if not panel:
		var packed := load(scene_path) as PackedScene
		if not packed:
			return
		panel = packed.instantiate() as Control
		add_child(panel)
		panel.position = (size - panel.size) * 0.5
		_extra_panel_nodes[scene_path] = panel
		return
	_toggle_panel(panel)
