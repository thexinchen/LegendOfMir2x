extends Control

@onready var inventory_panel: Control = %InventoryPanel
@onready var player_state_panel: Control = %PlayerStatePanel
@onready var skill_panel: Control = %SkillPanel
@onready var quick_bar: Control = %QuickBar
@onready var location_label: Label = $Location

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
	quick_bar.hide()
	# C++ location format: "mapName: x y", font 10 size 15, white, at {4, localBaseY+110}
	# localBaseY = screenH - 133 = 600 - 133 = 467, so y = 467 + 110 = 577
	location_label.position = Vector2(4, 577)
	location_label.add_theme_font_size_override("font_size", 15)
	location_label.add_theme_color_override("font_color", Color(1, 1, 1, 1))
	location_label.text = "边境城市: 335 271"
	if OS.has_environment("MIR2X_CONTROL_PANEL_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(
			OS.get_environment("MIR2X_CONTROL_PANEL_SCREENSHOT"),
		)
		get_tree().quit()
		return
	if OS.has_environment("MIR2X_GAME_SCREENSHOT"):
		inventory_panel.show()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_GAME_SCREENSHOT"))
		get_tree().quit()


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	# C++ key bindings: ESC=center hero, TAB=pickup, Alt+E=exit, Alt+F=fullscreen, Enter=focus command
	# Other keys are magic hotkeys via checkMagicSpell
	if event.keycode == KEY_ESCAPE:
		# center hero - no-op in placeholder
		pass
	elif event.keycode == KEY_TAB:
		# pickup - no-op in placeholder
		pass
	elif event is InputEventKey and (event.alt_pressed):
		if event.keycode == KEY_E:
			get_tree().quit()
		elif event.keycode == KEY_F:
			# toggle fullscreen
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
			push_error("Unable to load panel: %s" % scene_path)
			return
		panel = packed.instantiate() as Control
		add_child(panel)
		panel.position = (size - panel.size) * 0.5
		_extra_panel_nodes[scene_path] = panel
		return
	_toggle_panel(panel)
