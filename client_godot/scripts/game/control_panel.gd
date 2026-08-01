extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CombatCalculatorScript = preload("res://scripts/game/combat_calculator.gd")
const AC_TEXTURE := preload("res://assets/ui/game/control_panel/00000046.png")
const DC_TEXTURE := preload("res://assets/ui/game/control_panel/00000047.png")
const MA_TEXTURE := preload("res://assets/ui/game/control_panel/00000048.png")
const MC_TEXTURE := preload("res://assets/ui/game/control_panel/00000049.png")

signal panel_requested(scene_path: String)
signal quick_bar_toggled

@onready var body: Control = %Body
@onready var command: LineEdit = %Command
@onready var compact_middle: NinePatchRect = %CompactMiddle
@onready var expanded_middle: NinePatchRect = %ExpandedMiddle
@onready var chat_background: ColorRect = %ChatBackground
@onready var chat_log: RichTextLabel = %ChatLog
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

var game_state: Node = null
var _minimized := false
var _expanded := false
var _ac_magic := false
var _dc_magic := false
var _resources: RefCounted = ActorResourceScript.new()
var _combat: Dictionary = {}

var _chat_signature := ""


func _ready() -> void:
	game_state = get_node("/root/GameState")
	_resources.configure_default()
	var meter_frame: Dictionary = _resources.frame("proguse", 0x000000A0)
	if not meter_frame.is_empty():
		exp_bar.texture_progress = meter_frame.texture
		load_bar.texture_progress = meter_frame.texture
	game_state.state_changed.connect(_refresh_static)
	for button in %BoardButtons.get_children():
		if button is BaseButton:
			button.pressed.connect(
				panel_requested.emit.bind(str(button.get_meta("scene_path"))),
			)
	var chat_scroll_bar := chat_log.get_v_scroll_bar()
	chat_scroll_bar.modulate = Color.TRANSPARENT
	chat_scroll_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_refresh_static()
	_update_chat_display()


func _process(_delta: float) -> void:
	if game_state == null:
		return
	# Update HP/MP bars
	if game_state.player_hp_max > 0:
		health_bar.value = float(game_state.player_hp) / float(game_state.player_hp_max) * 100.0
	if game_state.player_mp_max > 0:
		mana_bar.value = float(game_state.player_mp) / float(game_state.player_mp_max) * 100.0
	exp_bar.value = game_state.level_ratio() * 100.0
	face_health.size.x = 82.0 * (float(game_state.player_hp) / float(game_state.player_hp_max) if game_state.player_hp_max > 0 else 0.0)
	# Update level
	level_label.text = str(game_state.player_level)
	# Update chat log from game_state
	_update_chat_display()


func _refresh_static() -> void:
	_combat = CombatCalculatorScript.calculate(game_state, _resources)
	_update_ac_dc()
	exp_bar.value = game_state.level_ratio() * 100.0
	var max_load: int = _combat.get("load", PackedInt32Array([0, 0, 0]))[2]
	var current_load := CombatCalculatorScript.inventory_load(game_state, _resources)
	load_bar.value = clampf(float(current_load) / float(max_load) * 100.0, 0.0, 100.0) if max_load > 0 else 0.0
	face_health.size.x = 82.0 * (float(game_state.player_hp) / float(game_state.player_hp_max) if game_state.player_hp_max > 0 else 0.0)
	var job_index := 0 if (game_state.player_job & 1) != 0 else (1 if (game_state.player_job & 2) != 0 else 2)
	var face_id := 0x02000000 + job_index * 2 + (0 if game_state.player_gender else 1)
	var face_frame: Dictionary = _resources.frame("proguse", face_id)
	if not face_frame.is_empty():
		face.texture = face_frame.texture
	for child in buff_container.get_children():
		child.free()
	var draw_count := 0
	for buff_value in game_state.buff_list:
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
		style.border_width_left = 1
		style.border_width_top = 1
		style.border_width_right = 1
		style.border_width_bottom = 1
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
		signature += "%d:%s\n" % [line.get("type", 0), line.get("text", "")]
	if signature == _chat_signature:
		return
	_chat_signature = signature
	chat_log.clear()
	for index in range(game_state.chat_log.size()):
		var line: Dictionary = game_state.chat_log[index]
		chat_log.push_color(line.get("color", Color.WHITE))
		chat_log.add_text(line.get("text", ""))
		chat_log.pop()
		if index + 1 < game_state.chat_log.size():
			chat_log.newline()
	chat_log.scroll_to_line(maxi(0, chat_log.get_line_count() - 1))


func _on_minimize_pressed() -> void:
	_minimized = not _minimized
	body.visible = not _minimized


func _on_quick_pressed() -> void:
	quick_bar_toggled.emit()


func _on_expand_pressed() -> void:
	_expanded = not _expanded
	compact_middle.visible = not _expanded
	expanded_middle.visible = _expanded
	chat_background.offset_top = -269.0 if _expanded else 19.0
	chat_background.color.a = 220.0 / 255.0 if _expanded else 1.0
	chat_log.offset_top = -220.0 if _expanded else 34.0


func _on_minimap_pressed() -> void:
	panel_requested.emit("res://scenes/game/panels/minimap.tscn")


func _on_exit_pressed() -> void:
	get_tree().quit()


func _on_command_submitted(text: String) -> void:
	# C++ submitCommand: ! prefix = broadcast, @ = user command, $ = lua command, else = chat
	command.clear()
	if text.is_empty():
		return
	if text.begins_with("!"):
		NetworkClient.send_player_broadcast(text.substr(1))
	elif text.begins_with("@"):
		var user_command := text.substr(1).strip_edges()
		if user_command == "help":
			game_state.add_chat_log("可用命令：@help", 1)
		else:
			game_state.add_chat_log("无效的本地命令：%s" % user_command, 3)
	elif text.begins_with("$"):
		game_state.add_chat_log("Godot 客户端不提供本地 Lua 执行环境", 3)
	else:
		NetworkClient.send_player_say(text)


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
