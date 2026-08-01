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
	GameState.npc_dialog = {"npcUID": 1, "eventPath": "npc/test", "xmlLayout": xml}
	GameState.state_changed.emit()
	if OS.has_environment("MIR2X_NPC_DIALOG_SCREENSHOT"):
		panel.show()
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
