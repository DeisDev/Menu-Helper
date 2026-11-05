#pragma once

namespace MenuHelper::Injector
{
	void Install();
	void InjectEntries(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit);
	void RefreshOpenMenus();
}
