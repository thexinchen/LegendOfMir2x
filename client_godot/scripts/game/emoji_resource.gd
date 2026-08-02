extends RefCounted

const BASE_PATH := "res://assets/generated/emoji"

var _definitions: Dictionary = {}
var _loaded := false


func definition(emoji_id: int) -> Dictionary:
	_load_index()
	return _definitions.get(emoji_id, {})


func frame(emoji_id: int) -> Dictionary:
	var source: Dictionary = definition(emoji_id)
	if source.is_empty():
		return {}
	var atlas_source := load(source.path) as Texture2D
	if atlas_source == null:
		return {}
	var atlas := AtlasTexture.new()
	atlas.atlas = atlas_source
	atlas.region = Rect2(0, 0, source.width, source.height)
	var result := source.duplicate()
	result["texture"] = atlas
	return result


func _load_index() -> void:
	if _loaded:
		return
	_loaded = true
	for file_name in DirAccess.get_files_at(BASE_PATH):
		var resource_name := file_name.get_basename() if file_name.to_lower().ends_with(".png.import") else file_name
		if not resource_name.to_lower().ends_with(".png"):
			continue
		var stem := resource_name.get_basename()
		if stem.length() < 24:
			continue
		var key := stem.substr(0, 8).hex_to_int()
		if (key & 0xFF) != 0:
			continue
		_definitions[key >> 8] = {
			"path": BASE_PATH.path_join(resource_name),
			"frame_count": stem.substr(8, 2).hex_to_int(),
			"fps": stem.substr(10, 2).hex_to_int(),
			"width": stem.substr(12, 4).hex_to_int(),
			"height": stem.substr(16, 4).hex_to_int(),
			"h1": stem.substr(20, 4).hex_to_int(),
		}
