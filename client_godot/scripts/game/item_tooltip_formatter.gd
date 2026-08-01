class_name ItemTooltipFormatter
extends RefCounted

const DEFAULT_DESCRIPTION := "游戏处于开发阶段，暂无物品描述。"
const ELEMENT_NAMES := ["火", "冰", "雷", "风", "神圣", "暗黑", "幻影"]


static func plain_layout_lines(item: Dictionary, resources: RefCounted, price: Variant = null) -> Array[String]:
	var item_id := int(item.get("itemID", 0))
	var attributes: Dictionary = resources.item_attribute(item_id)
	var detail: Dictionary = resources.item_detail(item_id)
	var lines: Array[String] = ["  "] # stripped <layout> wrapper in the C++ renderer
	lines.append(_line("【名称】%s" % resources.item_name(item_id)))
	lines.append(_line("【类型】%s" % resources.item_type(item_id)))
	lines.append(_line("【重量】%d" % int(attributes.get("weight", 0))))
	if price != null:
		lines.append(_line("【售价】%d" % int(price)))

	var record_duration := int(detail.get("duration", 0))
	if record_duration > 0:
		var duration: Array = item.get("duration", [0, 0])
		var current := int(duration[0]) if duration.size() >= 1 else 0
		var maximum := int(duration[1]) if duration.size() >= 2 else 0
		lines.append(_line("【持久】%d/%d/%d" % [current, maximum, record_duration]))

	lines.append("  ")
	var description := str(detail.get("description", ""))
	lines.append(_line(description if not description.is_empty() else DEFAULT_DESCRIPTION))
	lines.append("  ")

	_append_pair(lines, item, attributes, "dc", "攻击", 1)
	_append_pair(lines, item, attributes, "mc", "魔法", 2)
	_append_pair(lines, item, attributes, "sc", "道术", 3)
	_append_pair(lines, item, attributes, "ac", "防御", 4)
	_append_pair(lines, item, attributes, "mac", "魔防", 5)

	_append_scalar(lines, item, int(attributes.get("dc_hit", 0)), "命中", 6)
	_append_scalar(lines, item, int(attributes.get("mc_hit", 0)), "魔法命中", 7)
	_append_scalar(lines, item, int(attributes.get("dc_dodge", 0)), "闪避", 8)
	_append_scalar(lines, item, int(attributes.get("mc_dodge", 0)), "魔法闪避", 9)
	_append_scalar(lines, item, int(attributes.get("speed", 0)), "速度", 10)
	_append_scalar(lines, item, int(attributes.get("comfort", 0)), "舒适度", 11)
	_append_luck_curse(lines, item, int(attributes.get("luck_curse", 0)))

	var hp: PackedInt32Array = detail.get("hp", PackedInt32Array())
	var mp: PackedInt32Array = detail.get("mp", PackedInt32Array())
	_append_scalar(lines, item, _array_value(hp, 0), "生命上限", 13)
	_append_scalar(lines, item, _array_value(hp, 1), "生命盗取", 14)
	_append_scalar(lines, item, _array_value(hp, 2), "生命恢复", 15)
	_append_scalar(lines, item, _array_value(mp, 0), "魔法上限", 16)
	_append_scalar(lines, item, _array_value(mp, 1), "魔法盗取", 17)
	_append_scalar(lines, item, _array_value(mp, 2), "魔法恢复", 18)

	_append_elements(lines, attributes.get("dc_elem", PackedInt32Array()), true)
	_append_elements(lines, attributes.get("ac_elem", PackedInt32Array()), false)

	var buff := _ext_attr(item, 39)
	if buff.present and int(buff.value) != 0:
		var name: String = resources.buff_name(int(buff.value))
		if not name.is_empty():
			lines.append(_line("附加BUFF：%s" % name))

	var load: PackedInt32Array = attributes.get("load", PackedInt32Array())
	if _array_value(load, 0) > 0 or _array_value(load, 1) > 0 or _array_value(load, 2) > 0:
		lines.append("  ")
		_append_scalar(lines, item, _array_value(load, 0), "身体负重", 33)
		_append_scalar(lines, item, _array_value(load, 1), "武器负重", 34)
		_append_scalar(lines, item, _array_value(load, 2), "包裹负重", 35)

	var req: PackedInt32Array = detail.get("req", PackedInt32Array())
	var job := str(detail.get("job", ""))
	if _array_value(req, 0) > 0 or _array_value(req, 1) > 0 or _array_value(req, 2) > 0 or _array_value(req, 3) > 0 or not job.is_empty():
		lines.append("  ")
		if _array_value(req, 0) > 0: lines.append(_line("需要攻击 %d" % _array_value(req, 0)))
		if _array_value(req, 1) > 0: lines.append(_line("需要魔法 %d" % _array_value(req, 1)))
		if _array_value(req, 2) > 0: lines.append(_line("需要道术 %d" % _array_value(req, 2)))
		if _array_value(req, 3) > 0: lines.append(_line("需要等级 %d" % _array_value(req, 3)))
		if not job.is_empty(): lines.append(_line("需要职业 %s" % job))

	lines.append("  ") # stripped </layout> wrapper in the C++ renderer
	return lines


