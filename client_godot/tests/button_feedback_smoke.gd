extends Node

const PANEL_SPECS := [
	{"scene": "res://scenes/game/panels/guild.tscn", "click": ["AnnouncementButton", "MembersButton", "ChatButton", "EditButton", "RemoveButton", "DisbandButton", "PositionButton", "CovenantButton", "CloseButton"], "overlay": ["AnnouncementButton", "MembersButton", "ChatButton", "EditButton", "RemoveButton", "DisbandButton", "PositionButton", "CovenantButton", "CloseButton"]},
	{"scene": "res://scenes/game/panels/horse.tscn", "click": ["UpButton", "DownButton", "HideButton", "ShowButton", "CloseButton"], "overlay": ["UpButton", "DownButton", "HideButton", "ShowButton", "CloseButton"]},
	{"scene": "res://scenes/game/panels/input_string.tscn", "click": ["ConfirmButton", "CancelButton"], "pressed": ["ConfirmButton", "CancelButton"]},
	{"scene": "res://scenes/game/panels/minimap.tscn", "click": ["AlphaButton", "ExtendButton", "CenterButton", "ConfigButton"], "normal": ["AlphaButton", "ExtendButton", "CenterButton", "ConfigButton"]},
	{"scene": "res://scenes/game/panels/purchase.tscn", "click": ["SelectButton", "CloseButton"], "pressed": ["SelectButton", "CloseButton"], "dynamic_purchase": true},
	{"scene": "res://scenes/game/panels/quest.tscn", "click": ["FoldButton", "CloseButton"], "overlay": ["CloseButton"], "pressed": ["FoldButton"]},
	{"scene": "res://scenes/game/panels/secured_items.tscn", "click": ["LeftButton", "SelectButton", "RightButton", "CloseButton"], "overlay": ["LeftButton", "SelectButton", "RightButton", "CloseButton"]},
	{"scene": "res://scenes/game/panels/skill.tscn", "click": ["Tab0", "Tab1", "Tab2", "Tab3", "Tab4", "Tab5", "Tab6", "Tab7", "CloseButton"], "pressed": ["Tab0", "Tab1", "Tab2", "Tab3", "Tab4", "Tab5", "Tab6", "Tab7", "CloseButton"]},
	{"scene": "res://scenes/game/panels/team.tscn", "click": ["EnableButton", "SwitchButton", "AddButton", "DeleteButton", "RefreshButton", "CloseButton"], "overlay": ["EnableButton", "SwitchButton", "RefreshButton", "CloseButton"], "pressed": ["AddButton", "DeleteButton"]},
	{"scene": "res://scenes/game/panels/runtime_config.tscn", "click": ["CloseButton"], "overlay": ["CloseButton"]},
	{"scene": "res://scenes/game/panels/friend_chat.tscn", "click": ["Toolbar/Back", "Toolbar/Search", "Toolbar/CreateGroup", "Toolbar/GroupConfirm", "Toolbar/Invert", "Toolbar/Friends", "CloseButton"], "pressed": ["Toolbar/Back", "Toolbar/Search", "Toolbar/CreateGroup", "Toolbar/GroupConfirm", "Toolbar/Invert", "Toolbar/Friends", "CloseButton"]},
]

