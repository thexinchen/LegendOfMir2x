extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default() or resources.skill_meta.is_empty() or resources.buff_meta.is_empty():
		_fail("HUD metadata unavailable")
		return
	var magic_id := 0
	for value in resources.skill_meta:
		var layout: PackedInt32Array = resources.skill_layout(int(value))
		if layout.size() >= 6 and layout[5] > 0 and not resources.frame("proguse", layout[0] + 0x1000).is_empty():
			magic_id = int(value)
			break
	if magic_id == 0:
		_fail("cooldown skill icon unavailable")
		return
	var buff_id := int(resources.buff_meta.keys()[0])
	GameState.magic_keys = {magic_id: 120}
	GameState.magic_cast_times = {magic_id: Time.get_ticks_msec()}
	GameState.magic_key_hud_visible = true
	GameState.buff_list = [buff_id]
	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	GameState.player_x = 405
	GameState.player_y = 120
	var main: Control = load("res://scenes/game/main.tscn").instantiate()
	add_child(main)
	await get_tree().process_frame
	var hud: Control = main.get_node("SkillBuffHUD")
	if hud.call("magic_icon_count") != 1 or hud.call("buff_icon_count") != 1:
		_fail("top HUD icon count mismatch")
		return
	var cool_down: int = resources.skill_layout(magic_id)[5]
	GameState.magic_cast_times[magic_id] = Time.get_ticks_msec() - cool_down / 2
	var angle := float(hud.call("cooldown_angle", magic_id))
	if angle < 150.0 or angle > 210.0 or main.call("_magic_ready", magic_id):
		_fail("half cooldown angle or cast guard mismatch: %s" % angle)
		return
	main.call("_on_control_panel_magic_key_hud_toggled")
	if GameState.magic_key_hud_visible:
		_fail("magic HUD toggle failed")
		return
	if main.call("minimap_hud_width") != 0.0:
		_fail("hidden minimap shifted buffs")
		return
	var minimap := main.call("_ensure_extra_panel", "res://scenes/game/panels/minimap.tscn") as Control
	minimap.show()
	await get_tree().process_frame
	if minimap.position != Vector2(600.0, 0.0) or main.call("minimap_hud_width") != minimap.size.x:
		_fail("visible minimap position or buff offset mismatch")
		return
	main.call("_on_control_panel_magic_key_hud_toggled")
	await get_tree().process_frame
	var screenshot_path := OS.get_environment("MIR2X_SCREENSHOT")
	if not screenshot_path.is_empty():
		await RenderingServer.frame_post_draw
		var error := get_viewport().get_texture().get_image().save_png(screenshot_path)
		if error != OK:
			_fail("failed to save screenshot: %s" % error)
			return
	print("SKILL BUFF HUD PASS: original icons, cooldown, toggle and right-aligned buffs")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("SKILL_BUFF_HUD_SMOKE %s" % message)
	get_tree().quit(1)
