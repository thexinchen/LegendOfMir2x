extends Control

@onready var inventory_panel: Control = %InventoryPanel
@onready var player_state_panel: Control = %PlayerStatePanel
@onready var skill_panel: Control = %SkillPanel

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
	inventory_panel.hide()
	player_state_panel.hide()
	skill_panel.hide()
	if OS.has_environment("MIR2X_GAME_SCREENSHOT"):
		inventory_panel.show()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GAME_SCREENSHOT"))
		get_tree().quit()


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	if event.keycode == KEY_B:
		_toggle_panel(inventory_panel)
	elif event.keycode == KEY_C:
		_toggle_panel(player_state_panel)
	elif event.keycode == KEY_S:
		_toggle_panel(skill_panel)
	elif EXTRA_PANELS.has(event.keycode):
		_toggle_extra_panel(EXTRA_PANELS[event.keycode])


func _on_inventory_pressed() -> void:
	_toggle_panel(inventory_panel)


func _on_player_pressed() -> void:
	_toggle_panel(player_state_panel)


func _on_skill_pressed() -> void:
	_toggle_panel(skill_panel)


func _toggle_panel(panel: Control) -> void:
	panel.visible = not panel.visible
	if panel.visible:
		panel.move_to_front()


func _toggle_extra_panel(scene_path: String) -> void:
	var panel := _extra_panel_nodes.get(scene_path) as Control
	if not panel:
		var packed := load(scene_path) as PackedScene
		if not packed:
			push_error("Unable to load panel: %s" % scene_path)
			return
		panel = packed.instantiate() as Control
		add_child(panel)
		panel.position = (size - panel.size) * 0.5
		_extra_panel_nodes[scene_path] = panel
		return
	_toggle_panel(panel)
