#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper::Dialogs
{
	MenuHelper_BoxId NextId();

	// Any thread. Text is translated for a_modName. Callbacks run on the game's UI thread.
	MenuHelper_BoxId ShowMessageBox(std::string_view a_modName, std::string_view a_body, const std::vector<std::string>& a_buttons, std::function<void(MenuHelper_BoxId, std::uint32_t)> a_onButton);
	MenuHelper_BoxId ShowConfirm(std::string_view a_modName, std::string_view a_text, std::function<void(MenuHelper_BoxId, bool)> a_onResult);
	bool             ShowNotification(std::string_view a_modName, std::string_view a_text, std::string_view a_sound, bool a_warning);
}