const ACCOUNT_SPECS := [
	{"scene": "res://scenes/account/login.tscn", "click": ["CreateAccountButton", "ChangePasswordButton", "ExitButton", "LoginButton"], "pressed": ["CreateAccountButton", "ChangePasswordButton", "ExitButton", "LoginButton"]},
	{"scene": "res://scenes/account/create_account.tscn", "click": ["SubmitButton", "ReturnButton"], "overlay": ["SubmitButton", "ReturnButton"]},
	{"scene": "res://scenes/account/change_password.tscn", "click": ["SubmitButton", "ReturnButton"], "overlay": ["SubmitButton", "ReturnButton"]},
	{"scene": "res://scenes/account/select_character.tscn", "click": ["StartButton", "CreateButton", "DeleteButton", "ExitButton", "DeleteCharacterDialog/YesButton", "DeleteCharacterDialog/NoButton"], "pressed": ["StartButton", "CreateButton", "DeleteButton", "ExitButton", "DeleteCharacterDialog/YesButton", "DeleteCharacterDialog/NoButton"]},
	{"scene": "res://scenes/account/delete_character_dialog.tscn", "click": ["YesButton", "NoButton"], "pressed": ["YesButton", "NoButton"]},
	{"scene": "res://scenes/account/create_character.tscn", "click": ["WarriorButton", "WizardButton", "TaoistButton", "SubmitButton", "ExitButton"], "pressed": ["WarriorButton", "WizardButton", "TaoistButton", "SubmitButton", "ExitButton"]},
]


func _ready() -> void:
	AudioService.set_seff_enabled(true)
	for spec in PANEL_SPECS + ACCOUNT_SPECS:
		if not await _verify_scene(spec):
			return
	AudioService.stop_bgm()
	AudioService.stop_seff()
	print("BUTTON FEEDBACK PASS: exact C++ click scope, overlay visibility and pressed textures across HUD panels and account dialogs")
	get_tree().quit()


func _verify_scene(spec: Dictionary) -> bool:
	var root := (load(spec.scene) as PackedScene).instantiate() as Control
	add_child(root)
	await get_tree().process_frame
	for path in spec.get("click", []):
		var button := root.get_node(path) as BaseButton
		if _click_connection_count(button) != 1:
			return _fail("%s:%s did not bind exactly one original click sound" % [spec.scene, path])
	for path in spec.get("overlay", []):
		var button := root.get_node(path) as TextureButton
		if button.texture_normal != null or button.texture_hover == null or button.texture_pressed == null or button.modulate.a != 0.0:
			return _fail("%s:%s did not preserve original idle/hover/down overlay states" % [spec.scene, path])
		button.mouse_entered.emit()
		if button.modulate.a != 1.0:
			return _fail("%s:%s did not become visible on hover" % [spec.scene, path])
		button.mouse_exited.emit()
	for path in spec.get("normal", []):
		if (root.get_node(path) as TextureButton).texture_normal == null:
			return _fail("%s:%s lost its original normal texture" % [spec.scene, path])
	for path in spec.get("pressed", []):
		var button := root.get_node(path) as TextureButton
		if button.texture_normal == null or button.texture_pressed == null:
			return _fail("%s:%s lost its original normal/down state" % [spec.scene, path])
	if spec.get("dynamic_purchase", false):
		root.call("_add_detail_button", Vector2(10, 10), root.get_node("SelectButton").texture_normal, root.get_node("SelectButton").texture_pressed, func(): pass)
		root.call("_add_detail_close", Vector2(50, 10))
		for index in range(root.get_node("Detail").get_child_count() - 2, root.get_node("Detail").get_child_count()):
			var button := root.get_node("Detail").get_child(index) as TextureButton
			if button == null or button.texture_normal == null or button.texture_pressed == null or _click_connection_count(button) != 1:
				return _fail("dynamic purchase control did not preserve normal/down/click behavior")
	if spec.scene == "res://scenes/game/panels/horse.tscn":
		AudioService.last_seff_id = AudioService.INVALID_SEFF_ID
		root.get_node("UpButton").pressed.emit()
		if AudioService.last_seff_id != AudioService.UI_CLICK_SEFF_ID:
			return _fail("bound panel button did not play SEFF 0x01020069")
	root.queue_free()
	await get_tree().process_frame
	return true


func _click_connection_count(button: BaseButton) -> int:
	var callback := Callable(AudioService, "play_ui_click")
	var count := 0
	for connection in button.pressed.get_connections():
		if connection.callable == callback:
			count += 1
	return count


func _fail(message: String) -> bool:
	push_error("BUTTON_FEEDBACK_SMOKE %s" % message)
	get_tree().quit(1)
	return false
