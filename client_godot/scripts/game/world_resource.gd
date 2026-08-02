class_name WorldResource
extends RefCounted

const MAGIC := "M2GW"
const VERSION := 1

var base_path: String = ""
var map_id: int = 0
var width: int = 0
var height: int = 0
var minimap_id: int = -1
var map_name: String = ""
var bgm_id: int = -1
var land: PackedByteArray = PackedByteArray()
var tiles: Dictionary = {}
var objects: Array[Dictionary] = [{}, {}, {}, {}]
var _texture_cache: Dictionary = {}
var last_error: String = ""


func load_map(requested_map_id: int, progress_callback := Callable()) -> bool:
	clear()
	base_path = _find_base_path(requested_map_id)
	if base_path.is_empty():
		last_error = "world resource directory not found for map %d" % requested_map_id
		return false

	var map_path := "%s/maps/%08X.m2xmap" % [base_path, requested_map_id]
	var file := FileAccess.open(map_path, FileAccess.READ)
	if file == null:
		last_error = "failed to open map manifest: %s" % map_path
		return false
	if file.get_buffer(4).get_string_from_ascii() != MAGIC:
		last_error = "invalid map manifest magic: %s" % map_path
		return false
	var version := file.get_32()
	if version != VERSION:
		last_error = "unsupported map manifest version: %d" % version
		return false

	map_id = file.get_32()
	width = file.get_32()
	height = file.get_32()
	var tile_count := file.get_32()
	var object_count := file.get_32()
	if map_id != requested_map_id or width <= 0 or height <= 0:
		last_error = "invalid map manifest header: %s" % map_path
		return false
	var meta_file := FileAccess.open("%s/maps/%08X.m2xmeta" % [base_path, requested_map_id], FileAccess.READ)
	if meta_file != null and meta_file.get_length() >= 4:
		minimap_id = meta_file.get_32()
		if meta_file.get_length() >= 6:
			var name_length := meta_file.get_16()
			if name_length <= meta_file.get_length() - meta_file.get_position():
				map_name = meta_file.get_buffer(name_length).get_string_from_utf8()
				if meta_file.get_length() - meta_file.get_position() >= 4:
					bgm_id = meta_file.get_32()
	_report_progress(progress_callback, 0)

	land = file.get_buffer(width * height)
	if land.size() != width * height:
		last_error = "truncated map land data: %s" % map_path
		return false
	_report_progress(progress_callback, 40)
	var total_records := maxi(1, tile_count + object_count)
	var loaded_records := 0
	var last_progress := 40
	for _index in range(tile_count):
		var x := file.get_16()
		var y := file.get_16()
		var texture_id := file.get_32()
		tiles[x + y * width] = texture_id
		loaded_records += 1
		last_progress = _report_record_progress(progress_callback, loaded_records, total_records, last_progress)
	for _index in range(object_count):
		var x := file.get_16()
		var y := file.get_16()
		var texture_id := file.get_32()
		var depth := file.get_8()
		var flags := file.get_8()
		var tick_type := file.get_8()
		var frame_count := file.get_8()
		if depth >= objects.size():
			continue
		var key := x + y * width
		var cell_objects: Array = objects[depth].get(key, [])
		cell_objects.append(PackedInt32Array([texture_id, flags, tick_type, frame_count]))
		objects[depth][key] = cell_objects
		loaded_records += 1
		last_progress = _report_record_progress(progress_callback, loaded_records, total_records, last_progress)
	_report_progress(progress_callback, 100)
	return true


func _report_record_progress(callback: Callable, loaded: int, total: int, last_progress: int) -> int:
	var progress := 40 + roundi(float(loaded) * 60.0 / float(total))
	if progress > last_progress:
		_report_progress(callback, progress)
		return progress
	return last_progress


func _report_progress(callback: Callable, progress: int) -> void:
	if callback.is_valid():
		callback.call(clampi(progress, 0, 100), map_name)


func clear() -> void:
	map_id = 0
	width = 0
	height = 0
	minimap_id = -1
	map_name = ""
	bgm_id = -1
	land.clear()
	tiles.clear()
	objects = [{}, {}, {}, {}]
	_texture_cache.clear()
	last_error = ""


func can_walk(x: int, y: int) -> bool:
	if x < 0 or y < 0 or x >= width or y >= height:
		return false
	return bool(land[x + y * width] & 0x80)


func texture(texture_id: int) -> Texture2D:
	if _texture_cache.has(texture_id):
		return _texture_cache[texture_id]
	var path := "%s/textures/%08X.png" % [base_path, texture_id]
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.is_empty():
		_texture_cache[texture_id] = null
		return null
	var image := Image.new()
	if image.load_png_from_buffer(bytes) != OK:
		_texture_cache[texture_id] = null
		return null
	var result := ImageTexture.create_from_image(image)
	_texture_cache[texture_id] = result
	return result


func _find_base_path(requested_map_id: int) -> String:
	var candidates: Array[String] = []
	var env_path := OS.get_environment("MIR2X_WORLD_RES")
	if not env_path.is_empty():
		candidates.append(env_path)
	candidates.append("res://world_res")
	if not OS.has_feature("editor"):
		candidates.append(OS.get_executable_path().get_base_dir().path_join("world_res"))
	for candidate in candidates:
		var map_path := "%s/maps/%08X.m2xmap" % [candidate, requested_map_id]
		if FileAccess.file_exists(map_path):
			return candidate
	return ""
