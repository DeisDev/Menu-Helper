#pragma once

namespace MenuHelper::GFx
{
	using Value = Scaleform::GFx::Value;

	inline std::optional<double> ToNumber(const Value& a_value)
	{
		if (a_value.IsNumber()) {
			return a_value.GetNumber();
		}
		if (a_value.IsInt()) {
			return a_value.GetInt();
		}
		if (a_value.IsUInt()) {
			return a_value.GetUInt();
		}
		return std::nullopt;
	}

	inline bool IsTrue(const Value& a_value)
	{
		return a_value.IsBoolean() && a_value.GetBoolean();
	}

	// List entries are rendered through htmlText, so plain-text labels need escaping.
	inline std::string EscapeHtml(std::string_view a_text)
	{
		std::string result;
		result.reserve(a_text.size());
		for (const char c : a_text) {
			switch (c) {
				case '&':
					result += "&amp;";
					break;
				case '<':
					result += "&lt;";
					break;
				case '>':
					result += "&gt;";
					break;
				default:
					result += c;
			}
		}
		return result;
	}
}
