extends Node


func _ready() -> void:
	var panel: Control = load("res://scenes/game/panels/npc_chat.tscn").instantiate()
	add_child(panel)
	var xml := "<layout><par>你好<t color=\"red\">勇士</t></par><par><event id=\"buy\" args=\"{'id':1}\" close=\"1\">购买</event></par></layout>"
	var bbcode: String = panel.call("_build_bbcode", xml)
	if not bbcode.contains("你好") or not bbcode.contains("[color=red]勇士[/color]"):
		_fail("styled text missing: %s" % bbcode)
		return
	if not bbcode.contains("[url=") or not bbcode.contains("购买[/url]") or not bbcode.contains("\"close\":true"):
		_fail("click event missing: %s" % bbcode)
		return
	var event_meta := JSON.stringify({"id": "buy", "path": "", "args": "{'id':1}", "close": true})
	if not (panel.call("_build_bbcode", xml, event_meta) as String).contains("[color=#00ff00]"):
		_fail("hover event color missing")
		return
	if not (panel.call("_build_bbcode", xml, event_meta, event_meta) as String).contains("[color=#ff00ff]"):
		_fail("pressed event color missing")
		return
	GameState.npc_dialog = {"npcUID": 1, "eventPath": "npc/test", "xmlLayout": xml}
	GameState.state_changed.emit()
	await get_tree().process_frame
	await get_tree().process_frame
	if panel.size == Vector2(386, 204) or panel.size.x > 300.0:
		_fail("dialog did not use content-sized C++ layout: %s" % panel.size)
		return
	if panel.get_node("Upper").size != Vector2(panel.size.x, panel.size.y - 44.0):
		_fail("upper nine-patch geometry mismatch")
		return
	if panel.get_node("Lower").position != Vector2(0.0, panel.size.y - 44.0):
		_fail("lower nine-patch geometry mismatch")
		return
	var close := panel.get_node("CloseButton") as TextureButton
	if close.texture_normal != null or close.texture_hover == null or close.texture_pressed == null:
		_fail("close overlay texture states mismatch")
		return
	if not is_zero_approx(close.modulate.a):
		_fail("close overlay should be transparent while idle")
		return
	close.mouse_entered.emit()
	if not is_equal_approx(close.modulate.a, 1.0):
		_fail("close overlay did not appear while hovered")
		return
	close.mouse_exited.emit()
	if close.position != Vector2(panel.size.x - 40.0, panel.size.y - 43.0):
		_fail("close overlay position mismatch")
		return
	var face_image := Image.create_empty(48, 64, false, Image.FORMAT_RGBA8)
	panel.get_node("Face").texture = ImageTexture.create_from_image(face_image)
	panel.get_node("Face").visible = true
	panel.call("_apply_layout")
	if panel.get_node("Dialog").position.x != 118.0 or panel.size.y < 134.0:
		_fail("NPC face margin/layout mismatch: dialog=%s panel=%s" % [panel.get_node("Dialog").position, panel.size])
		return
	panel.call("_refresh")
	await get_tree().process_frame
	var player_state: Control = load("res://scenes/game/panels/player_state.tscn").instantiate()
	add_child(player_state)
	if (player_state.get_node("CloseButton") as TextureButton).texture_pressed == null:
		_fail("player-state close pressed texture missing")
		return
	player_state.queue_free()
	if OS.has_environment("MIR2X_NPC_DIALOG_SCREENSHOT"):
		panel.show()
		Input.warp_mouse(Vector2(700, 500))
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_NPC_DIALOG_SCREENSHOT"))
		print("NPC DIALOG PASS: text, color and click metadata")
		get_tree().quit()
		return
	panel.show()
	panel.call("_on_meta_clicked", JSON.stringify({"id": "buy", "path": "", "args": "{'id':1}", "close": true}))
	if panel.visible:
		_fail("close event did not hide the panel")
		return
	print("NPC DIALOG PASS: text, color and click metadata")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("NPC_DIALOG_SMOKE %s" % message)
	get_tree().quit(1)
