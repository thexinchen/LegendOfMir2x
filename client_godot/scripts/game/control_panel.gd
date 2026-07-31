extends Control

signal panel_requested(scene_path: String)
signal quick_bar_toggled

@onready var body: Control = %Body
@onready var command: LineEdit = %Command
@onready var compact_middle: NinePatchRect = %CompactMiddle
@onready var expanded_middle: NinePatchRect = %ExpandedMiddle
@onready var chat_log: Label = %ChatLog

var _minimized := false
var _expanded := false


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
	chat_log.offset_top = -220.0 if _expanded else 34.0


func _on_minimap_pressed() -> void:
	panel_requested.emit("res://scenes/game/panels/minimap.tscn")


func _on_exit_pressed() -> void:
	get_tree().quit()


func _on_command_submitted(_text: String) -> void:
	command.clear()
