extends Control

const Protocol = preload("res://scripts/network/protocol.gd")
const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")
const PreviewScript = preload("res://scripts/account/account_character_preview.gd")

const JOB_WARRIOR := 1
const JOB_TAOIST := 2
const JOB_WIZARD := 4

@onready var character_sprite: Control = %CharacterSprite
@onready var character_info: Label = %CharacterInfo
@onready var notice: Control = %Notice
@onready var delete_dialog: Control = %DeleteCharacterDialog

var has_character := false
var _query_complete := false
var character_name := "预览角色"
var character_gender := 1
var character_job := JOB_WARRIOR
var character_exp := 0
var _resources: RefCounted = ActorResourceScript.new()
var _animation_time_ms := 0.0
var _character_motion := 0
var _last_motion_switch_frame := 0


func _ready() -> void:
	_resources.configure_default()
	AudioService.play_map_bgm(0x00040002)
	if OS.has_environment("MIR2X_SCREENSHOT"):
		has_character = true
		_query_complete = true
		_update_character_preview()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_SCREENSHOT"))
		get_tree().quit()
		return
	character_sprite.hide()
	$InfoPanel.hide()
	character_info.hide()
	_update_button_visibility()
	NetworkClient.message_received.connect(_on_server_message)
	delete_dialog.confirmed.connect(_on_delete_confirmed)
	delete_dialog.canceled.connect(_on_delete_canceled)
	delete_dialog.hide()
	var error := NetworkClient.query_character()
	if error != OK:
		_show_notice("服务器尚未连接")
	else:
		_show_notice("正在下载游戏角色")


func _process(delta: float) -> void:
	_animation_time_ms += delta * 1000.0
	if has_character:
		_switch_character_motion()
		_update_character_preview()


func _on_start_pressed() -> void:
	if not has_character:
		_show_notice("请先创建游戏角色")
	elif NetworkClient.enter_game() != OK:
		_show_notice("服务器尚未连接")


func _on_create_pressed() -> void:
	if not _query_complete:
		_show_notice("正在下载游戏角色")
	elif has_character:
		_show_notice("一个账号只能创建一个游戏角色")
	else:
		get_tree().change_scene_to_file("res://scenes/account/create_character.tscn")


func _on_delete_pressed() -> void:
	if has_character:
		delete_dialog.open()


func _on_exit_pressed() -> void:
	get_tree().change_scene_to_file("res://scenes/account/login.tscn")


func _on_delete_confirmed(password: String) -> void:
	if not has_character:
		_show_notice("此账号没有角色")
		return
	if password.is_empty():
		_show_notice("无效的密码")
		return
	if NetworkClient.delete_character(password) != OK:
		_show_notice("服务器尚未连接")


func _on_delete_canceled() -> void:
	pass


func _on_server_message(head_code: int, payload: PackedByteArray) -> void:
	match head_code:
		NetworkClient.SM_QUERYCHAROK:
			_apply_character(payload)
		NetworkClient.SM_QUERYCHARERROR:
			_query_complete = true
			has_character = false
			character_sprite.hide()
			$InfoPanel.hide()
			character_info.hide()
			_update_button_visibility()
			_show_notice("请先创建游戏角色")
			_capture_flow_if_requested()
		NetworkClient.SM_DELETECHAROK:
			_query_complete = true
			has_character = false
			character_sprite.hide()
			$InfoPanel.hide()
			character_info.hide()
			_update_button_visibility()
			_show_notice("删除角色成功")
		NetworkClient.SM_DELETECHARERROR:
			var messages := {2: "没有角色可以删除", 3: "密码错误", 4: "删除角色失败，请稍后重试"}
			_show_notice(messages.get(payload[0] if not payload.is_empty() else 0, "删除角色失败"))
		NetworkClient.SM_ONLINEERROR:
			var messages := {2: "请勿频繁登录", 3: "先创建角色再进入游戏"}
			_show_notice(messages.get(payload[0] if not payload.is_empty() else 0, "进入游戏失败"))
		NetworkClient.SM_ONLINEOK:
			# Parse SMOnlineOK and store in GameState before switching scene
			var data := Protocol.decode_sm_online_ok(payload)
			var action: Dictionary = data.get("action", {})
			var game_state := get_node("/root/GameState")
			game_state.set_player_online({
				"uid": data.get("uid", 0),
				"name": data.get("name", ""),
				"gender": data.get("gender", 0),
				"job": data.get("job", 0),
				"map_uid": data.get("mapUID", 0),
				"x": action.get("x", 0),
				"y": action.get("y", 0),
				"direction": action.get("direction", 0),
			})
			get_tree().change_scene_to_file("res://scenes/game/main.tscn")


