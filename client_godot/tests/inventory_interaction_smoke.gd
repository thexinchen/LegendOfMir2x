extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")
const ItemTooltipFormatter = preload("res://scripts/game/item_tooltip_formatter.gd")


class TooltipFixtureResource:
	extends RefCounted
	var attributes := {
		"weight": 7,
		"dc": PackedInt32Array([1, 2]), "mc": PackedInt32Array([0, 0]), "sc": PackedInt32Array([3, 4]),
		"ac": PackedInt32Array([5, 6]), "mac": PackedInt32Array([7, 8]),
		"dc_hit": 1, "mc_hit": 2, "dc_dodge": 3, "mc_dodge": 4, "speed": 5, "comfort": 6, "luck_curse": -3,
		"dc_elem": PackedInt32Array([1, 0, 0, 0, 0, 0, 0]),
		"ac_elem": PackedInt32Array([2, -3, 0, 0, 0, 0, 0]),
		"load": PackedInt32Array([1, 0, 3]),
	}
	var detail := {
		"duration": 40,
		"hp": PackedInt32Array([10, 11, 12]),
		"mp": PackedInt32Array([13, 14, 15]),
		"req": PackedInt32Array([16, 17, 18, 19]),
		"description": "测试描述",
		"job": "战士",
	}
	func item_attribute(_item_id: int) -> Dictionary: return attributes
	func item_detail(_item_id: int) -> Dictionary: return detail
	func item_name(_item_id: int) -> String: return "测试装备"
	func item_type(_item_id: int) -> String: return "武器"
	func buff_name(buff_id: int) -> String: return "测试BUFF" if buff_id == 9 else ""


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	var potion_id := _find_type(resources, "恢复药水")
	var weapon_id := _find_durable_type(resources, "武器")
	if potion_id == 0 or weapon_id == 0 or resources.item_types.size() < 1000:
		_fail("item type metadata incomplete")
		return
	if not _test_complete_tooltip_formatter():
		return
	var potion := _item(potion_id, 1, 3)
	var weapon := _item(weapon_id, 2, 1)
	var record_duration := int(resources.item_detail(weapon_id).get("duration", 0))
	weapon.duration = [maxi(2, record_duration / 2), record_duration]
	var buff_id := int(resources.buff_names.keys()[0])
	weapon.extAttrList = {1: _portable_s32(3), 39: _portable_s32(buff_id)}
	GameState.inventory = [potion, weapon]
	GameState.grabbed_item = {}
	var panel: Control = load("res://scenes/game/panels/inventory.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("ItemGrid").position != Vector2(18, 59) or panel.get_node("ItemGrid").size != Vector2(380, 380):
		_fail("inventory grid geometry mismatch")
		return
	if panel.get_node("Title").position.x + panel.get_node("Title").size.x / 2.0 != 238.0:
		_fail("inventory title center mismatch")
		return
	if panel.get_node("SliderTrack").position != Vector2(403, 56) or panel.get_node("SliderTrack").size != Vector2(22, 383):
		_fail("inventory slider hit area mismatch")
		return
	if panel.get_node("Slider").position != Vector2(403, 56) or panel.get_node("Slider").size != Vector2(16, 16):
		_fail("inventory slider thumb geometry mismatch")
		return
	if not panel.get_node("Emblem").visible or panel.get_node("Emblem").texture == null or panel.get_node("Emblem").position != Vector2(23, 14):
		_fail("inventory animated emblem missing")
		return
	var potion_lines: Array[String] = ItemTooltipFormatter.plain_layout_lines(potion, resources)
	if potion_lines.front() != "  " or potion_lines.back() != "  " or not potion_lines.has(" 【名称】%s " % resources.item_name(potion_id)) or not potion_lines.has(" 【重量】%d " % resources.item_weight(potion_id)):
		_fail("ordinary item tooltip layout mismatch")
		return
	var potion_text := "\n".join(potion_lines)
	if potion_text.contains("数量") or potion_text.contains("序号"):
		_fail("inventory tooltip retained invented count/sequence fields")
		return
	var weapon_lines: Array[String] = ItemTooltipFormatter.plain_layout_lines(weapon, resources)
	var weapon_dc: PackedInt32Array = resources.item_attribute(weapon_id).get("dc", PackedInt32Array([0, 0]))
	if not weapon_lines.has(" 【持久】%d/%d/%d " % [weapon.duration[0], weapon.duration[1], record_duration]):
		_fail("durable item tooltip omitted original durability triple")
		return
	if not weapon_lines.has(" 攻击 %d - %d（+3） " % [weapon_dc[0], weapon_dc[1] + 3]):
		_fail("equipment tooltip extra-attribute formatting mismatch")
		return
	if not weapon_lines.has(" 附加BUFF：%s " % resources.buff_name(buff_id)):
		_fail("equipment tooltip buff-name formatting mismatch")
		return
	panel.call("_show_item_tooltip", weapon)
	var tooltip := panel.get_node("ItemTooltip") as Panel
	if not tooltip.visible or tooltip.size != Vector2(220, maxf(40.0, 20.0 + weapon_lines.size() * 15.0)) or tooltip.get_child_count() != weapon_lines.size():
		_fail("custom inventory tooltip geometry mismatch")
		return
	var tooltip_label := tooltip.get_child(0) as Label
	if tooltip_label.text != weapon_lines[0] or tooltip_label.position != Vector2(10, 10) or tooltip_label.get_theme_color("font_color") != Color.WHITE or tooltip_label.get_theme_font_size("font_size") != 12 or not tooltip_label.get_theme_font("font").resource_path.ends_with("/01_Yahei.ttf"):
		_fail("custom inventory tooltip did not render authoritative plain-white lines")
		return
	GameState.state_changed.emit()
	if not tooltip.visible or panel.get("_tooltip_key") != "%d:2" % weapon_id or tooltip.get_child_count() != weapon_lines.size():
		_fail("inventory refresh did not preserve the hovered item tooltip")
		return
	panel.call("_update_tooltip_position", panel.get_viewport_rect().size + Vector2(100, 100))
	if tooltip.global_position != panel.get_viewport_rect().size - tooltip.size:
		_fail("custom inventory tooltip viewport clamp mismatch")
		return
	panel.call("_hide_item_tooltip")
	for item_button in panel.get_node("ItemGrid").get_children():
		if item_button is BaseButton and not item_button.tooltip_text.is_empty():
			_fail("inventory item retained engine-default tooltip")
			return
	var sort_button := panel.get_node("SortButton") as TextureButton
	var close_button := panel.get_node("CloseButton") as TextureButton
	if sort_button.modulate.a != 0.0 or close_button.modulate.a != 0.0 or not sort_button.tooltip_text.is_empty() or not close_button.tooltip_text.is_empty():
		_fail("inventory overlay chrome visible while idle or retained tooltip")
		return
	sort_button.mouse_entered.emit()
	if sort_button.modulate.a != 1.0 or close_button.modulate.a != 0.0:
		_fail("inventory sort hover overlay mismatch")
		return
	sort_button.mouse_exited.emit()
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	sort_button.pressed.emit()
	if AudioService.last_seff_id != AudioService.UI_CLICK_SEFF_ID:
		_fail("inventory texture button did not play original click SEFF")
		return
	if int(panel.get("_repack_index")) != 1:
		_fail("inventory sort did not advance the original repack sequence")
		return
	var weapon_size: Vector2i = panel.call("_item_grid_size", weapon_id)
	var base_sort_key: Array = [weapon_size.x * weapon_size.y, weapon_id, resources.item_type(weapon_id)]
	for method in range(6):
		var expected_key := base_sort_key.duplicate()
		var swap_index := method % expected_key.size()
		var first: Variant = expected_key[0]
		expected_key[0] = expected_key[swap_index]
		expected_key[swap_index] = first
		if panel.call("_repack_sort_key", weapon, method) != expected_key:
			_fail("inventory repack key rotation mismatch at method %d" % method)
			return
	var inventory_before_sort: Dictionary = {}
	for item: Dictionary in GameState.inventory:
		inventory_before_sort["%d:%d" % [item.itemID, item.seqID]] = item.count
	var repack_signatures: Dictionary = {_bin_signature(panel.get("_bins")): true}
	for _method in range(1, 6):
		panel.call("_repack")
		repack_signatures[_bin_signature(panel.get("_bins"))] = true
	if int(panel.get("_repack_index")) != 6 or repack_signatures.size() < 2:
		_fail("inventory repeated sort did not cycle through original layouts")
		return
	if not GameState.grabbed_item.is_empty() or GameState.inventory.size() != inventory_before_sort.size():
		_fail("inventory sort mutated authoritative item ownership")
		return
	for item: Dictionary in GameState.inventory:
		if inventory_before_sort.get("%d:%d" % [item.itemID, item.seqID], -1) != item.count:
			_fail("inventory sort changed item identity or count")
			return
	panel.call("_sync_bins")
	var bins: Dictionary = panel.get("_bins")
	if bins.size() != 2:
		_fail("inventory bin count mismatch")
		return
	var potion_key := "%d:1" % potion_id
	var weapon_key := "%d:2" % weapon_id
	var weapon_origin := Vector2i(int(bins[weapon_key].x), int(bins[weapon_key].y))
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	panel.call("_grab_item", potion_key)
	if GameState.grabbed_item.get("itemID", 0) != potion_id or GameState.inventory.size() != 1 or AudioService.last_seff_id != resources.item_sound_effect(potion_id):
		_fail("grab operation mismatch")
		return
	panel.call("_grab_item", weapon_key)
	var swapped_potion: Dictionary = panel.get("_bins").get(potion_key, {})
	if GameState.grabbed_item.get("itemID", 0) != weapon_id or Vector2i(int(swapped_potion.get("x", -1)), int(swapped_potion.get("y", -1))) != weapon_origin:
		_fail("occupied-cell grabbed item exchange mismatch")
		return
	panel.call("_grab_item", potion_key)
	if GameState.grabbed_item.get("itemID", 0) != potion_id or not panel.get("_bins").has(weapon_key):
		_fail("grabbed item exchange did not preserve the displaced item")
		return
	panel.call("_place_grabbed", Vector2i(5, 5))
	if not GameState.grabbed_item.is_empty() or GameState.inventory.size() != 2:
		_fail("place operation mismatch")
		return
	var placed_potion: Dictionary = panel.get("_bins").get(potion_key, {})
	var potion_size: Vector2i = panel.call("_item_grid_size", potion_id)
	var expected_potion_position := Vector2i(5 - floori(potion_size.x * 0.5), 5 - floori(potion_size.y * 0.5))
	if typeof(placed_potion.get("x")) != TYPE_INT or typeof(placed_potion.get("y")) != TYPE_INT or Vector2i(placed_potion.x, placed_potion.y) != expected_potion_position:
		_fail("empty-cell placement did not use C++ integer-centered grid coordinates")
		return
	panel.call("_grab_item", potion_key)
	panel.call("_place_grabbed", Vector2i(-1, -1))
	var fallback_potion: Dictionary = panel.get("_bins").get(potion_key, {})
	if fallback_potion.is_empty() or int(fallback_potion.x) < 0 or int(fallback_potion.y) < 0 or typeof(fallback_potion.x) != TYPE_INT or typeof(fallback_potion.y) != TYPE_INT:
		_fail("invalid grabbed-item target did not fall back to an integer first-fit position")
		return
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	panel.call("_consume_or_equip", potion_key)
	if AudioService.last_seff_id != resources.item_sound_effect(potion_id):
		_fail("inventory consume omitted original item sound")
		return
	AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
	panel.call("_consume_or_equip", weapon_key)
	if AudioService.last_seff_id != 0x0102006F:
		_fail("inventory equip omitted original weapon sound")
		return
	var no_range_wheel := InputEventMouseButton.new()
	no_range_wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	no_range_wheel.pressed = true
	panel.call("_on_grid_input", no_range_wheel)
	if panel.get("_scroll_value") != 0.0 or panel.get_node("Slider").position.y != 56.0:
		_fail("inventory wheel moved slider without a scroll range")
		return
	for index in range(120):
		var extra := _item(potion_id, 100 + index, 1)
		GameState.inventory.append(extra)
	panel.call("_repack")
	var slider_press := InputEventMouseButton.new()
	slider_press.button_index = MOUSE_BUTTON_LEFT
	slider_press.pressed = true
	slider_press.position = Vector2(11, 191)
	panel.call("_on_slider_input", slider_press)
	if panel.get_node("Slider").position.y != 240.0 or panel.get("_scroll_row") != roundi(panel.call("_max_scroll_row") * 0.5):
		_fail("inventory slider did not preserve continuous half-way position")
		return
	slider_press.position = Vector2(11, 382)
	panel.call("_on_slider_input", slider_press)
	if panel.get("_scroll_row") != panel.call("_max_scroll_row") or panel.get_node("Slider").position.y != 424.0 or panel.get_node("Slider").modulate != Color.WHITE:
		_fail("inventory slider drag-to-bottom mismatch")
		return
	var slider_release := InputEventMouseButton.new()
	slider_release.button_index = MOUSE_BUTTON_LEFT
	slider_release.pressed = false
	panel.call("_on_slider_input", slider_release)
	if panel.get_node("Slider").modulate != Color(0.5, 0.5, 0.5, 1.0):
		_fail("inventory slider idle tint mismatch")
		return
	var wheel_up := InputEventMouseButton.new()
	wheel_up.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel_up.pressed = true
	var bottom_row := int(panel.get("_scroll_row"))
	panel.call("_on_item_input", wheel_up, potion_key)
	if panel.get("_scroll_row") != bottom_row - 1:
		_fail("inventory wheel over occupied cell did not scroll")
		return
	GameState.inventory_operation = {"invOp": 3, "uid": 9876, "typeList": ["武器"]}
	panel.show()
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if panel.visible or GameState.inventory_operation.is_empty():
		_fail("inventory Escape did not preserve original operation state")
		return
	panel.show()
	close_button.pressed.emit()
	if panel.visible or not GameState.inventory_operation.is_empty():
		_fail("inventory close button did not clear operation state")
		return
	if not _test_start_inv_op_reader():
		return
	print("INVENTORY INTERACTION PASS: types=%d potion=%d weapon=%d bins=%d" % [resources.item_types.size(), potion_id, weapon_id, bins.size()])
	get_tree().quit()


func _bin_signature(bins: Dictionary) -> String:
	var parts: PackedStringArray = []
	for key: String in bins:
		var bin: Dictionary = bins[key]
		parts.append("%s@%d,%d" % [key, int(bin.x), int(bin.y)])
	parts.sort()
	return "|".join(parts)


func _test_start_inv_op_reader() -> bool:
	var archive := PackedByteArray([1])
	_s32(archive, 3)
	_u64(archive, 9876)
	_string(archive, "query_repair")
	_string(archive, "commit_repair")
	_u64(archive, 2)
	_string(archive, "武器")
	_string(archive, "衣服")
	archive.append(0)
	var reader := CerealReader.new(archive)
	var operation := reader.read_sd_start_inv_op()
	if not reader.valid or not reader.at_end() or operation.invOp != 3 or operation.uid != 9876 or operation.typeList != ["武器", "衣服"]:
		_fail("SDStartInvOp parse mismatch: %s" % reader.error)
		return false
	return true


func _find_type(resources: RefCounted, type_name: String) -> int:
	for item_id in resources.item_types:
		if resources.item_types[item_id] == type_name:
			return int(item_id)
	return 0


func _find_durable_type(resources: RefCounted, type_name: String) -> int:
	for item_id in resources.item_types:
		if resources.item_types[item_id] == type_name and int(resources.item_detail(int(item_id)).get("duration", 0)) > 0:
			return int(item_id)
	return 0


func _item(item_id: int, seq_id: int, count: int) -> Dictionary:
	return {"itemID": item_id, "seqID": seq_id, "count": count, "duration": [0, 0], "extAttrList": {}}


func _string(buf: PackedByteArray, value: String) -> void:
	var encoded := value.to_utf8_buffer()
	_u64(buf, encoded.size())
	buf.append_array(encoded)


func _s32(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 4)
	buf.encode_s32(offset, value)


func _u64(buf: PackedByteArray, value: int) -> void:
	var offset := buf.size()
	buf.resize(offset + 8)
	buf.encode_u32(offset, value & 0xFFFFFFFF)
	buf.encode_u32(offset + 4, (value >> 32) & 0xFFFFFFFF)


func _portable_s32(value: int) -> PackedByteArray:
	var result := PackedByteArray([1, 0, 0, 0, 0])
	result.encode_s32(1, value)
	return result


func _test_complete_tooltip_formatter() -> bool:
	var item := _item(1, 1, 1)
	item.duration = [30, 35]
	item.extAttrList = {
		1: _portable_s32(3), 2: _portable_s32(-2), 3: _portable_s32(0),
		6: _portable_s32(1), 12: _portable_s32(1), 13: _portable_s32(2),
		34: _portable_s32(2), 39: _portable_s32(9),
	}
	var lines: Array[String] = ItemTooltipFormatter.plain_layout_lines(item, TooltipFixtureResource.new())
	var expected := [
		" 【名称】测试装备 ", " 【类型】武器 ", " 【重量】7 ", " 【持久】30/35/40 ", " 测试描述 ",
		" 攻击 1 - 5（+3） ", " 魔法 0 - -2（-2） ", " 道术 3 - 4（+0） ", " 防御 5 - 6 ", " 魔防 7 - 8 ",
		" 命中 +2（+1） ", " 魔法命中 +2 ", " 闪避 +3 ", " 魔法闪避 +4 ", " 速度 +5 ", " 舒适度 +6 ",
		" 诅咒 +2（-1） ", " 生命上限 +12（+2） ", " 生命盗取 +11 ", " 生命恢复 +12 ",
		" 魔法上限 +13 ", " 魔法盗取 +14 ", " 魔法恢复 +15 ", " 攻击元素：火 +1 ",
		" 强防元素：火 +2 ", " 弱防元素：冰 +3 ", " 附加BUFF：测试BUFF ",
		" 身体负重 +1 ", " 武器负重 +2（+2） ", " 包裹负重 +3 ",
		" 需要攻击 16 ", " 需要魔法 17 ", " 需要道术 18 ", " 需要等级 19 ", " 需要职业 战士 ",
	]
	var previous := -1
	for expected_line in expected:
		var index := lines.find(expected_line, previous + 1)
		if index < 0:
			_fail("complete item tooltip line missing or out of order: %s" % expected_line)
			return false
		previous = index
	return true


func _fail(message: String) -> void:
	push_error("INVENTORY_INTERACTION_SMOKE %s" % message)
	get_tree().quit(1)
