extends Control

signal panel_requested(scene_path: String)
signal quick_bar_toggled

const NetworkClient = preload("res://scripts/network/network_client.gd")

@onready var body: Control = %Body
@onready var command: LineEdit = %Command
@onready var compact_middle: NinePatchRect = %CompactMiddle
@onready var expanded_middle: NinePatchRect = %ExpandedMiddle
@onready var chat_log: Label = %ChatLog
@onready var level_label: Label = %Level
@onready var ac_value: Label = %ACValue
@onready var dc_value: Label = %DCValue
@onready var ac_icon: TextureRect = %ACIcon
@onready var dc_icon: TextureRect = %DCIcon
@onready var health_bar: TextureProgressBar = %Health
@onready var mana_bar: TextureProgressBar = %Mana

var game_state: Node = null
var _minimized := false
var _expanded := false
var _ac_magic := false
var _dc_magic := false

# Chat log display
var _chat_lines: Array = []
const CHAT_MAX_LINES := 8


func _ready() -> void:
	game_state = get_node("/root/GameState")
	for button in %BoardButtons.get_children():
		if button is BaseButton:
			button.pressed.connect(
				panel_requested.emit.bind(str(button.get_meta("scene_path"))),
			)


func _process(_delta: float) -> void:
	if game_state == null:
		return
	# Update HP/MP bars
	if game_state.player_hp_max > 0:
		health_bar.value = float(game_state.player_hp) / float(game_state.player_hp_max) * 100.0
	if game_state.player_mp_max > 0:
		mana_bar.value = float(game_state.player_mp) / float(game_state.player_mp_max) * 100.0
	# Update level
	level_label.text = str(game_state.player_level)
	# Update AC/DC
	ac_value.text = "%d-%d" % [game_state.ac_min, game_state.ac_max]
	dc_value.text = "%d-%d" % [game_state.dc_min, game_state.dc_max]
	# Update chat log from game_state
	_update_chat_display()


func _update_chat_display() -> void:
	# Show last N chat lines
	var lines: Array = game_state.chat_log.slice(maxi(0, game_state.chat_log.size() - CHAT_MAX_LINES))
	var text := ""
	for line in lines:
		text += line.get("text", "") + "\n"
	chat_log.text = text.strip_edges(false, true)


func _on_minimize_pressed() -> void:
	_minimized = not _minimized
	body.visible = not _minimized


func _on_quick_pressed() -> void:
	quick_bar_toggled.emit()


func _on_expand_pressed() -> void:
	_expanded = not _expanded
	compact_middle.visible = not _expanded
	expanded_middle.visible = _expanded
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
		pass  # TODO: user command
	elif text.begins_with("$"):
		pass  # TODO: lua command
	else:
		NetworkClient.send_player_say(text)


func _on_ac_pressed() -> void:
	_ac_magic = not _ac_magic
	_update_ac_dc()


func _on_dc_pressed() -> void:
	_dc_magic = not _dc_magic
	_update_ac_dc()


func _update_ac_dc() -> void:
	# C++ toggles AC<->MA (0x46<->0x48) and DC<->MC (0x47<->0x49)
	# TODO: swap textures when available
	pass


func add_log(text: String, log_type: int = 0) -> void:
	if game_state:
		game_state.add_chat_log(text, log_type)
