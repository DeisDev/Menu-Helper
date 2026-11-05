#pragma once

namespace MenuHelper::SavedValues
{
	bool IsValidKey(std::string_view a_key);

	std::optional<float> Load(std::string_view a_modName, std::string_view a_key);
	void                 Save(std::string_view a_modName, std::string_view a_key, float a_value);
}
