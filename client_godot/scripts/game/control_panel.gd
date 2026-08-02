extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")
const AC_TEXTURE := preload("res://assets/ui/game/control_panel/00000046.png")
const DC_TEXTURE := preload("res://assets/ui/game/control_panel/00000047.png")
const MA_TEXTURE := preload("res://assets/ui/game/control_panel/00000048.png")
const MC_TEXTURE := preload("res://assets/ui/game/control_panel/00000049.png")
const USER_COMMANDS := [
	"moveTo", "luaEditor", "makeItem", "getAttackUID", "addHP",
	"addExp", "killPets", "die", "revive", "help",
]

signal panel_requested(scene_path: String)
signal quick_bar_toggled
signal magic_key_hud_toggled
signal minimized_changed(minimized: bool)

@onready var body: Control = %Body
@onready var command: LineEdit = %Command
@onready var compact_middle: NinePatchRect = %CompactMiddle
@onready var expanded_middle: NinePatchRect = %ExpandedMiddle
@onready var chat_background: ColorRect = %ChatBackground
@onready var chat_log: RichTextLabel = %ChatLog
@onready var chat_slider_hit_area: Control = $Body/ChatSliderHitArea
@onready var chat_slider: TextureRect = $Body/ChatSlider
@onready var level_label: Label = %Level
@onready var ac_value: Label = %ACValue
@onready var dc_value: Label = %DCValue
@onready var ac_icon: TextureRect = %ACIcon
@onready var dc_icon: TextureRect = %DCIcon
@onready var health_bar: TextureProgressBar = %Health
@onready var mana_bar: TextureProgressBar = %Mana
@onready var exp_bar: TextureProgressBar = %Experience
@onready var load_bar: TextureProgressBar = %Load
@onready var face: TextureRect = %Face
@onready var face_health: ColorRect = %FaceHealth
@onready var buff_container: Control = %BuffContainer
@onready var title: TextureRect = $Title
@onready var title_arc_current: TextureRect = %ArcCurrent
@onready var title_arc_next: TextureRect = %ArcNext
@onready var minimize_button: TextureButton = $MinimizeButton
@onready var expand_button: TextureButton = $Body/ExpandButton
@onready var emoji_button: TextureButton = $Body/EmojiButton
@onready var mute_button: TextureButton = $Body/MuteButton

var game_state: Node = null
var _minimized := false
var _expanded := false
var _ac_magic := false
var _dc_magic := false
var _resources: RefCounted = ActorResourceScript.new()
var _combat: Dictionary = {}

var _chat_signature := ""
var _focus_hud_signature := ""
var _button_blinks: Dictionary = {}
var _chat_slider_dragging := false
var _title_arc_frames: Array[Texture2D] = []
var _title_arc_elapsed_ms := 0.0


func _ready() -> void:
	game_state = get_node("/root/GameState")
	_resources.configure_default()
	for frame_index in range(4):
		var frame: Dictionary = _resources.frame("proguse", 0x04000000 + frame_index)
		if not frame.is_empty():
			_title_arc_frames.append(frame.texture)
	_update_title_arc(0.0)
	var meter_frame: Dictionary = _resources.frame("proguse", 0x000000A0)
	if not meter_frame.is_empty():
		exp_bar.texture_progress = meter_frame.texture
		load_bar.texture_progress = meter_frame.texture
	game_state.state_changed.connect(_refresh_static)
	for button in %BoardButtons.get_children():
		if button is BaseButton:
			button.pressed.connect(_on_board_button_pressed.bind(button))
			button.pressed.connect(AudioService.play_ui_click)
	for button_path in [
		"Body/QuickButton", "Body/ExitButton", "Body/Exchange", "Body/MiniMap",
		"Body/MagicKey", "Body/ExpandButton", "Body/EmojiButton", "Body/MuteButton",
		"MinimizeButton",
	]:
		var button := get_node(button_path) as TextureButton
		button.pressed.connect(AudioService.play_ui_click)
		_bind_overlay_button(button)
	var chat_scroll_bar := chat_log.get_v_scroll_bar()
	chat_scroll_bar.modulate = Color.TRANSPARENT
	chat_scroll_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	chat_log.gui_input.connect(_on_chat_input)
	chat_slider_hit_area.gui_input.connect(_on_chat_slider_input)
	_refresh_static()
	_update_chat_display()
	$Body/MagicKey.pressed.connect(magic_key_hud_toggled.emit)
	title.gui_input.connect(_on_title_gui_input)
	_update_button_blinks()
	_update_chat_slider()


