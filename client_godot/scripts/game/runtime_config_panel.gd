extends "res://scripts/game/closable_panel.gd"

var _updating := false


func _ready() -> void:
	super._ready()
	$SystemPage/Fullscreen.toggled.connect(_set_fullscreen)
	$SystemPage/ShowFPS.toggled.connect(func(value: bool): NetworkClient.send_runtime_bool(6, value))
	$SystemPage/BGM.toggled.connect(func(value: bool): NetworkClient.send_runtime_bool(1, value))
	$SystemPage/BGMVolume.value_changed.connect(func(value: float): NetworkClient.send_runtime_float(2, value / 100.0))
	$SystemPage/SEFF.toggled.connect(func(value: bool): NetworkClient.send_runtime_bool(3, value))
	$SystemPage/SEFFVolume.value_changed.connect(func(value: float): NetworkClient.send_runtime_float(4, value / 100.0))
	$SystemPage/IME.item_selected.connect(func(index: int): NetworkClient.send_runtime_int(7, index))
	$SystemPage/Resolution.item_selected.connect(_set_resolution)
	for button in [$Menu/System, $Menu/Social, $Menu/Network, $Menu/Game, $Menu/Help]:
		button.pressed.connect(func(): $SystemPage.show())


func _set_fullscreen(enabled: bool) -> void:
	if _updating:
		return
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if enabled else DisplayServer.WINDOW_MODE_WINDOWED)
	NetworkClient.send_runtime_bool(5, enabled)


func _set_resolution(index: int) -> void:
	var sizes := [Vector2i(800, 600), Vector2i(960, 600), Vector2i(1024, 768), Vector2i(1280, 720), Vector2i(1280, 768), Vector2i(1280, 800)]
	if index >= 0 and index < sizes.size() and DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_WINDOWED:
		DisplayServer.window_set_size(sizes[index])
