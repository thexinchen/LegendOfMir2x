extends Node

const AutoLogin = preload("res://scripts/account/auto_login.gd")


func _ready() -> void:
	if AutoLogin._validated("test:123456") != PackedStringArray(["test", "123456"]) or not AutoLogin._validated(":123456").is_empty() or not AutoLogin._validated("test:").is_empty() or not AutoLogin._validated("test").is_empty():
		_fail("auto-login credential validation diverged from C++")
		return
	var logo := Control.new()
	logo.set_script(load("res://scripts/startup/logo.gd"))
	var expect_auto_login := OS.has_environment("MIR2X_EXPECT_AUTO_LOGIN")
	var detected_auto_login := bool(logo.call("_has_auto_login"))
	if detected_auto_login != expect_auto_login:
		_fail("ProcessLogo auto-login detection mismatch: expected=%s actual=%s" % [expect_auto_login, detected_auto_login])
		return
	logo.free()
	var sync := load("res://scenes/startup/sync.tscn").instantiate() as Control
	add_child(sync)
	await get_tree().process_frame
	sync.set_process(false)
	var progress := sync.get_node("ProgressBar") as Control
	var progress_clip := sync.get_node("ProgressBar/Clip") as Control
	var backdrop := sync.get_node("Backdrop") as ColorRect
	var status := sync.get_node("Status") as Label
	if backdrop.color != Color.BLACK or progress.position != Vector2(112, 528) or progress.size != Vector2(570, 18) or status.text != "Connecting..." or status.get_theme_font_size("font_size") != 10:
		_fail("sync draw geometry/text mismatch: progress=%s/%s text=%s size=%d" % [progress.position, progress.size, status.text, status.get_theme_font_size("font_size")])
		return
	sync.set("ratio", 0)
	sync.call("_process", 0.0)
	if sync.get("ratio") != 0:
		_fail("zero-delta update advanced the original artificial progress")
		return
	sync.call("_process", 0.01)
	sync.call("_process", 0.01)
	if sync.get("ratio") != 2 or not is_equal_approx(progress_clip.size.x, 11.4):
		_fail("one-point-per-update progress mismatch: ratio=%d width=%s" % [sync.get("ratio"), progress_clip.size.x])
		return
	if OS.has_environment("MIR2X_STARTUP_SYNC_SCREENSHOT"):
		sync.set("ratio", 50)
		sync.call("_update_progress")
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_STARTUP_SYNC_SCREENSHOT"))
	var space := InputEventKey.new()
	space.pressed = true
	space.keycode = KEY_SPACE
	var current_scene := get_tree().current_scene
	sync.call("_unhandled_input", space)
	if get_tree().current_scene != current_scene or sync.get("_login_requested"):
		_fail("Space incorrectly skipped ProcessSync")
		return
	var escape := InputEventKey.new()
	escape.pressed = true
	escape.keycode = KEY_ESCAPE
	sync.call("_unhandled_input", escape)
	if not sync.get("_login_requested"):
		_fail("Escape did not request the Login scene")
		return
	sync.set("_login_requested", false)
	sync.queue_free()
	await get_tree().process_frame
	print("STARTUP SYNC PASS: original geometry, update timing and Escape-only skip")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("STARTUP_SYNC_SMOKE %s" % message)
	get_tree().quit(1)