func _bind_overlay_button(button: TextureButton) -> void:
	button.set_meta("overlay_hovered", false)
	button.set_meta("overlay_pressed", false)
	button.modulate.a = 0.0
	button.mouse_entered.connect(_set_overlay_hovered.bind(button, true))
	button.mouse_exited.connect(_set_overlay_hovered.bind(button, false))
	button.button_down.connect(_set_overlay_pressed.bind(button, true))
	button.button_up.connect(_set_overlay_pressed.bind(button, false))


func _set_overlay_hovered(button: TextureButton, hovered: bool) -> void:
	button.set_meta("overlay_hovered", hovered)
	_update_overlay_alpha(button)


func _set_overlay_pressed(button: TextureButton, pressed: bool) -> void:
	button.set_meta("overlay_pressed", pressed)
	_update_overlay_alpha(button)


func _update_overlay_alpha(button: TextureButton) -> void:
	button.modulate.a = 1.0 if button.get_meta("overlay_hovered", false) or button.get_meta("overlay_pressed", false) else 0.0


func start_button_blink(button_name: String, duration_ms := 5000) -> void:
	_button_blinks[button_name] = Time.get_ticks_msec() + duration_ms


func stop_button_blink(button_name: String) -> void:
	_button_blinks.erase(button_name)
	var button := %BoardButtons.get_node_or_null(button_name) as BaseButton
	if button:
		button.modulate.a = 1.0


func _update_button_blinks() -> void:
	var now := Time.get_ticks_msec()
	for button_name in _button_blinks.keys():
		var button := %BoardButtons.get_node_or_null(button_name) as BaseButton
		if now >= int(_button_blinks[button_name]):
			stop_button_blink(button_name)
		elif button:
			button.modulate.a = 1.0 if now % 200 < 100 else 0.0


func _on_board_button_pressed(button: BaseButton) -> void:
	stop_button_blink(button.name)
	panel_requested.emit(str(button.get_meta("scene_path")))


func _process(_delta: float) -> void:
	if game_state == null:
		return
	_title_arc_elapsed_ms += _delta * 1000.0
	_update_title_arc(_title_arc_elapsed_ms)
	# Update HP/MP bars
	if game_state.player_hp_max > 0:
		health_bar.value = float(game_state.player_hp) / float(game_state.player_hp_max) * 100.0
	if game_state.player_mp_max > 0:
		mana_bar.value = float(game_state.player_mp) / float(game_state.player_mp_max) * 100.0
	exp_bar.value = game_state.level_ratio() * 100.0
	_refresh_focus_hud()
	# Update level
	level_label.text = str(game_state.player_level)
	# Update chat log from game_state
	_update_chat_display()
	_update_button_blinks()
	_update_chat_slider()


func _update_title_arc(elapsed_ms: float) -> void:
	if _title_arc_frames.size() != 4:
		title_arc_current.hide()
		title_arc_next.hide()
		return
	var decimal_frame := maxf(elapsed_ms, 0.0) / 1000.0
	var current_frame := floori(decimal_frame) % 4
	var next_frame := (current_frame + 1) % 4
	var alpha_byte := clampi(roundi(255.0 * (decimal_frame - floorf(decimal_frame))), 0, 255)
	title_arc_current.texture = _title_arc_frames[current_frame]
	title_arc_next.texture = _title_arc_frames[next_frame]
	title_arc_current.modulate.a = float(255 - alpha_byte) / 255.0
	title_arc_next.modulate.a = float(alpha_byte) / 255.0
	title_arc_current.show()
	title_arc_next.show()


func _refresh_static() -> void:
	_combat = CombatCalculatorScript.calculate(game_state, _resources)
	_update_ac_dc()
	exp_bar.value = game_state.level_ratio() * 100.0
	var max_load: int = _combat.get("load", PackedInt32Array([0, 0, 0]))[2]
	var current_load := CombatCalculatorScript.inventory_load(game_state, _resources)
	load_bar.value = clampf(float(current_load) / float(max_load) * 100.0, 0.0, 100.0) if max_load > 0 else 0.0
	_focus_hud_signature = ""
	_refresh_focus_hud()


func _refresh_focus_hud() -> void:
	var focus_creature: Dictionary = _mouse_focus_creature()
	_apply_focus_hud(focus_creature)


func _mouse_focus_creature() -> Dictionary:
	var renderer := get_parent().get_node_or_null("WorldRenderer") if get_parent() else null
	if renderer == null or not renderer.has_method("focus_uid_at_screen"):
		return {}
	var focus_uid: int = renderer.focus_uid_at_screen(get_viewport().get_mouse_position())
	var creature: Dictionary = game_state.get_creature(focus_uid)
	return creature if creature.get("type", 0) in [1, 2] else {}


