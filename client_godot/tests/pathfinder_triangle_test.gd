extends SceneTree

# Standalone test for world_pathfinder.gd overshoot fix.
# Run with: godot --headless --script tests/pathfinder_triangle_test.gd
# Verifies that straight-line moves no longer produce triangle paths.

const PathfinderScript = preload("res://scripts/game/world_pathfinder.gd")

const W := 400
const H := 400

var _pathfinder: RefCounted


func _init() -> void:
	_pathfinder = PathfinderScript.new()
	var failures: Array[String] = []
	_run_cases(failures)
	if failures.is_empty():
		print("pathfinder_triangle_test: ALL PASSED")
		quit(0)
	else:
		for f in failures:
			print("FAIL: ", f)
		print("pathfinder_triangle_test: %d FAILURES" % failures.size())
		quit(1)


func _can_walk(x: int, y: int) -> bool:
	return x >= 0 and y >= 0 and x < W and y < H


func _run_cases(failures: Array[String]) -> void:
	# Case 1: the exact triangle from the trace log.
	# (310,269) -> (305,266): dx=-5, dy=-3. Old pathfinder produced
	# up-left 2, up-left 2, down-left 1 (a triangle). The fix should
	# produce a path that never reverses on the y axis.
	_check_path(Vector2i(310, 269), Vector2i(305, 266), 2, failures, "trace_case_310_269_to_305_266")

	# Case 2: repeat-trace case (311,268) -> (306,265): dx=-5, dy=-3.
	_check_path(Vector2i(311, 268), Vector2i(306, 265), 2, failures, "trace_case_311_268_to_306_265")

	# Case 3: pure horizontal, no triangle possible — sanity.
	_check_straight(Vector2i(10, 10), Vector2i(20, 10), 2, failures, "horizontal_10_to_20")

	# Case 4: pure vertical.
	_check_straight(Vector2i(10, 10), Vector2i(10, 20), 2, failures, "vertical_10_to_20")

	# Case 5: exact diagonal (dx == dy), should be single direction.
	_check_straight(Vector2i(10, 10), Vector2i(20, 20), 2, failures, "diagonal_10_10_to_20_20")

	# Case 6: dx=7, dy=3 — heavy x, light y. Classic triangle risk.
	_check_path(Vector2i(30, 30), Vector2i(23, 27), 2, failures, "dx7_dy3")

	# Case 7: dx=3, dy=7 — heavy y, light x.
	_check_path(Vector2i(30, 30), Vector2i(27, 23), 2, failures, "dx3_dy7")


func _check_path(start: Vector2i, goal: Vector2i, max_step: int, failures: Array[String], label: String) -> void:
	var goals: Array[Vector2i] = [goal]
	var path: Array[Vector2i] = _pathfinder.find_path(start, goals, _can_walk, {}, 50000, max_step)
	if path.is_empty():
		failures.append("%s: no path found" % label)
		return
	# Reconstruct full point list including start.
	var points: Array[Vector2i] = [start]
	for p in path:
		points.append(p)
	if points[points.size() - 1] != goal:
		failures.append("%s: path ends at %s, expected %s" % [label, points[points.size() - 1], goal])
		return
	# A triangle is a reversal on either axis. Track the running sign of each
	# axis delta; if it flips (and the axis wasn't zero), that's a reversal.
	var prev_sx := 0
	var prev_sy := 0
	for i in range(1, points.size()):
		var dx := points[i].x - points[i - 1].x
		var dy := points[i].y - points[i - 1].y
		var sx := signi(dx)
		var sy := signi(dy)
		if sx != 0 and prev_sx != 0 and sx != prev_sx:
			failures.append("%s: x axis reversed at hop %d (%s -> %s): path=%s" % [label, i, points[i - 1], points[i], points])
			return
		if sy != 0 and prev_sy != 0 and sy != prev_sy:
			failures.append("%s: y axis reversed at hop %d (%s -> %s): path=%s" % [label, i, points[i - 1], points[i], points])
			return
		if sx != 0:
			prev_sx = sx
		if sy != 0:
			prev_sy = sy
	print("%s: OK path=%s" % [label, points])


func _check_straight(start: Vector2i, goal: Vector2i, max_step: int, failures: Array[String], label: String) -> void:
	var goals: Array[Vector2i] = [goal]
	var path: Array[Vector2i] = _pathfinder.find_path(start, goals, _can_walk, {}, 50000, max_step)
	if path.is_empty():
		failures.append("%s: no path found" % label)
		return
	var points: Array[Vector2i] = [start]
	for p in path:
		points.append(p)
	if points[points.size() - 1] != goal:
		failures.append("%s: path ends at %s, expected %s" % [label, points[points.size() - 1], goal])
		return
	# All hops must share the same direction sign as the overall move.
	var overall_sx := signi(goal.x - start.x)
	var overall_sy := signi(goal.y - start.y)
	for i in range(1, points.size()):
		var dx := points[i].x - points[i - 1].x
		var dy := points[i].y - points[i - 1].y
		if signi(dx) != overall_sx and overall_sx != 0:
			failures.append("%s: x hop sign mismatch at hop %d: path=%s" % [label, i, points])
			return
		if signi(dy) != overall_sy and overall_sy != 0:
			failures.append("%s: y hop sign mismatch at hop %d: path=%s" % [label, i, points])
			return
	print("%s: OK path=%s" % [label, points])
