extends Node


func _ready() -> void:
	var select := load("res://scenes/account/select_character.tscn").instantiate() as Control
	add_child(select)
	await get_tree().process_frame
	if select.get("_query_complete") or select.get("has_character") or select.get_node("CharacterSprite").visible or select.get_node("InfoPanel").visible or select.get_node("InfoPanel/CharacterInfo").visible:
		_fail("selection screen exposed a fake character before query completion")
		return
	if select.get_node("StartButton").visible or select.get_node("CreateButton").visible or select.get_node("DeleteButton").visible:
		_fail("selection actions were available while the character query was pending")
		return
	if AudioService.current_bgm_id != 0x00040002 or AudioService.current_bgm_path.is_empty():
		_fail("selection BGM mismatch: %08X %s" % [AudioService.current_bgm_id, AudioService.current_bgm_path])
		return

	select.call("_on_server_message", NetworkClient.SM_QUERYCHARERROR, PackedByteArray([1]))
	if not select.get("_query_complete") or select.get("has_character") or select.get_node("InfoPanel").visible or select.get_node("StartButton").visible or not select.get_node("CreateButton").visible or select.get_node("DeleteButton").visible:
		_fail("confirmed empty account did not expose only character creation")
		return
	if OS.has_environment("MIR2X_ACCOUNT_FLOW_SCREENSHOT"):
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(OS.get_environment("MIR2X_ACCOUNT_FLOW_SCREENSHOT"))

	select.call("_on_server_message", NetworkClient.SM_QUERYCHAROK, _character_payload("账号流程测试", 1, 1, 1100))
	if not select.get("has_character") or not select.get_node("StartButton").visible or not select.get_node("CreateButton").visible or not select.get_node("DeleteButton").visible:
		_fail("existing character actions were not restored")
		return
	if "账号流程测试" not in select.get_node("InfoPanel/CharacterInfo").text or not select.get_node("InfoPanel").visible or not select.get_node("CharacterSprite").visible:
		_fail("queried character preview was not applied")
		return
	select.call("_on_server_message", NetworkClient.SM_DELETECHAROK, PackedByteArray())
	if select.get_node("StartButton").visible or not select.get_node("CreateButton").visible or select.get_node("DeleteButton").visible:
		_fail("deleted character did not return to the playable creation state")
		return

	select.queue_free()
	await get_tree().process_frame
	var login := load("res://scenes/account/login.tscn").instantiate() as Control
	add_child(login)
	await get_tree().process_frame
	if AudioService.current_bgm_id != 0x00040007 or AudioService.current_bgm_path.is_empty():
		_fail("login BGM mismatch: %08X %s" % [AudioService.current_bgm_id, AudioService.current_bgm_path])
		return
	if not (AudioService.get_node("BGMPlayer").stream is AudioStreamWAV):
		_fail("original WAV opening music did not use the WAV decoder")
		return
	login.queue_free()
	await get_tree().process_frame
	var create := load("res://scenes/account/create_character.tscn").instantiate() as Control
	add_child(create)
	await get_tree().process_frame
	if AudioService.current_bgm_id != 0x00040001 or AudioService.current_bgm_path.is_empty():
		_fail("character-creation BGM mismatch: %08X %s" % [AudioService.current_bgm_id, AudioService.current_bgm_path])
		return
	print("ACCOUNT FLOW PASS: pending/empty/existing states, playable creation path and original BGM transitions")
	get_tree().quit()


func _character_payload(name: String, gender: int, job: int, experience: int) -> PackedByteArray:
	var encoded := name.to_utf8_buffer()
	var payload := PackedByteArray()
	payload.resize(74)
	payload.encode_u16(0, encoded.size())
	for index in range(mini(encoded.size(), 64)):
		payload[2 + index] = encoded[index]
	payload[68] = gender
	payload[69] = job
	payload.encode_u32(70, experience)
	return payload


func _fail(message: String) -> void:
	push_error("ACCOUNT_FLOW_SMOKE %s" % message)
	get_tree().quit(1)
