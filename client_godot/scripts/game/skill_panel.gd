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


func _ready() -> void:
	super._ready()
	for index in tab_buttons.size():
		tab_buttons[index].pressed.connect(_select_tab.bind(index))
	_select_tab(0)


func _select_tab(index: int) -> void:
	page.texture = PAGE_TEXTURES[index]
	for button_index in tab_buttons.size():
		tab_buttons[button_index].button_pressed = button_index == index


func set_learned_skills(skill_nodes: Array[Control]) -> void:
	for child in $PageViewport/LearnedSkills.get_children():
		child.queue_free()
	for skill_node in skill_nodes:
		$PageViewport/LearnedSkills.add_child(skill_node)
