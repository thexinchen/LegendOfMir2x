extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const PreviewScript = preload("res://scripts/account/account_character_preview.gd")

const JOB_WARRIOR := 1
const JOB_TAOIST := 2
const JOB_WIZARD := 4

@onready var male_sprite: Control = %MaleSprite
@onready var female_sprite: Control = %FemaleSprite
@onready var name_input: LineEdit = %NameInput
@onready var notice: Control = %Notice

var selected_job := JOB_WARRIOR
var selected_male := true
var _resources: RefCounted = ActorResourceScript.new()
var _animation_time_ms := 0.0
var _last_sound_frame := -1


func _ready() -> void:
	_resources.configure_default()
	AudioService.play_map_bgm(0x00040001)
	_update_characters()
	if OS.has_environment("MIR2X_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()
		return
	NetworkClient.message_received.connect(_on_server_message)
	name_input.grab_focus()


func _process(delta: float) -> void:
	_animation_time_ms += delta * 1000.0
	_update_characters()
	_play_cycle_sound()


func _on_male_pressed() -> void:
	selected_male = true
	_restart_animation()
	_update_characters()


func _on_female_pressed() -> void:
	selected_male = false
	_restart_animation()
	_update_characters()


func _on_warrior_pressed() -> void:
	selected_job = JOB_WARRIOR
	_restart_animation()
	_update_characters()


func _on_wizard_pressed() -> void:
	selected_job = JOB_WIZARD
	_restart_animation()
	_update_characters()


func _on_taoist_pressed() -> void:
	selected_job = JOB_TAOIST
	_restart_animation()
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


func _set_character(sprite: Control, male: bool) -> void:
	var frame_count: int = PreviewScript.frame_count(selected_job, male, 4)
	var frame := _absolute_frame() % frame_count if male == selected_male and frame_count > 0 else 0
	var anchor := Vector2(193, 215) if male else Vector2(495, 220 + (40 if selected_job == JOB_WARRIOR else 0))
	var tint := Color.WHITE if male == selected_male else Color(0.5, 0.5, 0.5, 1.0)
	sprite.show_frame(_resources, anchor, PreviewScript.create_base_id(selected_job, male) + frame, tint)


func _restart_animation() -> void:
	_animation_time_ms = 0.0
	_last_sound_frame = -1


func _absolute_frame() -> int:
	return roundi(_animation_time_ms / 200.0)


func _play_cycle_sound() -> void:
	var frame_count: int = PreviewScript.frame_count(selected_job, selected_male, 4)
	var frame := _absolute_frame()
	if frame_count <= 0 or frame % frame_count != 0 or frame == _last_sound_frame:
		return
	_last_sound_frame = frame
	# Match the 0/1/2 job layout present in the original SEFF database. The C++
	# `job - JOB_BEGIN` expression incorrectly maps the bit-flag wizard job (4) to 3.
	var job_index: int = {JOB_WARRIOR: 0, JOB_TAOIST: 1, JOB_WIZARD: 2}.get(selected_job, 0)
	var seff_id := 0x00010000 | ((1 if selected_male else 0) << 4) | (job_index << 8)
	AudioService.play_seff_at(seff_id, 0, 0, 0, 0)


func _show_notice(message: String) -> void:
	notice.show_message(message)
