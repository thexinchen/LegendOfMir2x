extends Control

signal panel_requested(scene_path: String)
signal quick_bar_toggled

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

var _minimized := false
var _expanded := false
var _ac_magic := false
var _dc_magic := false


func _ready() -> void:
	for button in %BoardButtons.get_children():
		if button is BaseButton:
			button.pressed.connect(
				panel_requested.emit.bind(str(button.get_meta("scene_path"))),
			)


func _on_minimize_pressed() -> void:
	_minimized = not _minimized
	body.visible = not _minimized


func _on_quick_pressed() -> void:
	quick_bar_toggled.emit()


func _on_expand_pressed() -> void:
	_expanded = not _expanded
	compact_middle.visible = not _expanded
	expanded_middle.visible = _expanded
	# C++ expanded: panel height up to 421, log width = middleW - 24, log height = expandedPanelH - 70
	# Compact: log width = middleW - 112, log height = 84
	chat_log.offset_top = -220.0 if _expanded else 34.0


func _on_minimap_pressed() -> void:
	panel_requested.emit("res://scenes/game/panels/minimap.tscn")


func _on_exit_pressed() -> void:
	get_tree().quit()


func _on_command_submitted(_text: String) -> void:
	# C++ submits command: ! prefix = broadcast, @ = user command, $ = lua command, else = chat
	command.clear()


func _on_ac_pressed() -> void:
	_ac_magic = not _ac_magic
	_update_ac_dc()


func _on_dc_pressed() -> void:
	_dc_magic = not _dc_magic
	_update_ac_dc()


func _update_ac_dc() -> void:
	# C++ toggles AC<->MA (0x46<->0x48) and DC<->MC (0x47<->0x49)
	# Text font 11, size 15, yellow
	pass