func _apply_focus_hud(focus_creature: Dictionary) -> void:
	var hud_creature := focus_creature
	if hud_creature.is_empty():
		hud_creature = {
			"uid": game_state.player_uid,
			"type": 2,
			"gender": game_state.player_gender,
			"job": game_state.player_job,
			"hp": game_state.player_hp,
			"hp_max": game_state.player_hp_max,
			"buffs": game_state.buff_list,
		}
	var buffs: Array = hud_creature.get("buffs", [])
	var signature := "%s:%s:%s:%s:%s:%s:%s:%s" % [hud_creature.get("uid", 0), hud_creature.get("type", 0), hud_creature.get("gender", 0), hud_creature.get("job", 0), hud_creature.get("monster_id", 0), hud_creature.get("hp", 0), hud_creature.get("hp_max", 0), buffs]
	if signature == _focus_hud_signature:
		return
	_focus_hud_signature = signature
	var hp_max: int = hud_creature.get("hp_max", 0)
	face_health.size.x = 82.0 * clampf(float(hud_creature.get("hp", 0)) / float(hp_max), 0.0, 1.0) if hp_max > 0 else 0.0
	var face_frame: Dictionary = {}
	if hud_creature.get("type", 0) == 1:
		var look_id: int = _resources.monster_look(hud_creature.get("monster_id", 0))
		face_frame = _resources.frame("proguse", 0x01000000 + look_id)
	else:
		var job: int = hud_creature.get("job", 0)
		var job_index := 0 if (job & 1) != 0 else (1 if (job & 2) != 0 else 2)
		var face_id := 0x02000000 + job_index * 2 + (0 if hud_creature.get("gender", 0) else 1)
		face_frame = _resources.frame("proguse", face_id)
	if face_frame.is_empty():
		face_frame = _resources.frame("proguse", 0x010007CF)
	face.texture = face_frame.get("texture")
	for child in buff_container.get_children():
		buff_container.remove_child(child)
		child.queue_free()
	var draw_count := 0
	for buff_value in buffs:
		var buff_id: int = buff_value if buff_value is int else buff_value.get("id", 0)
		var layout: PackedInt32Array = _resources.buff_layout(buff_id)
		if layout.size() != 2:
			continue
		var icon: Dictionary = _resources.frame("proguse", layout[0])
		if icon.is_empty():
			continue
		var panel := Panel.new()
		panel.position = Vector2((draw_count % 5) * 16, 79 - (draw_count / 5) * 16)
		panel.size = Vector2(16, 16)
		panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var style := StyleBoxFlat.new()
		style.bg_color = Color.TRANSPARENT
		style.set_border_width_all(1)
		style.border_color = Color.GREEN if layout[1] > 0 else (Color.YELLOW if layout[1] == 0 else Color.RED)
		panel.add_theme_stylebox_override("panel", style)
		var image := TextureRect.new()
		image.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		image.texture = icon.texture
		image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		image.mouse_filter = Control.MOUSE_FILTER_IGNORE
		panel.add_child(image)
		buff_container.add_child(panel)
		draw_count += 1


func _update_chat_display() -> void:
	var signature := ""
	for line in game_state.chat_log:
		signature += "%d:%s:%s\n" % [line.get("type", 0), line.get("text", ""), line.get("background_color", Color.TRANSPARENT).to_html()]
	if signature == _chat_signature:
		return
	_chat_signature = signature
	chat_log.clear()
	for index in range(game_state.chat_log.size()):
		var line: Dictionary = game_state.chat_log[index]
		var background: Color = line.get("background_color", Color.TRANSPARENT)
		if background.a > 0.0:
			chat_log.push_bgcolor(background)
		chat_log.push_color(line.get("color", Color.WHITE))
		chat_log.add_text(line.get("text", ""))
		chat_log.pop()
		if background.a > 0.0:
			chat_log.pop()
		if index + 1 < game_state.chat_log.size():
			chat_log.newline()
	chat_log.scroll_to_line(maxi(0, chat_log.get_line_count() - 1))
	chat_log.scroll_following = true
	call_deferred("_update_chat_slider")


func _on_minimize_pressed() -> void:
	_set_minimized(not _minimized)


func _set_minimized(minimized: bool) -> void:
	if _minimized == minimized:
		return
	_minimized = minimized
	body.visible = not minimized
	title.position.y = 121.0 if minimized else 0.0
	level_label.visible = not minimized
	minimize_button.visible = not minimized
	minimized_changed.emit(minimized)


