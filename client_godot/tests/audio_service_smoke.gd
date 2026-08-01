extends Node

const WorldResourceScript = preload("res://scripts/game/world_resource.gd")


func _ready() -> void:
	var world: RefCounted = WorldResourceScript.new()
	if not world.load_map(24) or world.bgm_id != 0x00010002:
		_fail("map BGM metadata unavailable")
		return
	if not AudioService.play_map_bgm(world.bgm_id):
		_fail("failed to load map BGM")
		return
	if AudioService.current_bgm_id != world.bgm_id or AudioService.current_bgm_path.is_empty():
		_fail("map BGM state mismatch")
		return
	AudioService.set_seff_enabled(true)
	AudioService.set_seff_volume(0.75)
	if not AudioService.play_seff_at(0x01000001, 10, 0, 0, 0):
		_fail("failed to load step SEFF")
		return
	if AudioService.last_seff_id != 0x01000001 or AudioService.last_seff_distance != 10 or AudioService.last_seff_angle != 90:
		_fail("SEFF position mismatch")
		return
	if absf(AudioService.last_seff_pan - 1.0) > 0.001 or absf(AudioService.last_seff_gain - (123.0 / 127.0)) > 0.001:
		_fail("SEFF panning/attenuation mismatch")
		return
	var seff_players: Array = AudioService.get("_seff_players")
	AudioService.set_seff_volume(0.25)
	if seff_players.is_empty() or absf(seff_players[0].volume_db - linear_to_db(0.25 * AudioService.last_seff_gain)) > 0.001:
		_fail("active SEFF volume did not follow runtime slider")
		return
	if AudioService.play_seff_at(0x01000001, 256, 0, 0, 0):
		_fail("SEFF beyond C++ 255-grid limit was played")
		return
	var controls: Control = load("res://scenes/game/control_panel.tscn").instantiate()
	add_child(controls)
	await get_tree().process_frame
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	controls.get_node("Body/QuickButton").pressed.emit()
	if AudioService.last_seff_id != AudioService.UI_CLICK_SEFF_ID or AudioService.last_seff_distance != 0 or AudioService.last_seff_gain != 1.0:
		_fail("main HUD button did not play the original centered click SEFF")
		return
	var quick_button := controls.get_node("Body/QuickButton") as TextureButton
	if quick_button.modulate.a != 0.0:
		_fail("HUD overlay button was visible without hover")
		return
	quick_button.mouse_entered.emit()
	if quick_button.modulate.a != 1.0:
		_fail("HUD overlay button did not appear on hover")
		return
	quick_button.mouse_exited.emit()
	if quick_button.modulate.a != 0.0:
		_fail("HUD overlay button remained after hover")
		return
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	controls.get_node("Body/ACButton").pressed.emit()
	if AudioService.last_seff_id != AudioService.INVALID_SEFF_ID:
		_fail("direct AC control incorrectly gained a click SEFF")
		return
	for path in [
		"Body/QuickButton", "Body/ExitButton", "Body/BoardButtons/Guild", "Body/BoardButtons/Team",
		"Body/BoardButtons/Quest", "Body/BoardButtons/Inventory", "Body/BoardButtons/PlayerState",
		"Body/BoardButtons/Skill", "Body/BoardButtons/Horse", "Body/BoardButtons/Config",
		"Body/BoardButtons/Friend", "Body/Exchange", "Body/MiniMap", "Body/MagicKey",
		"Body/ExpandButton", "MinimizeButton",
	]:
		if not (controls.get_node(path) as BaseButton).tooltip_text.is_empty():
			_fail("main HUD retained an invented text tooltip: %s" % path)
			return
	controls.queue_free()
	await get_tree().process_frame
	AudioService.apply_runtime_config({
		1: PackedByteArray([1, 0, 0]),
		2: _float_archive(0.25),
		3: PackedByteArray([1, 0, 0]),
		4: _float_archive(0.75),
	})
	if AudioService.bgm_enabled or AudioService.seff_enabled or absf(AudioService.bgm_volume - 0.25) > 0.001 or absf(AudioService.seff_volume - 0.75) > 0.001:
		_fail("runtime audio config mismatch")
		return
	var panel: Control = load("res://scenes/game/panels/runtime_config.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("SystemPage/BGM").button_pressed or panel.get_node("SystemPage/SEFF").button_pressed:
		_fail("runtime panel enabled state mismatch")
		return
	if absf(panel.get_node("SystemPage/BGMVolume").value - 25.0) > 0.01 or absf(panel.get_node("SystemPage/SEFFVolume").value - 75.0) > 0.01:
		_fail("runtime panel volume mismatch")
		return
	panel.queue_free()
	await get_tree().process_frame
	AudioService.stop_bgm()
	AudioService.stop_seff()
	await get_tree().create_timer(0.1).timeout
	print("AUDIO SERVICE PASS: BGM, 128-track WAV SEFF, spatial attenuation, saved config and panel restore")
	get_tree().quit()


func _float_archive(value: float) -> PackedByteArray:
	var result := PackedByteArray([1, 0, 0, 0, 0, 0])
	result.encode_float(1, value)
	return result


func _fail(message: String) -> void:
	push_error("AUDIO_SERVICE_SMOKE %s" % message)
	get_tree().quit(1)
