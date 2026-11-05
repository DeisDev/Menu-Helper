#pragma once

namespace MenuHelper::Translations
{
	bool IsValidModName(std::string_view a_modName);

	// Called once the game has read its INI files, which hold the language.
	void OnGameReady();

	// Returns the translation of a_text, or a_text itself. Translations stay valid until the game exits.
	std::string_view Lookup(std::string_view a_modName, std::string_view a_text);

	// Adds a_text to the mod's translation template when MenuHelper.ini enables templates.
	void AddToTemplate(std::string_view a_modName, std::string_view a_text);
}
