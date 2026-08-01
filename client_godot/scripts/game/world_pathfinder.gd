extends RefCounted

const DIRECTIONS := [
	Vector2i(-1, -1), Vector2i(0, -1), Vector2i(1, -1),
	Vector2i(-1, 0),                         Vector2i(1, 0),
	Vector2i(-1, 1),  Vector2i(0, 1),  Vector2i(1, 1),
]


func find_path(start: Vector2i, goals: Array[Vector2i], can_walk: Callable, occupied: Dictionary, max_nodes := 50000) -> Array[Vector2i]:
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
	_push_heap(open, {"point": start, "score": _heuristic(start, valid_goals)})
	var expanded := 0
	while not open.is_empty() and expanded < max_nodes:
		var entry := _pop_heap(open)
		var current: Vector2i = entry.point
		var current_cost: int = best_cost.get(current, 0x7FFFFFFF)
		if int(entry.score) > current_cost + _heuristic(current, valid_goals):
			continue
		if goal_set.has(current):
			return _reconstruct(came_from, start, current)
		expanded += 1
		for direction_value in DIRECTIONS:
			var direction: Vector2i = direction_value
			var next: Vector2i = current + direction
			if occupied.has(next) or not can_walk.call(next.x, next.y):
				continue
			var next_cost := current_cost + (14 if direction.x != 0 and direction.y != 0 else 10)
			if next_cost >= int(best_cost.get(next, 0x7FFFFFFF)):
				continue
			best_cost[next] = next_cost
			came_from[next] = current
			_push_heap(open, {"point": next, "score": next_cost + _heuristic(next, valid_goals)})
	return []


func _heuristic(point: Vector2i, goals: Array[Vector2i]) -> int:
	var best := 0x7FFFFFFF
	for goal in goals:
		var dx := absi(point.x - goal.x)
		var dy := absi(point.y - goal.y)
		best = mini(best, 10 * maxi(dx, dy) + 4 * mini(dx, dy))
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
