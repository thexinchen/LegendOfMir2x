extends Node


func _ready() -> void:
	var panel := load("res://scenes/game/panels/input_string.tscn").instantiate() as Control
	add_child(panel)
	await get_tree().process_frame
	var commits: Array[String] = []
	var cancel_count := [0]
	panel.committed.connect(func(value: String): commits.append(value))
	panel.cancelled.connect(func(): cancel_count[0] += 1)
	panel.configure("<layout><par>请输入密码<br/>用于验证</par></layout>", true)
	if panel.get_node("Title").text != "请输入密码\n用于验证" or not panel.get_node("Value").secret:
		_fail("XML title/security mode mismatch: title=%s secret=%s" % [panel.get_node("Title").text, panel.get_node("Value").secret])
		return
	panel.get_node("Value").text = "  \t密码  "
	panel.call("_confirm")
	if commits != ["密码  "] or panel.visible or not panel.get_node("Value").text.is_empty():
		_fail("leading trim/confirm cleanup mismatch: %s" % commits)
		return
	panel.configure("购买数量", false)
	if panel.get_node("Value").secret:
		_fail("non-security input was hidden")
		return
	panel.get_node("Value").text = "中".repeat(100)
	panel.call("_on_text_changed", panel.get_node("Value").text)
	if panel.get_node("Value").text.to_utf8_buffer().size() > 255:
		_fail("UTF-8 input exceeded the C++ 255-byte buffer")
		return
	panel.call("_cancel")
	if cancel_count[0] != 1 or panel.visible or not panel.get_node("Value").text.is_empty():
		_fail("Cancel did not clear and signal")
		return
	panel.configure("再次输入", true)
	panel.get_node("Value").text = "secret"
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	panel.call("_unhandled_key_input", escape)
	if cancel_count[0] != 2 or panel.visible or not panel.get_node("Value").text.is_empty():
		_fail("Escape did not use cancellation cleanup")
		return
	if OS.has_environment("MIR2X_INPUT_SCREENSHOT"):
		panel.configure("<layout><par>请输入密码</par></layout>", true)
		panel.get_node("Value").text = "mir2x-password"
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_INPUT_SCREENSHOT"))
	print("INPUT STRING PASS: XML title, security, UTF-8 limit, trim, Cancel and Escape")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("INPUT_STRING_SMOKE %s" % message)
	get_tree().quit(1)
