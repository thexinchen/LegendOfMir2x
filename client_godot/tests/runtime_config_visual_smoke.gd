extends Control


func _ready() -> void:
	GameState.runtime_config = {}
	var panel := load("res://scenes/game/panels/runtime_config.tscn").instantiate() as Control
	add_child(panel)
	panel.position = Vector2(100, 60)
	await get_tree().process_frame
	await get_tree().process_frame
	var system_path := OS.get_environment("MIR2X_RUNTIME_SYSTEM_SCREENSHOT")
	if not system_path.is_empty():
		get_viewport().get_texture().get_image().save_png(system_path)
	panel.call("_select_main_page", 1)
	await get_tree().process_frame
	var social_path := OS.get_environment("MIR2X_RUNTIME_SOCIAL_SCREENSHOT")
	if not social_path.is_empty():
		get_viewport().get_texture().get_image().save_png(social_path)
	if panel.get_node_or_null("SystemPage/Config23") == null:
		push_error("RUNTIME_CONFIG_VISUAL social page did not render")
		get_tree().quit(1)
		return
	print("RUNTIME CONFIG VISUAL PASS")
	get_tree().quit()
