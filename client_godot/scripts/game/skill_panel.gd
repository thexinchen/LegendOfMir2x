extends "res://scripts/game/closable_panel.gd"

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const PAGE_NAMES := ["火", "冰", "雷", "风", "神圣", "暗黑", "幻影", "无"]
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
@onready var learned_skills: Control = $PageViewport/LearnedSkills
@onready var selection_label: Label = $SelectionLabel
@onready var slider: TextureRect = $Slider
@onready var tab_buttons: Array[TextureButton] = [
	$Tab0, $Tab1, $Tab2, $Tab3, $Tab4, $Tab5, $Tab6, $Tab7,
]

var _state: Node
var _resources: RefCounted = ActorResourceScript.new()
var _selected_tab := 0
var _hovered_magic_id := 0
var _scroll := 0.0
var _scroll_reach := 0.0


func _ready() -> void:
	super._ready()
	_resources.configure_default()
	for index in tab_buttons.size():
		tab_buttons[index].pressed.connect(_select_tab.bind(index))
		tab_buttons[index].mouse_entered.connect(_show_tab_name.bind(index))
	_state = get_node("/root/GameState")
	_state.state_changed.connect(_refresh_learned_skills)
	$PageViewport.gui_input.connect(_on_page_input)
	_select_tab(0)


func _select_tab(index: int) -> void:
	_selected_tab = index
	_scroll = 0.0
	page.texture = PAGE_TEXTURES[index]
	for button_index in tab_buttons.size():
		tab_buttons[button_index].button_pressed = button_index == index
	selection_label.text = "元素【%s】" % PAGE_NAMES[index]
	_refresh_learned_skills()


func set_learned_skills(skill_nodes: Array[Control]) -> void:
	_clear_skills()
	for skill_node in skill_nodes:
		learned_skills.add_child(skill_node)


func _refresh_learned_skills() -> void:
	_clear_skills()
	var max_reach := 329.0
	for magic_value in _state.learned_magic:
		var magic: Dictionary = magic_value
		var magic_id: int = magic.get("magicID", 0)
		var layout: PackedInt32Array = _resources.skill_layout(magic_id)
		if layout.size() < 5 or layout[1] != _selected_tab:
			continue
		var frame: Dictionary = _resources.frame("proguse", layout[0])
		if frame.is_empty():
			continue
		var texture: Texture2D = frame.texture
		var button := TextureButton.new()
		button.position = Vector2(layout[2] * 60 + 12, layout[3] * 65 + 13)
		button.size = texture.get_size() + Vector2(8, 8)
		button.texture_normal = texture
		button.ignore_texture_size = true
		button.mouse_entered.connect(_show_magic.bind(magic_id))
		button.mouse_exited.connect(_hide_magic.bind(magic_id))
		learned_skills.add_child(button)

		var level_label := _make_overlay_label("1", 12, Color.YELLOW)
		level_label.position = Vector2(texture.get_width() - 2, texture.get_height() - 1)
		button.add_child(level_label)

		var key: int = _state.magic_keys.get(magic_id, 0)
		if key != 0:
			var key_label := _make_overlay_label(char(key).to_upper(), 20, Color(1.0, 0.5, 0.0, 0.88))
			key_label.position = Vector2(2, 2)
			key_label.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.88))
			key_label.add_theme_constant_override("outline_size", 2)
			button.add_child(key_label)
		max_reach = maxf(max_reach, button.position.y + button.size.y + 10)
	_scroll_reach = maxf(0.0, minf(max_reach, 553.0) - 329.0)
	_apply_scroll()


func _make_overlay_label(text_value: String, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	label.text = text_value
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	return label


func _clear_skills() -> void:
	for child in learned_skills.get_children():
		child.free()


func _show_tab_name(index: int) -> void:
	selection_label.text = "元素【%s】" % PAGE_NAMES[index]


func _show_magic(magic_id: int) -> void:
	_hovered_magic_id = magic_id
	selection_label.text = "元素【%s】%s" % [PAGE_NAMES[_selected_tab], _resources.magic_names.get(magic_id, "")]


func _hide_magic(magic_id: int) -> void:
	if _hovered_magic_id == magic_id:
		_hovered_magic_id = 0
		selection_label.text = "元素【%s】" % PAGE_NAMES[_selected_tab]


func _on_page_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_scroll = maxf(0.0, _scroll - 0.1)
			_apply_scroll()
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_scroll = minf(1.0, _scroll + 0.1)
			_apply_scroll()


func _apply_scroll() -> void:
	var offset := -_scroll_reach * _scroll
	page.position.y = offset
	learned_skills.position.y = offset
	slider.position.y = 67.0 + _scroll * 266.0


func _unhandled_key_input(event: InputEvent) -> void:
	if not visible or not event.pressed or event.echo:
		return
	if event.keycode == KEY_ESCAPE:
		hide()
		get_viewport().set_input_as_handled()
		return
	if _hovered_magic_id == 0:
		return
	var key: int = event.unicode
	if key >= 65 and key <= 90:
		key += 32
	if not ((key >= 48 and key <= 57) or (key >= 97 and key <= 122)):
		return
	var layout: PackedInt32Array = _resources.skill_layout(_hovered_magic_id)
	if layout.size() >= 5 and (layout[4] & 1) != 0:
		_state.add_chat_log("无法为被动技能设置快捷键：%s" % _resources.magic_names.get(_hovered_magic_id, ""), 1)
		get_viewport().set_input_as_handled()
		return
	for magic_id in _state.magic_keys.keys():
		if _state.magic_keys[magic_id] == key:
			_state.magic_keys.erase(magic_id)
	_state.magic_keys[_hovered_magic_id] = key
	NetworkClient.send_set_magic_key(_hovered_magic_id, key)
	_refresh_learned_skills()
	get_viewport().set_input_as_handled()
