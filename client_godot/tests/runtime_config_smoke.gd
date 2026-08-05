extends Node


func _ready() -> void:
	NetworkClient.disconnect_from_server()
	GameState.runtime_config = {
		1: _bool_archive(false),
		2: _float_archive(0.25),
		3: _bool_archive(true),
		4: _float_archive(0.75),
		6: _bool_archive(true),
		7: _int_archive(1),
		9: _bool_archive(false),
		24: _bool_archive(false),
		38: _bool_archive(false),
		43: _bool_archive(false),
		47: _pair_archive(Vector2i(800, 600)),
		48: _int_archive(1),
	}
	var panel := load("res://scenes/game/panels/runtime_config.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	if not panel.has_method("_display_settings"):
		_fail("runtime config has no shared display-settings entry point")
		return
	var display_settings: Dictionary = panel.call("_display_settings", {
		5: _bool_archive(true),
		47: _pair_archive(Vector2i(1280, 720)),
	})
	if not display_settings.get("fullscreen", false) or display_settings.get("window_size", Vector2i.ZERO) != Vector2i(1280, 720):
		_fail("runtime display settings did not decode the server archive: %s" % display_settings)
		return
	if panel.size != Vector2(600, 480) or panel.get_node("Background").patch_margin_left != 58:
		_fail("native frame size or 58px slicing mismatch")
		return
	if panel.get_node("SystemPage/BGM").button_pressed or not panel.get_node("SystemPage/SEFF").button_pressed:
		_fail("incoming audio enabled state was not restored")
		return
	if absf(panel.get_node("SystemPage/BGMVolume").value - 25.0) > 0.01 or absf(panel.get_node("SystemPage/SEFFVolume").value - 75.0) > 0.01:
		_fail("incoming audio volume was not restored")
		return
	if not panel.get_node("SystemPage/ShowFPS").button_pressed or panel.get_node("SystemPage/IME").selected != 0:
		_fail("incoming FPS or IME state was not restored: fps=%s ime=%s raw=%s" % [panel.get_node("SystemPage/ShowFPS").button_pressed, panel.get_node("SystemPage/IME").selected, GameState.runtime_config])
		return
	panel.call("_set_ime", 2)
	if panel.call("_config_int", 7, 0) != 3:
		_fail("IME UI index was not encoded as the protocol enum: raw=%s" % panel.call("_config_int", 7, 0))
		return
	panel.call("_select_system_tab", 1)
	if panel.get_node_or_null("SystemPage/EnglishPreview") == null or panel.get_node("SystemPage/PreviewSize").value != 12:
		_fail("appearance/font preview page is incomplete")
		return
	panel.call("_set_preview_size", 18.0)
	if panel.get_node("SystemPage/EnglishPreview").get_theme_font_size("font_size") != 18:
		_fail("font preview size did not update")
		return
	panel.call("_select_main_page", 1)
	if panel.get("_main_page") != 1 or panel.get_node_or_null("SystemPage/Config23") == null:
		_fail("social page or full permission list is missing")
		return
	if panel.get_node("SystemPage/Config9").button_pressed:
		_fail("incoming social permission was not restored")
		return
	panel.call("_select_social_tab", 1)
	var friend_radios: Array = panel.get("_friend_radios")
	if friend_radios.size() != 3 or not friend_radios[1].button_pressed:
		_fail("friend-request policy was not restored")
		return
	friend_radios[0].button_pressed = true
	if panel.call("_config_int", 48, 2) != 0:
		_fail("friend-request policy did not update the local archive")
		return
	panel.call("_select_main_page", 2)
	if panel.get("_main_page") != 2 or panel.get_node("SystemPage").get_child_count() != 0:
		_fail("network placeholder did not preserve its distinct empty page")
		return
	panel.call("_select_main_page", 3)
	if panel.get_node_or_null("SystemPage/Config37") == null or panel.get_node("SystemPage/Config24").button_pressed:
		_fail("game common page or incoming state is incomplete")
		return
	panel.call("_select_game_tab", 1)
	if panel.get_node_or_null("SystemPage/Config42") == null or panel.get_node("SystemPage/Config38").button_pressed:
		_fail("game auxiliary page is incomplete")
		return
	panel.call("_select_game_tab", 2)
	if panel.get_node_or_null("SystemPage/Config46") == null or panel.get_node("SystemPage/Config43").button_pressed:
		_fail("game protection page is incomplete")
		return
	panel.call("_select_main_page", 4)
	if panel.get("_main_page") != 4 or panel.get_node("SystemPage").get_child_count() != 0:
		_fail("help placeholder did not preserve its distinct empty page")
		return
	panel.call("_select_main_page", 0)
	panel.call("_select_system_tab", 0)
	panel.call("_set_resolution", 1)
	if panel.call("_config_pair", 47, Vector2i.ZERO) != Vector2i(960, 600):
		_fail("window-size pair did not round-trip through the local archive")
		return
	if DisplayServer.get_name() != "headless":
		for _frame in 3:
			await get_tree().process_frame
		if DisplayServer.window_get_size() != Vector2i(960, 600):
			_fail("window-size runtime config did not apply to the live window: %s" % DisplayServer.window_get_size())
			return
		if get_viewport().get_visible_rect().size != Vector2(960, 600):
			_fail("window-size runtime config only stretched the 800x600 canvas instead of resizing the game viewport: visible=%s root_size=%s content_mode=%s content_size=%s window=%s" % [get_viewport().get_visible_rect().size, get_tree().root.size, get_tree().root.content_scale_mode, get_tree().root.content_scale_size, DisplayServer.window_get_size()])
			return
	panel.get_node("SystemPage/BGM").button_pressed = true
	if not panel.call("_config_bool", 1, false) or not AudioService.bgm_enabled:
		_fail("BGM toggle did not update runtime state and audio")
		return
	print("RUNTIME CONFIG PASS: five pages, all protocol flags, native controls, audio, window, IME and font preview")
	get_tree().quit()


func _bool_archive(value: bool) -> PackedByteArray:
	return PackedByteArray([1, 1 if value else 0, 0])


func _float_archive(value: float) -> PackedByteArray:
	var result := PackedByteArray([1, 0, 0, 0, 0, 0])
	result.encode_float(1, value)
	return result


func _int_archive(value: int) -> PackedByteArray:
	var result := PackedByteArray([1, 0, 0, 0, 0, 0])
	result.encode_s32(1, value)
	return result


func _pair_archive(value: Vector2i) -> PackedByteArray:
	var result := PackedByteArray([1, 0, 0, 0, 0, 0, 0, 0, 0, 0])
	result.encode_s32(1, value.x)
	result.encode_s32(5, value.y)
	return result


func _fail(message: String) -> void:
	push_error("RUNTIME_CONFIG_SMOKE %s" % message)
	get_tree().quit(1)
