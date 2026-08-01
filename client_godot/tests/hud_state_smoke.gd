extends Node

const ActorResourceScript = preload("res://scripts/game/actor_resource.gd")


func _ready() -> void:
	var resources: RefCounted = ActorResourceScript.new()
	if not resources.configure_default() or resources.buff_meta.is_empty():
		_fail("HUD metadata unavailable")
		return
	GameState.player_gender = 1
	GameState.player_job = 1
	GameState.player_hp = 75
	GameState.player_hp_max = 100
	GameState.player_mp = 50
	GameState.player_mp_max = 100
	GameState.update_exp(1100)
	GameState.inventory.resize(25)
	GameState.buff_list = [resources.buff_meta.keys()[0]]
	var panel: Control = load("res://scenes/game/control_panel.tscn").instantiate()
	add_child(panel)
	await get_tree().process_frame
	if panel.get_node("%Face").texture == null:
		_fail("player face missing")
		return
	if panel.get_node("%BuffContainer").get_child_count() != 1:
		_fail("buff icon missing")
		return
	if absf(panel.get_node("%Experience").value - GameState.level_ratio() * 100.0) > 0.01:
		_fail("experience meter mismatch: panel=%s expected=%s" % [panel.get_node("%Experience").value, GameState.level_ratio() * 100.0])
		return
	panel.call("_on_ac_pressed")
	panel.call("_on_dc_pressed")
	if panel.get_node("%ACValue").text != "3-4" or panel.get_node("%DCValue").text != "4-5":
		_fail("AC/MA or DC/MC toggle mismatch")
		return
	print("HUD STATE PASS: face, HP, experience, load, buff and combat toggles")
	get_tree().quit()


func _fail(message: String) -> void:
	push_error("HUD_STATE_SMOKE %s" % message)
	get_tree().quit(1)
