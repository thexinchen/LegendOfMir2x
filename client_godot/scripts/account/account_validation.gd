extends RefCounted

const EMAIL_PATTERN := "^(?:[^<>()\\[\\]\\\\.,;:\\s@\"]+(?:\\.[^<>()\\[\\]\\\\.,;:\\s@\"]+)*|\".+\")@(?:(?:(?:\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(?:\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])|(?:[a-zA-Z\\-0-9]+\\.)+[a-zA-Z]{2,})$"


static func is_email(value: String) -> bool:
	var expression := RegEx.create_from_string(EMAIL_PATTERN)
	return expression.search(value) != null


static func is_password(value: String) -> bool:
	var has_digit := false
	var has_lower := false
	var has_upper := false
	var has_special := false
	for index in value.length():
		var code := value.unicode_at(index)
		has_digit = has_digit or (code >= 48 and code <= 57)
		has_upper = has_upper or (code >= 65 and code <= 90)
		has_lower = has_lower or (code >= 97 and code <= 122)
		has_special = has_special or "~!@#$%^&*()".contains(value[index])
	return value.length() >= 8 and has_digit and has_lower and has_upper and has_special
