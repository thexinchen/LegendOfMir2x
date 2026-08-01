extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()

const WEAR_GRIDS := {
	4: Rect2(10, 240, 38, 56),
	5: Rect2(168, 88, 38, 38),
	6: Rect2(10, 155, 38, 38),
	7: Rect2(168, 155, 38, 38),
	8: Rect2(10, 195, 38, 38),
	9: Rect2(168, 195, 38, 38),
	10: Rect2(88, 265, 38, 38),
	11: Rect2(128, 265, 38, 38),
}


func _ready() -> void:
	super._ready()
	_state = get_node("/root/GameState")
	_resources.configure_default()
	_state.state_changed.connect(_refresh)
	_refresh()


func _refresh() -> void:
	$Name.text = _state.player_name
	$StateValues.text = "%d\n%d\n%d/%d\n%d/%d\n-\n-\n-\n-\n-" % [_state.player_level, _state.player_exp, _state.player_hp, _state.player_hp_max, _state.player_mp, _state.player_mp_max]
	$CombatStats.text = "攻击 %d - %d          防御 %d - %d\n魔法 -                 魔防 -                 道术 -" % [_state.dc_min, _state.dc_max, _state.ac_min, _state.ac_max]
	for child in $EquipmentSlots.get_children():
		child.free()
	for location in WEAR_GRIDS:
		var item: Dictionary = _state.wear.get(location, {})
		if item.is_empty():
			continue
		var frame: Dictionary = _resources.frame("item", _resources.item_package_gfx_id(item.get("itemID", 0)) | 0x01000000)
		if frame.is_empty():
			continue
		var grid: Rect2 = WEAR_GRIDS[location]
		var icon := TextureButton.new()
		icon.texture_normal = frame.texture
		icon.position = grid.position - $EquipmentSlots.position
		icon.size = grid.size
		icon.ignore_texture_size = true
		icon.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		icon.tooltip_text = "物品 %d" % item.get("itemID", 0)
		icon.pressed.connect(NetworkClient.send_request_grab_wear.bind(location))
		$EquipmentSlots.add_child(icon)
