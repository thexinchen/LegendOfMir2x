extends Node


func _ready() -> void:
	var panel: Control = load("res://scenes/game/panels/npc_chat.tscn").instantiate()
	add_child(panel)
	var dialog := panel.get_node("Dialog") as RichTextLabel
	if dialog.selection_enabled or dialog.scroll_active:
		_fail("NPC dialog enabled text selection or internal scrolling absent from the content-sized C++ board: selection=%s scroll=%s" % [dialog.selection_enabled, dialog.scroll_active])
		return
	if not dialog.get_theme_font("normal_font").resource_path.ends_with("/0B_WenQuanYi_Bitmap_Song_15_px.ttf") or dialog.get_theme_font_size("normal_font_size") != 15:
		_fail("NPC dialog did not use original font-11/15px default: font=%s size=%d" % [dialog.get_theme_font("normal_font").resource_path, dialog.get_theme_font_size("normal_font_size")])
		return
	var xml := "<layout><par>你好<t color=\"red\">勇士</t></par><par><event id=\"buy\" args=\"{'id':1}\" close=\"1\">购买</event></par></layout>"
	var bbcode: String = panel.call("_build_bbcode", xml)
	if not bbcode.contains("你好") or not bbcode.contains("[color=red]勇士[/color]"):
		_fail("styled text missing: %s" % bbcode)
		return
	if not bbcode.contains("[url=") or not bbcode.contains("购买[/url]") or not bbcode.contains("\"close\":true"):
		_fail("click event missing: %s" % bbcode)
		return
	var background_xml := "<layout><par bgcolor=\"rgb(0x00, 0x80, 0x00)\">段落<t bgcolor=\"#0000ff\">内联</t></par></layout>"
	var background_bbcode: String = panel.call("_build_bbcode", background_xml)
	if not background_bbcode.contains("[bgcolor=#008000]段落[bgcolor=#0000ff]内联[/bgcolor][/bgcolor]"):
		_fail("NPC text background color missing: %s" % background_bbcode)
		return
	if dialog.get_theme_constant("text_highlight_h_padding") != 0 or dialog.get_theme_constant("text_highlight_v_padding") != 0:
		_fail("NPC text background padding exceeded the original token box: h=%d v=%d" % [dialog.get_theme_constant("text_highlight_h_padding"), dialog.get_theme_constant("text_highlight_v_padding")])
		return
	var no_args_bbcode: String = panel.call("_build_bbcode", "<layout><par><event id=\"hello\">问候</event></par></layout>")
	if not no_args_bbcode.contains("\"args\":null"):
		_fail("missing NPC event args did not preserve the original null value: %s" % no_args_bbcode)
		return
	var null_payload: PackedByteArray = NetworkClient._make_npc_event_payload(1, "npc/test", "hello")
	var empty_payload: PackedByteArray = NetworkClient._make_npc_event_payload(1, "npc/test", "hello", "")
	var value_payload: PackedByteArray = NetworkClient._make_npc_event_payload(1, "npc/test", "hello", "参数")
	if null_payload.decode_u16(408) != 0xFFFF or empty_payload.decode_u16(408) != 0 or value_payload.decode_u16(408) != 6 or value_payload.slice(208, 214).get_string_from_utf8() != "参数":
		_fail("NPC event optional value encoding mismatch: null=%d empty=%d value=%d" % [null_payload.decode_u16(408), empty_payload.decode_u16(408), value_payload.decode_u16(408)])
		return
	var event_meta := JSON.stringify({"id": "buy", "path": "", "args": "{'id':1}", "close": true})
	if not (panel.call("_build_bbcode", xml, event_meta) as String).contains("[color=#00ff00]"):
		_fail("hover event color missing")
		return
	if not (panel.call("_build_bbcode", xml, event_meta, event_meta) as String).contains("[color=#ff00ff]"):
		_fail("pressed event color missing")
		return
	var emoji_bbcode: String = panel.call("_build_bbcode", "<layout><par>A<emoji id=\"0\"/>B</par></layout>")
	if emoji_bbcode.contains("☺") or not emoji_bbcode.contains("[[MIR2X_EMOJI:0]]"):
		_fail("emoji id still degraded to a generic glyph: %s" % emoji_bbcode)
		return
	var emoji_definition: Dictionary = panel.call("_emoji_definition", 0)
	if emoji_definition.get("frame_count", 0) != 8 or emoji_definition.get("fps", 0) != 5 or emoji_definition.get("width", 0) != 24 or emoji_definition.get("height", 0) != 22 or emoji_definition.get("h1", 0) != 22:
		_fail("emoji 0 atlas metadata mismatch: %s" % emoji_definition)
		return
	panel.call("_render_dialog", "<layout><par>A<emoji id=\"0\"/>B</par></layout>")
	var emoji_frames: Array = panel.get("_emoji_frames")
	if emoji_frames.size() != 1 or (emoji_frames[0].atlas as AtlasTexture).region.size != Vector2(24, 22):
		_fail("emoji atlas was not inserted inline: %s" % emoji_frames)
		return
	emoji_frames[0].start_ms = Time.get_ticks_msec() - 250
	panel.call("_process", 0.0)
	if (emoji_frames[0].atlas as AtlasTexture).region.position != Vector2(24, 0):
		_fail("emoji atlas did not advance at original 5 FPS: %s" % (emoji_frames[0].atlas as AtlasTexture).region)
		return
	var cjk_text := "\u7532\u4e59\u4e19\u4e01"
	var nowrap_xml := "<layout><par>12345678<event id=\"spawn\" wrap=\"false\">%s</event></par></layout>" % cjk_text
	var nowrap_bbcode: String = panel.call("_build_bbcode", nowrap_xml, "", "", 100.0)
	if not nowrap_bbcode.contains("12345678\n[color=#ffff00][url=") or not nowrap_bbcode.contains("%s[/url]" % cjk_text):
		_fail("wrap=false event was not moved to an intact next line: %s" % nowrap_bbcode)
		return
	var display_xml := xml
	if OS.has_environment("MIR2X_NPC_BGCOLOR_SCREENSHOT"):
		display_xml = "<layout><par bgcolor=\"rgb(0x00, 0x80, 0x00)\">原版段落背景色</par><par color=\"yellow\">黄色段落<t bgcolor=\"#0000ff\">蓝底继承黄字</t></par><par><event id=\"close\" close=\"1\">关闭</event></par></layout>"
	elif OS.has_environment("MIR2X_NPC_NOWRAP_SCREENSHOT"):
		display_xml = "<layout><par>Monster list:</par><par><event id=\"a\" wrap=\"false\">\u7532\u4e59\u4e19\u4e01\uff0c</event><event id=\"b\" wrap=\"false\">\u620a\u5df1\u5e9a\u8f9b\uff0c</event><event id=\"c\" wrap=\"false\">\u58ec\u7678\u5b50\u4e11\uff0c</event><event id=\"d\" wrap=\"false\">\u5bc5\u536f\u8fb0\u5df3\uff0c</event></par></layout>"
	elif OS.has_environment("MIR2X_NPC_EMOJI_SCREENSHOT"):
		display_xml = "<layout><par>Original emoji:</par><par><emoji id=\"0\"/> <emoji id=\"1\"/> <emoji id=\"2\"/> <emoji id=\"3\"/> <emoji id=\"4\"/> <emoji id=\"5\"/> <emoji id=\"6\"/> <emoji id=\"7\"/> <emoji id=\"8\"/> <emoji id=\"9\"/></par></layout>"
	GameState.npc_dialog = {"npcUID": 1, "eventPath": "npc/test", "xmlLayout": display_xml}
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
