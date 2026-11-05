#pragma once

namespace MenuHelper::SavedValues
{
	bool IsValidKey(std::string_view a_key);

	std::optional<double> Load(std::string_view a_modName, std::string_view a_key);
	// Writes to disk shortly after the last change, or on Flush.
	void                  Save(std::string_view a_modName, std::string_view a_key, double a_value);
	void                  Flush();
}
