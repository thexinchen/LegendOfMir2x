extends Node

const SEFF_TRACK_COUNT := 128
const INVALID_SEFF_ID := 0xFFFFFFFF
const UI_CLICK_SEFF_ID := 0x01020069

var bgm_enabled := true
var bgm_volume := 0.5
var seff_enabled := true
var seff_volume := 0.5
var current_bgm_id := -1
var current_bgm_path := ""
var last_seff_id := INVALID_SEFF_ID
var last_seff_path := ""
var last_seff_distance := 0
var last_seff_angle := 0
var last_seff_pan := 0.0
var last_seff_gain := 0.0

var _bgm_player: AudioStreamPlayer
var _seff_players: Array[AudioStreamPlayer] = []
var _seff_panners: Array[AudioEffectPanner] = []
var _seff_bus_names: Array[StringName] = []
var _seff_gains: Array[float] = []
var _seff_cache: Dictionary = {}


func _ready() -> void:
	_bgm_player = AudioStreamPlayer.new()
	_bgm_player.name = "BGMPlayer"
	add_child(_bgm_player)
	for index in range(SEFF_TRACK_COUNT):
		var bus_name := StringName("SEFF%03d" % index)
		var bus_index := AudioServer.bus_count
		AudioServer.add_bus(bus_index)
		AudioServer.set_bus_name(bus_index, bus_name)
		AudioServer.set_bus_send(bus_index, &"Master")
		var panner := AudioEffectPanner.new()
		AudioServer.add_bus_effect(bus_index, panner)
		var player := AudioStreamPlayer.new()
		player.name = "SEFFPlayer%03d" % index
		player.bus = bus_name
		add_child(player)
		_seff_players.append(player)
		_seff_panners.append(panner)
		_seff_bus_names.append(bus_name)
		_seff_gains.append(1.0)
	_apply_bgm_volume()


func _exit_tree() -> void:
	stop_bgm()
	stop_seff()
	for index in range(_seff_bus_names.size() - 1, -1, -1):
		var bus_index := AudioServer.get_bus_index(_seff_bus_names[index])
		if bus_index >= 0:
			AudioServer.remove_bus(bus_index)


func play_map_bgm(bgm_id: int) -> bool:
	if bgm_id == current_bgm_id and _bgm_player.stream != null:
		return true
	stop_bgm()
	if bgm_id < 0 or bgm_id == 0xFFFFFFFF:
		return false
	var path := _find_bgm_path(bgm_id)
	if path.is_empty():
		push_warning("BGM resource not found: %08X" % bgm_id)
		return false
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.is_empty():
		push_warning("BGM resource is empty: %s" % path)
		return false
	var stream: AudioStream
	match path.get_extension().to_lower():
		"mp3":
			var mp3 := AudioStreamMP3.new()
			mp3.data = bytes
			mp3.loop = true
			stream = mp3
		"wav":
			var wav := AudioStreamWAV.load_from_file(path)
			if wav != null:
				wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
				wav.loop_begin = 0
				wav.loop_end = maxi(1, roundi(wav.get_length() * float(wav.mix_rate)))
			stream = wav
	if stream == null:
		push_warning("Failed to load BGM resource: %s" % path)
		return false
	_bgm_player.stream = stream
	current_bgm_id = bgm_id
	current_bgm_path = path
	_apply_bgm_volume()
	_bgm_player.play()
	return true


func stop_bgm() -> void:
	if _bgm_player != null:
		_bgm_player.stop()
		_bgm_player.stream = null
	current_bgm_id = -1
	current_bgm_path = ""


func set_bgm_enabled(enabled: bool) -> void:
	bgm_enabled = enabled
	_apply_bgm_volume()


func set_bgm_volume(value: float) -> void:
	bgm_volume = clampf(value, 0.0, 1.0)
	_apply_bgm_volume()


func set_seff_enabled(enabled: bool) -> void:
	seff_enabled = enabled
	if not enabled:
		stop_seff()


func set_seff_volume(value: float) -> void:
	seff_volume = clampf(value, 0.0, 1.0)
	for index in range(_seff_players.size()):
		if _seff_players[index].playing:
			_apply_seff_volume(index)


func play_seff_at(seff_id: int, source_x: int, source_y: int, listener_x: int, listener_y: int, repeats := 1) -> bool:
	if not seff_enabled or seff_id < 0 or seff_id == INVALID_SEFF_ID:
		return false
	var dx := source_x - listener_x
	var dy := source_y - listener_y
	var distance := roundi(Vector2(dx, dy).length())
	if distance > 255:
		return false
	var path := _find_seff_path(seff_id)
	if path.is_empty():
		push_warning("SEFF resource not found: %08X" % seff_id)
		return false
	var stream: AudioStreamWAV = _seff_cache.get(seff_id)
	if stream == null:
		stream = AudioStreamWAV.load_from_file(path)
		if stream == null:
			push_warning("Failed to load SEFF resource: %s" % path)
			return false
		_seff_cache[seff_id] = stream
	var player := _available_seff_player()
	if player == null:
		return false
	var angle := 0 if distance == 0 else roundi(90.0 - rad_to_deg(atan2(-float(dy), float(dx))))
	var spatial_distance := float(distance) * 0.5
	var gain := 1.0 if spatial_distance <= 1.0 else clampf((128.0 - spatial_distance) / 127.0, 0.0, 1.0)
	var pan := 0.0 if distance == 0 else sin(deg_to_rad(float(angle)))
	var play_stream: AudioStreamWAV = stream
	if repeats == 0:
		play_stream = stream.duplicate()
		play_stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
	player.stream = play_stream
	var player_index := _seff_players.find(player)
	if player_index >= 0:
		_seff_gains[player_index] = gain
		_seff_panners[player_index].pan = pan
		_apply_seff_volume(player_index)
	last_seff_id = seff_id
	last_seff_path = path
	last_seff_distance = distance
	last_seff_angle = angle
	last_seff_pan = pan
	last_seff_gain = gain
	player.play()
	return true


