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
	var expected_font_paths := [
		"res://assets/font/00_SIMSUN.ttf",
		"res://assets/font/01_Yahei.ttf",
		"res://assets/font/02_CALIBRI.ttf",
		"res://assets/font/03_MONOWIDE.ttf",
		"res://assets/font/04_YaHei_Consolas_Hybrid.ttf",
		"res://assets/font/05_NSIMSUN.ttf",
		"res://assets/font/06_YaHei_Monaco_Hybrid.ttf",
		"res://assets/font/07_fusion-pixel-12px-monospaced-zh_hans.ttf",
		"res://assets/font/08_fusion-pixel-12px-proportional-zh_hans.ttf",
		"res://assets/font/09_WenQuanYi_Bitmap_Song_15_px.ttf",
		"res://assets/font/0A_WenQuanYi_Bitmap_Song_15_px.ttf",
		"res://assets/font/0B_WenQuanYi_Bitmap_Song_15_px.ttf",
		"res://assets/font/0C_WenQuanYi_Bitmap_Song_18_px.ttf",
		"res://assets/font/0D_WenQuanYi_Bitmap_Song_18_px.ttf",
	]
	if not panel.has_method("_paragraph_font_path"):
		_fail("NPC paragraph font mapping is missing")
		return
	for font_id in expected_font_paths.size():
		var font_path: Variant = panel.call("_paragraph_font_path", str(font_id))
		if font_path != expected_font_paths[font_id] or not ResourceLoader.exists(str(font_path)):
			_fail("NPC font ID %d unavailable: path=%s expected=%s" % [font_id, font_path, expected_font_paths[font_id]])
			return
	if panel.call("_paragraph_font_path", "MONOWIDE") != expected_font_paths[3] \
			or panel.call("_paragraph_font_path", "WenQuanYi_Bitmap_Song_15_px") != expected_font_paths[9] \
			or panel.call("_paragraph_font_path", "WenQuanYi_Bitmap_Song_18_px") != expected_font_paths[12] \
			or panel.call("_paragraph_font_path", "unifont_17_0_04") != expected_font_paths[11] \
			or panel.call("_paragraph_font_path", "missing-font") != expected_font_paths[0]:
		_fail("NPC named font mapping did not match the original font database order")
		return
	var font_xml := "<layout><par font=\"MONOWIDE\">段落<t font=\"1\">内联</t><t font=\"255\">继承</t><event id=\"font-event\" font=\"0\">事件</event></par></layout>"
	var font_bbcode: String = panel.call("_build_bbcode", font_xml)
	if not font_bbcode.contains("[font=%s]段落[font=%s]内联[/font]继承[font=%s][color=#ffff00][url=" % [expected_font_paths[3], expected_font_paths[1], expected_font_paths[0]]) \
			or not font_bbcode.contains("事件[/url][/color][/font][/font]"):
		_fail("NPC paragraph/inline font tags or fallback missing: %s" % font_bbcode)
		return
	var layout_bbcode: String = panel.call("_build_bbcode", "<layout><par wordSpace=\"3\">甲乙丙丁</par><par align=\"distributed\">末行</par></layout>")
	if not layout_bbcode.begins_with("[fill][font glyph_spacing=3]") or layout_bbcode.contains("[fill]末行"):
		_fail("NPC justify/distributed or wordSpace mapping mismatch: %s" % layout_bbcode)
		return
	var inline_size_bbcode: String = panel.call("_build_bbcode", "<layout><par><t size=\"18\">大字</t><event id=\"small\" size=\"12\">小字</event></par></layout>")
	if not inline_size_bbcode.contains("[font_size=18]大字[/font_size]") or not inline_size_bbcode.contains("[font_size=12][color=#ffff00][url=") or not inline_size_bbcode.contains("小字[/url][/color][/font_size]"):
		_fail("NPC inline text/event size mapping mismatch: %s" % inline_size_bbcode)
		return
	var mono_font := load(expected_font_paths[3]) as Font
	var default_font := dialog.get_theme_font("normal_font")
	var prefix := "iiii"
	var atomic := "WWWW"
	var prefix_width := default_font.get_string_size(prefix, HORIZONTAL_ALIGNMENT_LEFT, -1, 15).x
	var mono_atomic_width := mono_font.get_string_size(atomic, HORIZONTAL_ALIGNMENT_LEFT, -1, 15).x
	var default_atomic_width := default_font.get_string_size(atomic, HORIZONTAL_ALIGNMENT_LEFT, -1, 15).x
	if is_equal_approx(mono_atomic_width, default_atomic_width):
		_fail("NPC font-aware wrap fixture has no measurable font difference")
		return
	var font_wrap_width := prefix_width + (mono_atomic_width + default_atomic_width) * 0.5
	var font_wrap_bbcode: String = panel.call("_build_bbcode", "<layout><par>%s<event id=\"font-wrap\" font=\"3\" wrap=\"false\">%s</event></par></layout>" % [prefix, atomic], "", "", font_wrap_width)
	var should_wrap := prefix_width + mono_atomic_width > font_wrap_width
	if font_wrap_bbcode.contains("%s\n[font=" % prefix) != should_wrap:
		_fail("NPC wrap=false did not measure the event font: mono=%s default=%s width=%s bbcode=%s" % [mono_atomic_width, default_atomic_width, font_wrap_width, font_wrap_bbcode])
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
	var paragraph_layout_xml := "<layout><par lineWidth=\"80\" lineSpace=\"5\">甲乙丙丁戊己庚辛壬癸子丑</par><par>默认宽度</par><par lineWidth=\"0\">无限单行宽度</par></layout>"
	panel.call("_render_dialog", paragraph_layout_xml)
	if dialog.get_child_count() != 3:
		_fail("NPC paragraphs were not rendered by independent width controls: children=%d" % dialog.get_child_count())
		return
	var narrow_paragraph := dialog.get_child(0) as RichTextLabel
	var default_paragraph := dialog.get_child(1) as RichTextLabel
	var infinite_paragraph := dialog.get_child(2) as RichTextLabel
	if narrow_paragraph == null or default_paragraph == null or infinite_paragraph == null:
		_fail("NPC paragraph controls have the wrong type")
		return
	if narrow_paragraph.get_theme_color("default_color") != Color.WHITE:
		_fail("NPC paragraph default text color is not original white: %s" % narrow_paragraph.get_theme_color("default_color"))
		return
	if not is_equal_approx(narrow_paragraph.size.x, 80.0) or narrow_paragraph.get_theme_constant("line_separation") != 5 or narrow_paragraph.autowrap_mode != TextServer.AUTOWRAP_ARBITRARY or narrow_paragraph.get_line_count() < 2:
		_fail("NPC explicit paragraph width/lineSpace mismatch: size=%s lineSpace=%d wrap=%d" % [narrow_paragraph.size, narrow_paragraph.get_theme_constant("line_separation"), narrow_paragraph.autowrap_mode])
		return
	if not is_equal_approx(default_paragraph.size.x, panel.call("_dialog_line_width")):
		_fail("NPC paragraph did not inherit the board line width: size=%s board=%s" % [default_paragraph.size.x, panel.call("_dialog_line_width")])
		return
	if infinite_paragraph.autowrap_mode != TextServer.AUTOWRAP_OFF or infinite_paragraph.get_line_count() != 1 or infinite_paragraph.size.x < infinite_paragraph.get_content_width():
		_fail("NPC lineWidth=0 did not preserve an infinite single line: size=%s content=%s lines=%d" % [infinite_paragraph.size, infinite_paragraph.get_content_width(), infinite_paragraph.get_line_count()])
		return
	if default_paragraph.position.y != narrow_paragraph.size.y or infinite_paragraph.position.y != default_paragraph.position.y + default_paragraph.size.y:
		_fail("NPC paragraph Y stacking mismatch: %s %s %s" % [narrow_paragraph.get_rect(), default_paragraph.get_rect(), infinite_paragraph.get_rect()])
		return
	panel.call("_render_dialog", "<layout><par lineWidth=\"220\" align=\"right\">局部右对齐</par></layout>")
	var right_aligned_size: Vector2 = panel.get("_dialog_content_size")
	if not is_equal_approx(right_aligned_size.x, 220.0):
		_fail("NPC right-aligned paragraph escaped the panel bounds: content=%s" % right_aligned_size)
		return
	var display_xml := xml
	if OS.has_environment("MIR2X_NPC_FONT_SCREENSHOT"):
		display_xml = "<layout><par font=\"MONOWIDE\">MONOWIDE 123</par><par font=\"SIMSUN\">宋体位置合理</par><par font=\"7\" size=\"12\">12px像素字体</par><par font=\"12\" size=\"18\">18px文泉驿字体</par><par font=\"3\">父字体<t font=\"1\" color=\"yellow\">雅黑内联</t>恢复父字体</par><par><event id=\"close\" font=\"0\" close=\"1\">关闭</event></par></layout>"
	elif OS.has_environment("MIR2X_NPC_BGCOLOR_SCREENSHOT"):
		display_xml = "<layout><par bgcolor=\"rgb(0x00, 0x80, 0x00)\">原版段落背景色</par><par color=\"yellow\">黄色段落<t bgcolor=\"#0000ff\">蓝底继承黄字</t></par><par><event id=\"close\" close=\"1\">关闭</event></par></layout>"
	elif OS.has_environment("MIR2X_NPC_NOWRAP_SCREENSHOT"):
		display_xml = "<layout><par>Monster list:</par><par><event id=\"a\" wrap=\"false\">\u7532\u4e59\u4e19\u4e01\uff0c</event><event id=\"b\" wrap=\"false\">\u620a\u5df1\u5e9a\u8f9b\uff0c</event><event id=\"c\" wrap=\"false\">\u58ec\u7678\u5b50\u4e11\uff0c</event><event id=\"d\" wrap=\"false\">\u5bc5\u536f\u8fb0\u5df3\uff0c</event></par></layout>"
	elif OS.has_environment("MIR2X_NPC_LAYOUT_SCREENSHOT"):
		display_xml = "<layout><par lineWidth=\"150\" lineSpace=\"5\" wordSpace=\"3\">150px justify：甲乙丙丁戊己庚辛壬癸子丑</par><par lineWidth=\"220\" align=\"right\">220px 局部右对齐</par><par align=\"distributed\">distributed 靠左</par><par lineWidth=\"0\"><t size=\"18\" font=\"3\">无限宽 18px</t> <event id=\"small\" size=\"12\">12px事件</event></par></layout>"
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