func _apply_character(payload: PackedByteArray) -> void:
	# SMQueryCharOK: StaticBuffer<64>(68) + gender(1) + job(1) + exp(4) = 74
	if payload.size() < 74:
		_show_notice("角色数据格式错误")
		return
	var name_len := payload.decode_u16(0)
	var name_size := mini(name_len, 64)
	character_name = payload.slice(2, 2 + name_size).get_string_from_utf8()
	character_gender = payload[68]
	character_job = payload[69]
	character_exp = payload.decode_u32(70)
	has_character = true
	_query_complete = true
	notice.clear_messages()
	_update_character_preview()
	_capture_flow_if_requested()


func _update_character_preview() -> void:
	var first_job := _first_job(character_job)
	var frame_count: int = PreviewScript.frame_count(first_job, bool(character_gender), _character_motion)
	if frame_count <= 0:
		character_sprite.hide()
		return
	var frame_id: int = PreviewScript.select_base_id(first_job, bool(character_gender), _character_motion) + _absolute_frame() % frame_count
	character_sprite.show_frame(_resources, Vector2(430, 300), frame_id, Color.WHITE)
	character_sprite.show()
	$InfoPanel.show()
	var jobs := {JOB_WARRIOR: "战士", JOB_WIZARD: "法师", JOB_TAOIST: "道士"}
	character_info.text = "角色：%s\n等级：%d\n职业：%s" % [
		character_name,
		_level_from_exp(character_exp),
		jobs.get(first_job, "未知"),
	]
	character_info.show()
	# C++ uses per-line colors: name (237,226,200), level (175,196,175), profession (231,231,189)
	# Godot Label uses single color, so we use the name color as the closest match
	# The C++ also shows buttons only when has_character
	_update_button_visibility()


func _absolute_frame() -> int:
	return roundi(_animation_time_ms / 200.0)


func _switch_character_motion() -> void:
	var frame_count: int = PreviewScript.frame_count(_first_job(character_job), bool(character_gender), _character_motion)
	var frame := _absolute_frame()
	if frame_count <= 0 or frame % frame_count != 0 or _last_motion_switch_frame == frame:
		return
	_last_motion_switch_frame = frame
	match _character_motion:
		0: _character_motion = 1 if randi_range(0, 1) == 0 else 2
		1: _character_motion = 2
		2: _character_motion = 2 if randi_range(0, 1) == 0 else 3
		_: _character_motion = 0


func _first_job(job: int) -> int:
	if job & JOB_WARRIOR:
		return JOB_WARRIOR
	if job & JOB_TAOIST:
		return JOB_TAOIST
	if job & JOB_WIZARD:
		return JOB_WIZARD
	return 0


func _level_from_exp(experience: int) -> int:
	var level := 0
	while true:
		var sum_exp := 100 * level * level * level + 100 * level * level + 100 * level + 1000
		if sum_exp > experience:
			return level
		level += 1
	return 0


func _show_notice(message: String) -> void:
	notice.show_message(message)


func _update_button_visibility() -> void:
	$StartButton.visible = _query_complete and has_character
	$CreateButton.visible = _query_complete
	$DeleteButton.visible = _query_complete and has_character


func _capture_flow_if_requested() -> void:
	if not OS.has_environment("MIR2X_FLOW_SCREENSHOT"):
		return
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_FLOW_SCREENSHOT"))
	get_tree().quit()
