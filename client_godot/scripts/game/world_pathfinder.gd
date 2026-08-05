extends RefCounted

const DIRECTIONS := [
	Vector2i(-1, -1), Vector2i(0, -1), Vector2i(1, -1),
	Vector2i(-1, 0),                         Vector2i(1, 0),
	Vector2i(-1, 1),  Vector2i(0, 1),  Vector2i(1, 1),
]


func find_path(start: Vector2i, goals: Array[Vector2i], can_walk: Callable, occupied: Dictionary, max_nodes := 50000, max_step := 1) -> Array[Vector2i]:
	max_step = clampi(max_step, 1, 3)
	var goal_set := {}
	var valid_goals: Array[Vector2i] = []
	for goal in goals:
		if can_walk.call(goal.x, goal.y) and (not occupied.has(goal) or goal == start):
			goal_set[goal] = true
			valid_goals.append(goal)
	if goal_set.is_empty() or goal_set.has(start):
		return []

	var open: Array[Dictionary] = []
	var came_from := {}
	var best_cost := {start: 0}
	_push_heap(open, {"point": start, "score": _heuristic(start, valid_goals, max_step)})
	var expanded := 0
	while not open.is_empty() and expanded < max_nodes:
		var entry := _pop_heap(open)
		var current: Vector2i = entry.point
		var current_cost: int = best_cost.get(current, 0x7FFFFFFF)
		if int(entry.score) > current_cost + _heuristic(current, valid_goals, max_step):
			continue
		if goal_set.has(current):
			return _normalize_straight_segments(start, _reconstruct(came_from, start, current), max_step)
		expanded += 1
		for direction_value in DIRECTIONS:
			var direction: Vector2i = direction_value
			for hop_size in range(max_step, 0, -1):
				var next: Vector2i = current + direction * hop_size
				var reachable := true
				for distance in range(1, hop_size + 1):
					var crossed := current + direction * distance
					if occupied.has(crossed) or not can_walk.call(crossed.x, crossed.y):
						reachable = false
						break
				if not reachable:
					continue
				var hop_cost := (14 if direction.x != 0 and direction.y != 0 else 10) if max_step == 1 else 10 + hop_size
				var next_cost := current_cost + hop_cost
				if next_cost >= int(best_cost.get(next, 0x7FFFFFFF)):
					continue
				best_cost[next] = next_cost
				came_from[next] = current
				_push_heap(open, {"point": next, "score": next_cost + _heuristic(next, valid_goals, max_step)})
	return []


func _normalize_straight_segments(start: Vector2i, path: Array[Vector2i], max_step: int) -> Array[Vector2i]:
	if max_step <= 1 or path.size() < 2:
		return path
	var normalized: Array[Vector2i] = []
	var normalized_cursor := start
	var index := 0
	while index < path.size():
		var original_previous: Vector2i = start if index == 0 else path[index - 1]
		var first_delta: Vector2i = path[index] - original_previous
		var direction := Vector2i(signi(first_delta.x), signi(first_delta.y))
		var segment_distance := 0
		var segment_end := index
		var scan_previous := original_previous
		while segment_end < path.size():
			var delta: Vector2i = path[segment_end] - scan_previous
			if Vector2i(signi(delta.x), signi(delta.y)) != direction:
				break
			segment_distance += maxi(absi(delta.x), absi(delta.y))
			scan_previous = path[segment_end]
			segment_end += 1
		while segment_distance >= max_step:
			normalized_cursor += direction * max_step
			normalized.append(normalized_cursor)
			segment_distance -= max_step
		if segment_distance > 0:
			normalized_cursor += direction * segment_distance
			normalized.append(normalized_cursor)
		index = segment_end
	return normalized


func _heuristic(point: Vector2i, goals: Array[Vector2i], max_step: int) -> int:
	var best := 0x7FFFFFFF
	for goal in goals:
		var dx := absi(point.x - goal.x)
		var dy := absi(point.y - goal.y)
		if max_step == 1:
			best = mini(best, 10 * maxi(dx, dy) + 4 * mini(dx, dy))
			continue
		var distance := maxi(dx, dy)
		best = mini(best, (distance / max_step) * (10 + max_step) + (10 + distance % max_step if distance % max_step else 0))
	return best


func _reconstruct(came_from: Dictionary, start: Vector2i, goal: Vector2i) -> Array[Vector2i]:
	var path: Array[Vector2i] = []
	var current: Vector2i = goal
	while current != start:
		path.push_front(current)
		current = came_from[current]
	return path


func _push_heap(heap: Array[Dictionary], entry: Dictionary) -> void:
	heap.append(entry)
	var index := heap.size() - 1
	while index > 0:
		var parent: int = (index - 1) >> 1
		if int(heap[parent].score) <= int(entry.score):
			break
		heap[index] = heap[parent]
		index = parent
	heap[index] = entry


func _pop_heap(heap: Array[Dictionary]) -> Dictionary:
	var result: Dictionary = heap[0]
	var tail: Dictionary = heap.pop_back()
	if heap.is_empty():
		return result
	var index := 0
	while true:
		var left: int = index * 2 + 1
		if left >= heap.size():
			break
		var right: int = left + 1
		var child: int = right if right < heap.size() and int(heap[right].score) < int(heap[left].score) else left
		if int(heap[child].score) >= int(tail.score):
			break
		heap[index] = heap[child]
		index = child
	heap[index] = tail
	return result
