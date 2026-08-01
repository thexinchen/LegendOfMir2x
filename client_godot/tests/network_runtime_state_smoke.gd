extends Node

const Protocol = preload("res://scripts/network/protocol.gd")
const CerealReader = preload("res://scripts/network/cereal_reader.gd")

var _player_uid: int = 0
var _finished := false
var _received := {
	"scene": false,
	"health": false,
	"inventory": false,
	"belt": false,
	"magic": false,
	"config": false,
}


func _ready() -> void:
	NetworkClient.connection_changed.connect(_on_connection_changed)
	NetworkClient.message_received.connect(_on_message_received)
	NetworkClient.connect_to_server()
	get_tree().create_timer(12.0).timeout.connect(_on_timeout)


func _on_connection_changed(connected: bool, message: String) -> void:
	print("NETWORK_RUNTIME_STATE connection: ", message)
	if connected and NetworkClient.login("test", "123456") != OK:
		_fail("failed to send login", 2)


func _on_message_received(head_code: int, payload: PackedByteArray) -> void:
	if _finished:
		return
	match head_code:
		NetworkClient.SM_LOGINOK:
			if NetworkClient.enter_game() != OK:
				_fail("failed to send enter game", 3)
		NetworkClient.SM_LOGINERROR:
			_fail("login failed: %d" % (payload[0] if not payload.is_empty() else -1), 4)
		NetworkClient.SM_ONLINEERROR:
			_fail("enter game failed: %d" % (payload[0] if not payload.is_empty() else -1), 5)
		NetworkClient.SM_ONLINEOK:
			var online := Protocol.decode_sm_online_ok(payload)
			_player_uid = online.get("uid", 0)
			if _player_uid == 0:
				_fail("invalid player UID in SM_ONLINEOK", 6)
		NetworkClient.SM_STARTGAMESCENE:
			var reader := CerealReader.new(payload)
			var scene := reader.read_sd_start_game_scene()
			_check_reader(reader, "SM_STARTGAMESCENE")
			if scene.get("uid", 0) != _player_uid:
				_fail("scene UID mismatch", 7)
			_received.scene = true
			print("NETWORK_RUNTIME_STATE scene: ", scene)
		NetworkClient.SM_HEALTH:
			var reader := CerealReader.new(payload)
			var health := reader.read_sd_health()
			_check_reader(reader, "SM_HEALTH")
			if health.get("uid", 0) == _player_uid:
				_received.health = true
				print("NETWORK_RUNTIME_STATE health: ", health)
		NetworkClient.SM_INVENTORY:
			var reader := CerealReader.new(payload)
			var items := reader.read_sd_inventory()
			_check_reader(reader, "SM_INVENTORY")
			_received.inventory = true
			print("NETWORK_RUNTIME_STATE inventory items: ", items.size())
		NetworkClient.SM_BELT:
			var reader := CerealReader.new(payload)
			var items := reader.read_sd_belt()
			_check_reader(reader, "SM_BELT")
			_received.belt = true
			print("NETWORK_RUNTIME_STATE belt items: ", items.size())
		NetworkClient.SM_LEARNEDMAGICLIST:
			var reader := CerealReader.new(payload)
			var magic_list := reader.read_sd_learned_magic_list()
			_check_reader(reader, "SM_LEARNEDMAGICLIST")
			_received.magic = true
			print("NETWORK_RUNTIME_STATE learned magic: ", magic_list.size())
		NetworkClient.SM_PLAYERCONFIG:
			var reader := CerealReader.new(payload)
			var config := reader.read_sd_player_config()
			_check_reader(reader, "SM_PLAYERCONFIG")
			_received.config = true
			print("NETWORK_RUNTIME_STATE config: ", config)
	_check_complete()


func _check_reader(reader: RefCounted, packet_name: String) -> void:
	if not reader.valid:
		_fail("%s parse failed: %s" % [packet_name, reader.error], 9)
	if not reader.at_end():
		_fail("%s has %d unread archive bytes" % [packet_name, reader.remaining()], 10)


func _check_complete() -> void:
	for key in _received:
		if not _received[key]:
			return
	print("NETWORK RUNTIME STATE PASS")
	_finished = true
	NetworkClient.disconnect_from_server()
	get_tree().quit()


func _on_timeout() -> void:
	_fail("timed out; received=%s" % _received, 11)


func _fail(message: String, exit_code: int) -> void:
	if _finished:
		return
	_finished = true
	push_error("NETWORK_RUNTIME_STATE %s" % message)
	NetworkClient.disconnect_from_server()
	get_tree().quit(exit_code)
