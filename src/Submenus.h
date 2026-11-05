#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper::Submenus
{
	// UI thread only.
	void Attach(RE::IMenu& a_host, std::uint32_t a_hostBit);
	void OnMainListRebuilt(RE::IMenu& a_host);
	void CloseAll(std::uint32_t a_hostBit);
	bool HandleButtonEvent(RE::IMenu& a_host, const RE::ButtonEvent& a_event);
	void QueueOpen(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu);

	// Any thread.
	bool RequestOpen(MenuHelper_MenuId a_menu);
	bool RequestClose(MenuHelper_MenuId a_menu);
	void RequestRefresh(MenuHelper_MenuId a_menu);
}
