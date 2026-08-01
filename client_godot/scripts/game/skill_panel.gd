extends "res://scripts/game/closable_panel.gd"

const PAGE_TEXTURES: Array[Texture2D] = [
	preload("res://assets/ui/game/skill/05000010.png"),
	preload("res://assets/ui/game/skill/05000011.png"),
	preload("res://assets/ui/game/skill/05000012.png"),
	preload("res://assets/ui/game/skill/05000013.png"),
	preload("res://assets/ui/game/skill/05000014.png"),
	preload("res://assets/ui/game/skill/05000015.png"),
	preload("res://assets/ui/game/skill/05000016.png"),
	preload("res://assets/ui/game/skill/05000017.png"),
]

@onready var page: TextureRect = $PageViewport/Page
@onready var tab_buttons: Array[TextureButton] = [
	$Tab0, $Tab1, $Tab2, $Tab3, $Tab4, $Tab5, $Tab6, $Tab7,
]
var _state: Node


func _ready() -> void:
	super._ready()
	for index in tab_buttons.size():
		tab_buttons[index].pressed.connect(_select_tab.bind(index))
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh_learned_skills)
	_select_tab(0)
	_refresh_learned_skills()


func _select_tab(index: int) -> void:
	page.texture = PAGE_TEXTURES[index]
	for button_index in tab_buttons.size():
		tab_buttons[button_index].button_pressed = button_index == index


func set_learned_skills(skill_nodes: Array[Control]) -> void:
	for child in $PageViewport/LearnedSkills.get_children():
		child.queue_free()
	for skill_node in skill_nodes:
		$PageViewport/LearnedSkills.add_child(skill_node)


func _refresh_learned_skills() -> void:
	for child in $PageViewport/LearnedSkills.get_children():
		child.free()
	for index in range(_state.learned_magic.size()):
		var magic: Dictionary = _state.learned_magic[index]
		var button := Button.new()
		button.position = Vector2((index % 4) * 64, (index / 4) * 50)
		button.size = Vector2(60, 46)
		var magic_id: int = magic.get("magicID", 0)
		button.text = "%d\n%s" % [magic_id, char(_state.magic_keys.get(magic_id, 0)) if _state.magic_keys.get(magic_id, 0) else "-"]
		button.tooltip_text = "技能 %d，经验 %d" % [magic_id, magic.get("exp", 0)]
		$PageViewport/LearnedSkills.add_child(button)