static func _append_pair(lines: Array[String], item: Dictionary, attributes: Dictionary, key: String, label: String, attr_type: int) -> void:
	var pair: PackedInt32Array = attributes.get(key, PackedInt32Array())
	var minimum := _array_value(pair, 0)
	var maximum := _array_value(pair, 1)
	var ext := _ext_attr(item, attr_type)
	if minimum > 0 or maximum > 0 or (ext.present and int(ext.value) != 0):
		var suffix := "（%s）" % _signed(int(ext.value)) if ext.present else ""
		lines.append(_line("%s %d - %d%s" % [label, minimum, maximum + int(ext.value), suffix]))


static func _append_scalar(lines: Array[String], item: Dictionary, base_value: int, label: String, attr_type: int) -> void:
	var ext := _ext_attr(item, attr_type)
	if base_value != 0 or (ext.present and int(ext.value) != 0):
		var suffix := "（%s）" % _signed(int(ext.value)) if ext.present else ""
		lines.append(_line("%s %s%s" % [label, _signed(base_value + int(ext.value)), suffix]))


static func _append_luck_curse(lines: Array[String], item: Dictionary, base_value: int) -> void:
	var ext := _ext_attr(item, 12)
	var ext_value := int(ext.value)
	if base_value == 0 and ext_value == 0:
		return
	var total := base_value + ext_value
	if total >= 0:
		var suffix := "（%s）" % _signed(ext_value) if ext_value != 0 else ""
		lines.append(_line("幸运 %s%s" % [_signed(total), suffix]))
	else:
		var suffix := "（%s）" % _signed(-ext_value) if ext_value != 0 else ""
		lines.append(_line("诅咒 %s%s" % [_signed(absi(total)), suffix]))


static func _append_elements(lines: Array[String], values: PackedInt32Array, attack: bool) -> void:
	for index in range(ELEMENT_NAMES.size()):
		var value := _array_value(values, index)
		if attack and value > 0:
			lines.append(_line("攻击元素：%s %s" % [ELEMENT_NAMES[index], _signed(value)]))
		elif not attack and value > 0:
			lines.append(_line("强防元素：%s %s" % [ELEMENT_NAMES[index], _signed(value)]))
		elif not attack and value < 0:
			lines.append(_line("弱防元素：%s %s" % [ELEMENT_NAMES[index], _signed(absi(value))]))


static func _ext_attr(item: Dictionary, attr_type: int) -> Dictionary:
	var attributes: Dictionary = item.get("extAttrList", {})
	if not attributes.has(attr_type):
		return {"present": false, "value": 0}
	var data: PackedByteArray = attributes[attr_type]
	if data.size() >= 5 and data[0] == 1:
		return {"present": true, "value": data.decode_s32(1)}
	if data.size() >= 4:
		return {"present": true, "value": data.decode_s32(0)}
	return {"present": false, "value": 0}


static func _array_value(values: PackedInt32Array, index: int) -> int:
	return int(values[index]) if index >= 0 and index < values.size() else 0


static func _signed(value: int) -> String:
	return "+%d" % value if value >= 0 else str(value)


static func _line(text: String) -> String:
	return " %s " % text
