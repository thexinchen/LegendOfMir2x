extends "res://scripts/game/closable_panel.gd"

var _updating := false


func _ready() -> void:
	super._ready()
	_updating = true
	$SystemPage/BGM.button_pressed = AudioService.bgm_enabled
	$SystemPage/BGMVolume.value = AudioService.bgm_volume * 100.0
	$SystemPage/SEFF.button_pressed = AudioService.seff_enabled
	$SystemPage/SEFFVolume.value = AudioService.seff_volume * 100.0
	_updating = false
	$SystemPage/Fullscreen.toggled.connect(_set_fullscreen)
	$SystemPage/ShowFPS.toggled.connect(func(value: bool): NetworkClient.send_runtime_bool(6, value))
	$SystemPage/BGM.toggled.connect(_set_bgm_enabled)
	$SystemPage/BGMVolume.value_changed.connect(_set_bgm_volume)
	$SystemPage/SEFF.toggled.connect(_set_seff_enabled)
	$SystemPage/SEFFVolume.value_changed.connect(_set_seff_volume)
	$SystemPage/IME.item_selected.connect(func(index: int): NetworkClient.send_runtime_int(7, index))
	$SystemPage/Resolution.item_selected.connect(_set_resolution)
	for button in [$Menu/System, $Menu/Social, $Menu/Network, $Menu/Game, $Menu/Help]:
		button.pressed.connect(func(): $SystemPage.show())


func _set_bgm_enabled(enabled: bool) -> void:
	if _updating:
		return
	AudioService.set_bgm_enabled(enabled)
	NetworkClient.send_runtime_bool(1, enabled)


func _set_bgm_volume(value: float) -> void:
	if _updating:
		return
	AudioService.set_bgm_volume(value / 100.0)
	NetworkClient.send_runtime_float(2, value / 100.0)


func _set_seff_enabled(enabled: bool) -> void:
	if _updating:
		return
	AudioService.set_seff_enabled(enabled)
	NetworkClient.send_runtime_bool(3, enabled)


func _set_seff_volume(value: float) -> void:
	if _updating:
		return
	AudioService.set_seff_volume(value / 100.0)
	NetworkClient.send_runtime_float(4, value / 100.0)


func _set_fullscreen(enabled: bool) -> void:
	if _updating:
		return
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if enabled else DisplayServer.WINDOW_MODE_WINDOWED)
	NetworkClient.send_runtime_bool(5, enabled)


func _set_resolution(index: int) -> void:
	var sizes := [Vector2i(800, 600), Vector2i(960, 600), Vector2i(1024, 768), Vector2i(1280, 720), Vector2i(1280, 768), Vector2i(1280, 800)]
	if index >= 0 and index < sizes.size() and DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_WINDOWED:
		DisplayServer.window_set_size(sizes[index])
