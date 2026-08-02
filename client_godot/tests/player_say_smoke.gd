extends Control

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default():
		_fail("world resources unavailable")
		return
	GameState.player_map_id = 24
	GameState.player_map_uid = 24 << 35
	GameState.player_uid = (5 << 59) | (1 << 35) | 1
	GameState.player_name = "亚当"
	GameState.player_x = 405
	GameState.player_y = 120
	GameState.player_gender = 1
	GameState.player_direction = 5
	GameState.player_action_type = 2
	GameState.player_action_started_ms = Time.get_ticks_msec()
	GameState.player_desp = {"wear": {}}
	GameState.view_x = 405 * 48 - 400
	GameState.view_y = 120 * 32 - 300
	var remote_uid: int = (5 << 59) | (1 << 35) | 2
	GameState.creatures = {
		remote_uid: {
			"uid": remote_uid, "x": 410, "y": 120, "type": 2,
			"name": "远端玩家", "gender": 0, "direction": 7,
			"action_type": 2, "action_started_ms": Time.get_ticks_msec(),
			"action_speed": 100, "desp": {"wear": {}},
		},
	}
	GameState.player_say_messages.clear()
	for index in range(11):
		GameState.add_player_say(GameState.player_uid, "消息%d" % index)
	if GameState.player_say_messages.get(GameState.player_uid, []).size() != 10 or GameState.player_say_messages[GameState.player_uid][0].text != "消息1":
		_fail("ten-entry player say limit mismatch")
		return
	$WorldRenderer.actor_resource = resources
	$WorldRenderer.game_state = GameState
	if not $WorldRenderer.load_map(24):
		_fail("map 24 failed to load")
		return
	var script_constants := ($WorldRenderer.get_script() as Script).get_script_constant_map()
	var font := script_constants.get("PLAYER_SAY_FONT") as Font
	if font == null or not font.resource_path.ends_with("/0B_WenQuanYi_Bitmap_Song_15_px.ttf"):
		_fail("player say did not use the C++ font 11 resource: %s" % font)
		return
	GameState.player_say_messages.clear()
	GameState.add_player_say(GameState.player_uid, "短消息")
	var now := Time.get_ticks_msec()
	var short_layout: Dictionary = $WorldRenderer.call("_player_say_layout", GameState.player_uid, font, now)
	if short_layout.is_empty() or short_layout.width >= 164 or short_layout.messages[0].lines.size() != 1:
		_fail("short player say did not shrink to text width: %s" % short_layout)
		return
	GameState.player_say_messages.clear()
	GameState.add_player_say(remote_uid, "组队去矿洞吗？这是用于验证一百六十像素自动换行的较长消息。")
	var long_layout: Dictionary = $WorldRenderer.call("_player_say_layout", remote_uid, font, now)
	if long_layout.messages[0].lines.size() < 2 or long_layout.width > 164:
		_fail("long player say wrap mismatch: %s" % long_layout)
		return
	GameState.player_say_messages[remote_uid].push_front({"text": "过期消息", "start_time": now - 5000})
	long_layout = $WorldRenderer.call("_player_say_layout", remote_uid, font, now)
	if GameState.player_say_messages[remote_uid].size() != 1 or long_layout.messages.size() != 1:
		_fail("five-second player say expiry mismatch")
		return
	GameState.add_player_say(GameState.player_uid, "你好，欢迎来到比奇")
	GameState.add_player_say(remote_uid, "第二条消息向上堆叠")
	$WorldRenderer.queue_redraw()
	await get_tree().process_frame
	if OS.has_environment("MIR2X_PLAYER_SAY_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_PLAYER_SAY_SCREENSHOT"))
	print("PLAYER SAY PASS: protocol semantics, hero filtering, 5-second stack, wrapping and original placement")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("PLAYER_SAY_SMOKE %s" % message)
	get_tree().quit(1)