func play_ui_click() -> bool:
	return play_seff_at(UI_CLICK_SEFF_ID, 0, 0, 0, 0)


func bind_ui_click(button: BaseButton) -> void:
	var callback := Callable(self, "play_ui_click")
	if not button.pressed.is_connected(callback):
		button.pressed.connect(callback)


func bind_overlay_button(button: TextureButton) -> void:
	bind_ui_click(button)
	bind_overlay_visual(button)


func bind_overlay_visual(button: TextureButton) -> void:
	if button.has_meta("original_overlay_button"):
		return
	button.set_meta("original_overlay_button", true)
	button.set_meta("overlay_hovered", false)
	button.set_meta("overlay_pressed", false)
	button.modulate.a = 0.0
	button.mouse_entered.connect(_set_overlay_hovered.bind(button, true))
	button.mouse_exited.connect(_set_overlay_hovered.bind(button, false))
	button.button_down.connect(_set_overlay_pressed.bind(button, true))
	button.button_up.connect(_set_overlay_pressed.bind(button, false))


func _set_overlay_hovered(button: TextureButton, hovered: bool) -> void:
	button.set_meta("overlay_hovered", hovered)
	_update_overlay_alpha(button)


func _set_overlay_pressed(button: TextureButton, pressed: bool) -> void:
	button.set_meta("overlay_pressed", pressed)
	_update_overlay_alpha(button)


func _update_overlay_alpha(button: TextureButton) -> void:
	button.modulate.a = 1.0 if button.get_meta("overlay_hovered", false) or button.get_meta("overlay_pressed", false) else 0.0


func stop_seff() -> void:
	for player in _seff_players:
		player.stop()
		player.stream = null


func active_seff_count() -> int:
	var result := 0
	for player in _seff_players:
		result += 1 if player.playing else 0
	return result


func apply_runtime_config(config: Dictionary) -> void:
	set_bgm_enabled(_decode_bool(config.get(1, PackedByteArray()), true))
	set_bgm_volume(_decode_float(config.get(2, PackedByteArray()), 0.5))
	set_seff_enabled(_decode_bool(config.get(3, PackedByteArray()), true))
	set_seff_volume(_decode_float(config.get(4, PackedByteArray()), 0.5))


func _apply_bgm_volume() -> void:
	if _bgm_player == null:
		return
	var value := bgm_volume if bgm_enabled else 0.0
	_bgm_player.volume_db = linear_to_db(value) if value > 0.0 else -80.0


func _find_bgm_path(bgm_id: int) -> String:
	var prefix := "%08X_" % bgm_id
	for directory in _audio_base_paths():
		var bgm_dir := directory.path_join("bgm")
		if not DirAccess.dir_exists_absolute(bgm_dir):
			continue
		for file_name in DirAccess.get_files_at(bgm_dir):
			if file_name.to_upper().begins_with(prefix) and file_name.get_extension().to_lower() in ["mp3", "wav"]:
				return bgm_dir.path_join(file_name)
	return ""


func _find_seff_path(seff_id: int) -> String:
	for directory in _audio_base_paths():
		var seff_dir := directory.path_join("seff")
		if not DirAccess.dir_exists_absolute(seff_dir):
			continue
		var exact_path := seff_dir.path_join("%08X.WAV" % seff_id)
		if FileAccess.file_exists(exact_path):
			return exact_path
		var prefix := "%08X_" % seff_id
		for file_name in DirAccess.get_files_at(seff_dir):
			if file_name.to_upper().begins_with(prefix):
				return seff_dir.path_join(file_name)
	return ""


func _available_seff_player() -> AudioStreamPlayer:
	for player in _seff_players:
		if not player.playing:
			return player
	return null


func _apply_seff_volume(index: int) -> void:
	var value := seff_volume * _seff_gains[index]
	_seff_players[index].volume_db = linear_to_db(value) if value > 0.0 else -80.0


func _audio_base_paths() -> Array[String]:
	var result: Array[String] = []
	var env_path := OS.get_environment("MIR2X_AUDIO_RES")
	if not env_path.is_empty():
		result.append(env_path)
	result.append("res://audio")
	if not OS.has_feature("editor"):
		result.append(OS.get_executable_path().get_base_dir().path_join("audio"))
	return result


func _decode_bool(data: PackedByteArray, fallback: bool) -> bool:
	return data[1] != 0 if data.size() >= 2 else fallback


func _decode_float(data: PackedByteArray, fallback: float) -> float:
	return data.decode_float(1) if data.size() >= 5 and data[0] != 0 else fallback
