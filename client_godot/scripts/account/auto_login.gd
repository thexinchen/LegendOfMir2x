extends RefCounted


static func credentials() -> PackedStringArray:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--auto-login="):
			return _validated(arg.substr("--auto-login=".length()))
	if OS.has_environment("MIR2X_AUTO_LOGIN"):
		return _validated(OS.get_environment("MIR2X_AUTO_LOGIN"))
	return PackedStringArray()


static func requested() -> bool:
	return credentials().size() == 2


static func _validated(value: String) -> PackedStringArray:
	var parts := value.split(":", true, 1)
	return parts if parts.size() == 2 and not parts[0].is_empty() and not parts[1].is_empty() else PackedStringArray()