func _on_title_gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed and event.double_click:
		_set_minimized(not _minimized)
		accept_event()


func _on_quick_pressed() -> void:
	quick_bar_toggled.emit()


func _on_expand_pressed() -> void:
	_expanded = not _expanded
	compact_middle.visible = not _expanded
	expanded_middle.visible = _expanded
	face.visible = not _expanded
	face_health.visible = not _expanded
	buff_container.visible = not _expanded
	chat_background.offset_top = -269.0 if _expanded else 19.0
	chat_background.color.a = 220.0 / 255.0 if _expanded else 1.0
	chat_log.offset_top = -220.0 if _expanded else 34.0
	expand_button.position.y = -266.0 if _expanded else 22.0
	emoji_button.visible = _expanded
	mute_button.visible = _expanded
	_update_chat_slider()


func _chat_scroll_reach() -> float:
	var scroll_bar := chat_log.get_v_scroll_bar()
	return maxf(0.0, scroll_bar.max_value - scroll_bar.page)


func _chat_scroll_normalized() -> float:
	var reach := _chat_scroll_reach()
	return clampf(chat_log.get_v_scroll_bar().value / reach, 0.0, 1.0) if reach > 0.0 else 1.0


func _set_chat_scroll_normalized(value: float) -> void:
	var normalized := clampf(value, 0.0, 1.0)
	chat_log.scroll_following = normalized >= 1.0
	chat_log.get_v_scroll_bar().value = normalized * _chat_scroll_reach()
	_update_chat_slider()


func _on_chat_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed or event.button_index not in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
		return
	var reach := _chat_scroll_reach()
	if reach <= 0.0:
		return
	var direction := -1.0 if event.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0
	var factor: float = event.factor if event.factor > 0.0 else 1.0
	_set_chat_scroll_normalized(_chat_scroll_normalized() + direction * factor * 45.0 / reach)
	accept_event()


func _on_chat_slider_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_chat_slider_dragging = event.pressed
		if event.pressed:
			_set_chat_slider_from_hit_y(event.position.y)
		_update_chat_slider()
		accept_event()
	elif event is InputEventMouseMotion and _chat_slider_dragging:
		_set_chat_slider_from_hit_y(event.position.y)
		accept_event()


func _set_chat_slider_from_hit_y(hit_y: float) -> void:
	var travel := 324.0 if _expanded else 59.0
	_set_chat_scroll_normalized((hit_y - 10.0) / travel)


func _update_chat_slider() -> void:
	var normalized := _chat_scroll_normalized()
	if _expanded:
		chat_slider_hit_area.position = Vector2(615, -218)
		chat_slider_hit_area.size = Vector2(18, 345)
		chat_slider.position = Vector2(619.5, -216 + normalized * 324.0)
	else:
		chat_slider_hit_area.position = Vector2(615, 49)
		chat_slider_hit_area.size = Vector2(18, 80)
		chat_slider.position = Vector2(619.5, 51 + normalized * 59.0)
	chat_slider.modulate = Color.WHITE if _chat_slider_dragging else Color(0.5, 0.5, 0.5, 1.0)


func _on_minimap_pressed() -> void:
	panel_requested.emit("res://scenes/game/panels/minimap.tscn")


func _on_exit_pressed() -> void:
	get_tree().quit()


func _on_exchange_pressed() -> void:
	add_log("exchange doesn't implemented yet", 0)


func focus_command() -> void:
	command.grab_focus()
	command.caret_column = command.text.length()


func _on_command_submitted(text: String) -> void:
	# C++ submitCommand: ! prefix = broadcast, @ = user command, $ = lua command, else = chat
	command.clear()
	command.release_focus()
	var full_text := text.strip_edges(true, false)
	if full_text.is_empty():
		return
	if full_text.begins_with("!"):
		var content := full_text.substr(1).strip_edges(true, false)
		if not content.is_empty():
			NetworkClient.send_player_broadcast(content)
	elif full_text.begins_with("@"):
		_handle_user_command(full_text.substr(1))
	elif full_text.begins_with("$"):
		game_state.add_chat_log("Godot 客户端不提供本地 Lua 执行环境", 3)
	else:
		game_state.add_chat_log(full_text, 0)
		NetworkClient.send_player_say(full_text)


