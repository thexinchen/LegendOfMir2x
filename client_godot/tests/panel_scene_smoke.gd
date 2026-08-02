extends Node

const PANEL_SCENES := [
	"res://scenes/game/control_panel.tscn",
	"res://scenes/game/panels/inventory.tscn",
	"res://scenes/game/panels/player_state.tscn",
	"res://scenes/game/panels/skill.tscn",
	"res://scenes/game/panels/horse.tscn",
	"res://scenes/game/panels/guild.tscn",
	"res://scenes/game/panels/quest.tscn",
	"res://scenes/game/panels/team.tscn",
	"res://scenes/game/panels/secured_items.tscn",
	"res://scenes/game/panels/purchase.tscn",
	"res://scenes/game/panels/input_string.tscn",
	"res://scenes/game/panels/auction.tscn",
	"res://scenes/game/panels/friend_chat.tscn",
	"res://scenes/game/panels/runtime_config.tscn",
	"res://scenes/game/panels/npc_chat.tscn",
	"res://scenes/game/panels/minimap.tscn",
]


func _ready() -> void:
	for scene_path in PANEL_SCENES:
		var packed := load(scene_path) as PackedScene
		if not packed:
			_fail("cannot load %s" % scene_path)
			return
		var panel := packed.instantiate()
		if not panel.get_script():
			panel.free()
			_fail("root script failed to load in %s" % scene_path)
			return
		if _texture_count(panel) == 0:
			panel.free()
			_fail("no texture found in %s" % scene_path)
			return
		if scene_path.ends_with("/horse.tscn") and not _check_horse_layers(panel):
			panel.free()
			return
		panel.free()
		print("PANEL PASS: ", scene_path)
	print("ALL PANEL SCENES PASS: ", PANEL_SCENES.size())
	get_tree().quit()


func _texture_count(node: Node) -> int:
	var count := 0
	if node is TextureRect and node.texture:
		count += 1
	elif node is NinePatchRect and node.texture:
		count += 1
	elif node is TextureButton and node.texture_normal:
		count += 1
	for child in node.get_children():
		count += _texture_count(child)
	return count


func _check_horse_layers(panel: Node) -> bool:
	var backing := panel.get_node_or_null("GrayBacking") as ColorRect
	var viewport := panel.get_node_or_null("HorseViewport") as ColorRect
	var background := panel.get_node_or_null("Background") as TextureRect
	if backing == null or backing.position != Vector2.ZERO or backing.size != Vector2(257, 322) or not backing.color.is_equal_approx(Color8(128, 128, 128)):
		_fail("horse panel does not preserve the C++ opaque gray backing: node=%s position=%s size=%s color=%s" % [backing, backing.position if backing else Vector2.INF, backing.size if backing else Vector2.INF, backing.color if backing else Color.TRANSPARENT])
		return false
	if viewport == null or background == null or not (backing.get_index() < viewport.get_index() and viewport.get_index() < background.get_index()):
		_fail("horse panel draw order is not gray backing -> black viewport -> original texture")
		return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
