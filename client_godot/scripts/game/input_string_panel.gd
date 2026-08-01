extends "res://scripts/game/closable_panel.gd"

signal committed(value: String)


func _ready() -> void:
	super._ready()
	$ConfirmButton.pressed.connect(_confirm)
	$CancelButton.pressed.connect(hide)
	$Value.text_submitted.connect(func(_value: String): _confirm())


func configure(title: String, show_value: bool) -> void:
	$Title.text = title
	$Value.secret = not show_value
	$Value.text = ""
	show()
	$Value.grab_focus()


func _confirm() -> void:
	committed.emit($Value.text)
	hide()