func _handle_user_command(command_text: String) -> void:
	var tokens := Array(command_text.split(" ", false))
	if tokens.is_empty():
		return
	var command_prefix := str(tokens[0])
	var matches: Array[String] = []
	for command in USER_COMMANDS:
		if command.begins_with(command_prefix):
			matches.append(command)
	if matches.is_empty():
		game_state.add_chat_log("-> 无效的用户命令：%s" % command_prefix, 3)
		return
	if matches.size() > 1:
		game_state.add_chat_log(">> 用户命令有歧义：%s" % command_prefix, 3)
		for candidate in matches:
			game_state.add_chat_log(">> 候选命令：%s" % candidate, 3)
		return
	match matches[0]:
		"moveTo": _command_move_to(tokens)
		"luaEditor": game_state.add_chat_log(">> Lua 编辑器尚未实现", 3)
		"makeItem": _command_make_item(tokens)
		"getAttackUID": game_state.add_chat_log(str(_attack_focus_uid()), 3)
		"addHP": _command_add_value(tokens, true)
		"addExp": _command_add_value(tokens, false)
		"killPets":
			NetworkClient.send_request_kill_pets()
			game_state.add_chat_log("杀死所有宝宝", 1)
		"die":
			NetworkClient.send_request_die()
			game_state.add_chat_log("自杀", 1)
		"revive":
			if game_state.player_action_type == 13:
				NetworkClient.send_request_add_hp(1)
				game_state.add_chat_log("复活", 1)
		"help":
			for command in USER_COMMANDS:
				game_state.add_chat_log("@%s" % command, 1)


func _command_move_to(tokens: Array) -> void:
	if tokens.size() != 3 or not str(tokens[1]).is_valid_int() or not str(tokens[2]).is_valid_int():
		game_state.add_chat_log("用法：@moveTo X Y", 3)
		return
	var destination := Vector2i(int(tokens[1]), int(tokens[2]))
	var main := get_parent()
	if main == null or not main.has_method("_start_move_to"):
		game_state.add_chat_log("当前场景无法移动", 3)
		return
	main.call("_start_move_to", destination)


func _command_make_item(tokens: Array) -> void:
	if tokens.size() < 2 or tokens.size() > 3:
		game_state.add_chat_log("用法：@makeItem 物品名字 [数量]", 1)
		return
	var item_name := str(tokens[1])
	var item_id := 0
	for id_value in _resources.item_names:
		if str(_resources.item_names[id_value]) == item_name:
			item_id = int(id_value)
			break
	if item_id <= 0:
		game_state.add_chat_log("无效的物品名：%s" % item_name, 3)
		return
	var count := 1
	if tokens.size() == 3:
		if not str(tokens[2]).is_valid_int():
			game_state.add_chat_log("无效的物品数量：%s" % tokens[2], 3)
			return
		count = int(tokens[2])
	if count <= 0 or count > 0xFFFF:
		game_state.add_chat_log("无效的物品数量：%s" % count, 3)
		return
	NetworkClient.send_make_item(item_id, count)


func _command_add_value(tokens: Array, health: bool) -> void:
	var command_name := "addHP" if health else "addExp"
	if tokens.size() != 2 or not str(tokens[1]).is_valid_int():
		game_state.add_chat_log("用法：@%s 数量" % command_name, 3)
		return
	var value := int(tokens[1])
	if value <= 0:
		game_state.add_chat_log("无效的数量：%s" % tokens[1], 3)
		return
	if health:
		NetworkClient.send_request_add_hp(value)
	else:
		NetworkClient.send_request_add_exp(value)


func _attack_focus_uid() -> int:
	var main := get_parent()
	return int(main.get("_attack_focus_uid")) if main != null and main.has_method("_send_attack_action") else 0


func _on_ac_pressed() -> void:
	_ac_magic = not _ac_magic
	_update_ac_dc()


func _on_dc_pressed() -> void:
	_dc_magic = not _dc_magic
	_update_ac_dc()


func _update_ac_dc() -> void:
	ac_icon.texture = MA_TEXTURE if _ac_magic else AC_TEXTURE
	dc_icon.texture = MC_TEXTURE if _dc_magic else DC_TEXTURE
	if _combat.is_empty():
		return
	var ac_pair: PackedInt32Array = _combat.mac if _ac_magic else _combat.ac
	var dc_pair: PackedInt32Array = _combat.mc if _dc_magic else _combat.dc
	ac_value.text = "%d-%d" % [ac_pair[0], ac_pair[1]]
	dc_value.text = "%d-%d" % [dc_pair[0], dc_pair[1]]


func add_log(text: String, log_type: int = 0) -> void:
	if game_state:
		game_state.add_chat_log(text, log_type)
