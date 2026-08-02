extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default() or resources.buff_meta.is_empty() or resources.monster_meta.is_empty():
		_fail("HUD metadata unavailable")
		return
	var background := ColorRect.new()
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.color = Color(0.06, 0.08, 0.08, 1.0)
	add_child(background)
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.player_hp = 75
	GameState.player_hp_max = 100
	GameState.player_mp = 50
	GameState.player_mp_max = 100
	GameState.buff_list = [resources.buff_meta.keys()[0]]
	GameState.chat_log.clear()
	for index in range(12):
		GameState.add_chat_log("聊天滚动测试 %02d" % index, index % 4)
	var panel: Control = load("res://scenes/game/control_panel.tscn").instantiate()
	panel.position = Vector2(0, 448)
	add_child(panel)
	await get_tree().process_frame
	panel.set_process(false)
	var monster_id: int = resources.monster_meta.keys()[0]
	panel.call("_apply_focus_hud", {
		"uid": 101,
		"type": 1,
		"monster_id": monster_id,
		"hp": 25,
		"hp_max": 100,
		"buffs": GameState.buff_list,
	})
	await RenderingServer.frame_post_draw
	var output_path := OS.get_environment("MIR2X_PANEL_OUTPUT")
	if output_path.is_empty():
		output_path = "/tmp/mir2x-focus-hud.png"
	var error := get_viewport().get_texture().get_image().save_png(output_path)
	if error != OK:
		_fail("unable to save screenshot: %s" % error)
		return
	print("FOCUS HUD VISUAL PASS: %s" % output_path)
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("FOCUS_HUD_VISUAL %s" % message)
	get_tree().quit(1)
