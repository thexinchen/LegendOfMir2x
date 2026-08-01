extends Control

const JOB_WARRIOR := 1
const JOB_TAOIST := 2
const JOB_WIZARD := 4

const CHARACTER_TEXTURES := {
	Vector2i(JOB_WARRIOR, 0): preload("res://assets/characters/preview/warrior_female.png"),
	Vector2i(JOB_WARRIOR, 1): preload("res://assets/characters/preview/warrior_male.png"),
	Vector2i(JOB_WIZARD, 0): preload("res://assets/characters/preview/wizard_female.png"),
	Vector2i(JOB_WIZARD, 1): preload("res://assets/characters/preview/wizard_male.png"),
	Vector2i(JOB_TAOIST, 0): preload("res://assets/characters/preview/taoist_female.png"),
	Vector2i(JOB_TAOIST, 1): preload("res://assets/characters/preview/taoist_male.png"),
}

const CHARACTER_POSITIONS := {
	Vector2i(JOB_WARRIOR, 0): Vector2(527, 303),
	Vector2i(JOB_WARRIOR, 1): Vector2(198, 288),
	Vector2i(JOB_WIZARD, 0): Vector2(478, 296),
	Vector2i(JOB_WIZARD, 1): Vector2(201, 282),
	Vector2i(JOB_TAOIST, 0): Vector2(471, 302),
	Vector2i(JOB_TAOIST, 1): Vector2(202, 276),
}

@onready var male_sprite: TextureRect = %MaleSprite
@onready var female_sprite: TextureRect = %FemaleSprite
@onready var name_input: LineEdit = %NameInput
@onready var notice: Label = %Notice

var selected_job := JOB_WARRIOR
var selected_male := true


func _ready() -> void:
	AudioService.play_map_bgm(0x00040001)
	_update_characters()
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()
		return
	NetworkClient.message_received.connect(_on_server_message)
	name_input.grab_focus()


func _on_male_pressed() -> void:
	selected_male = true
	_update_characters()


func _on_female_pressed() -> void:
	selected_male = false
	_update_characters()


func _on_warrior_pressed() -> void:
	selected_job = JOB_WARRIOR
	_update_characters()


func _on_wizard_pressed() -> void:
	selected_job = JOB_WIZARD
	_update_characters()


func _on_taoist_pressed() -> void:
	selected_job = JOB_TAOIST
	_update_characters()


func _on_submit_pressed() -> void:
	if name_input.text.is_empty() or name_input.text.to_utf8_buffer().size() >= 64:
		_show_notice("无效的角色名")
		return
	var error := NetworkClient.create_character(name_input.text, selected_job, selected_male)
	if error != OK:
		_show_notice("服务器尚未连接")
	else:
		_show_notice("提交中")


func _on_exit_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/select_character.tscn")


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	if head_code == NetworkClient.SM_CREATECHAROK:
		get_tree().change_scene_to_file("res://scenes/account/select_character.tscn")
	elif head_code == NetworkClient.SM_CREATECHARERROR:
		var messages := {
			2: "一个账号只能创建一个角色",
			3: "角色名已被使用",
			4: "无效的角色名",
			5: "无效的角色性别",
			6: "无效的角色职业",
		}
		_show_notice(messages.get(payload[0] if not payload.is_empty() else 0, "创建角色失败"))


func _update_characters() -> void:
	_set_character(male_sprite, true)
	_set_character(female_sprite, false)
	male_sprite.modulate = Color.WHITE if selected_male else Color(0.5, 0.5, 0.5, 1.0)
	female_sprite.modulate = Color.WHITE if not selected_male else Color(0.5, 0.5, 0.5, 1.0)


func _set_character(sprite: TextureRect, male: bool) -> void:
	var key := Vector2i(selected_job, 1 if male else 0)
	var texture: Texture2D = CHARACTER_TEXTURES[key]
	sprite.texture = texture
	sprite.size = texture.get_size()
	sprite.position = CHARACTER_POSITIONS[key]


func _show_notice(message: String) -> void:
	notice.text = message
	notice.show()
