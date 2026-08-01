extends Node

var bgm_enabled := true
var bgm_volume := 0.5
var seff_enabled := true
var seff_volume := 0.5
var current_bgm_id := -1
var current_bgm_path := ""

var _bgm_player: AudioStreamPlayer


func _ready() -> void:
	_bgm_player = AudioStreamPlayer.new()
	_bgm_player.name = "BGMPlayer"
	add_child(_bgm_player)
	_apply_bgm_volume()


func _exit_tree() -> void:
	stop_bgm()


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
	var stream := AudioStreamMP3.new()
	stream.data = bytes
	stream.loop = true
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


func set_seff_volume(value: float) -> void:
	seff_volume = clampf(value, 0.0, 1.0)


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
		for file_name in DirAccess.get_files_at(bgm_dir):
			if file_name.to_upper().begins_with(prefix):
				return bgm_dir.path_join(file_name)
	return ""


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
