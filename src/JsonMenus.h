#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper
{
	struct Item;
}

namespace MenuHelper::JsonMenus
{
	// Reads Data/MenuHelper/Config/<ModName>/menu.json for every mod. Call once game data is ready.
	void Load();

	bool RegisterSettingListener(std::string_view a_modName, MenuHelper_SettingCallback a_callback, void* a_user);
	bool RegisterActionListener(std::string_view a_modName, MenuHelper_ActionCallback a_callback, void* a_user);

	// UI thread. A value with a save key changed.
	void NotifySetting(std::string_view a_modName, std::string_view a_saveKey, double a_value);
	// UI thread. Reports the item's action and calls its Papyrus function, if it has them.
	void RunAction(const Item& a_item);
}
