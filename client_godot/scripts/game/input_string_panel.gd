extends "res://scripts/game/closable_panel.gd"

signal committed(value: String)
signal cancelled

var _clamping_text := false


func _ready() -> void:
	super._ready()
	$ConfirmButton.pressed.connect(_confirm)
	$CancelButton.pressed.connect(_cancel)
	$Value.text_submitted.connect(func(_value: String): _confirm())
	$Value.text_changed.connect(_on_text_changed)


func configure(title: String, security: bool) -> void:
	$Title.text = _plain_layout_text(title)
	$Value.secret = security
	$Value.text = ""
	show()
	$Value.grab_focus()


func _confirm() -> void:
	var value: String = $Value.text.strip_edges(true, false)
	$Value.text = ""
	committed.emit(value)
	hide()


func _cancel() -> void:
	$Value.text = ""
	hide()
	cancelled.emit()


func close_for_escape() -> bool:
	_cancel()
	return true


func _on_text_changed(value: String) -> void:
	if _clamping_text or value.to_utf8_buffer().size() <= 255:
		return
	_clamping_text = true
	$Value.text = _utf8_prefix(value, 255)
	$Value.caret_column = $Value.text.length()
	_clamping_text = false


func _utf8_prefix(value: String, byte_limit: int) -> String:
	var result := ""
	for index in value.length():
		var next := result + value.substr(index, 1)
		if next.to_utf8_buffer().size() > byte_limit:
			break
		result = next
	return result


func _plain_layout_text(layout: String) -> String:
	var source := layout.replace("<br>", "\n").replace("<br/>", "\n")
	var result := ""
	var in_tag := false
	for index in source.length():
		var character := source.substr(index, 1)
		if character == "<":
			in_tag = true
		elif character == ">":
			in_tag = false
		elif not in_tag:
			result += character
	return result
