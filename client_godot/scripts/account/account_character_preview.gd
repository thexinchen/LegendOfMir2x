class_name AccountCharacterPreview
extends Control

const SHADOW_MASK := 1 << 14
const MAGIC_MASK := 1 << 13

const FRAME_COUNTS := {
	Vector3i(1, 0, 0): 11, Vector3i(1, 0, 1): 11, Vector3i(1, 0, 2): 11, Vector3i(1, 0, 3): 12, Vector3i(1, 0, 4): 16,
	Vector3i(1, 1, 0): 11, Vector3i(1, 1, 1): 20, Vector3i(1, 1, 2): 12, Vector3i(1, 1, 3): 8, Vector3i(1, 1, 4): 18,
	Vector3i(2, 0, 0): 11, Vector3i(2, 0, 1): 17, Vector3i(2, 0, 2): 11, Vector3i(2, 0, 3): 10, Vector3i(2, 0, 4): 15,
	Vector3i(2, 1, 0): 11, Vector3i(2, 1, 1): 17, Vector3i(2, 1, 2): 20, Vector3i(2, 1, 3): 12, Vector3i(2, 1, 4): 17,
	Vector3i(4, 0, 0): 11, Vector3i(4, 0, 1): 18, Vector3i(4, 0, 2): 11, Vector3i(4, 0, 3): 9, Vector3i(4, 0, 4): 17,
	Vector3i(4, 1, 0): 11, Vector3i(4, 1, 1): 12, Vector3i(4, 1, 2): 11, Vector3i(4, 1, 3): 11, Vector3i(4, 1, 4): 15,
}

var _resources: RefCounted
var _anchor := Vector2.ZERO
var _frame_id := 0
var _tint := Color.WHITE


static func frame_count(job: int, male: bool, motion: int) -> int:
	return FRAME_COUNTS.get(Vector3i(job, 1 if male else 0, motion), 0)


static func select_base_id(job: int, male: bool, motion: int) -> int:
	var job_index: int = {1: 0, 2: 1, 4: 2}.get(job, 0)
	return (job_index << 10) | ((1 if male else 0) << 9) | (motion << 5)


static func create_base_id(job: int, male: bool) -> int:
	# Jobs are protocol bit flags (1/2/4), while selectchar stores them as gfx indices (0/1/2).
	# The C++ creation screen uses `job - JOB_BEGIN`, which points wizard at a nonexistent index 3.
	var job_index: int = {1: 0, 2: 1, 4: 2}.get(job, 0)
	return (job_index << 10) | ((1 if male else 0) << 9) | (4 << 5)


func show_frame(resources: RefCounted, anchor: Vector2, frame_id: int, tint: Color) -> void:
	_resources = resources
	_anchor = anchor
	_frame_id = frame_id
	_tint = tint
	show()
	queue_redraw()


func _draw() -> void:
	if _resources == null:
		return
	_draw_layer(_frame_id, _tint)
	var shadow_tint := _tint
	shadow_tint.a *= 150.0 / 255.0
	_draw_layer(_frame_id | SHADOW_MASK, shadow_tint)
	_draw_layer(_frame_id, _tint)
	_draw_layer(_frame_id | MAGIC_MASK, _tint)


func _draw_layer(frame_id: int, tint: Color) -> void:
	var frame: Dictionary = _resources.frame("selectchar", frame_id)
	if frame.is_empty():
		return
	draw_texture(frame.texture, _anchor + Vector2(frame.offset), tint)
