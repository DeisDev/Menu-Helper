#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper
{
	struct Item;
}

namespace MenuHelper::Submenus
{
	// UI thread only.
	void Attach(RE::IMenu& a_host, std::uint32_t a_hostBit);
	void OnMainListRebuilt(RE::IMenu& a_host);
	void CloseAll(std::uint32_t a_hostBit);
	bool HandleButtonEvent(RE::IMenu& a_host, const RE::ButtonEvent& a_event);
	void QueueOpen(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu, std::int32_t a_selection = -1);
	// Runs onChange, setting listeners and Papyrus events for a value the player changed.
	void NotifyChanged(const Item& a_item);

	// Any thread.
	bool RequestOpen(MenuHelper_MenuId a_menu);
	bool RequestClose(MenuHelper_MenuId a_menu);
	void RequestRefresh(MenuHelper_MenuId a_menu);
	// Shows a confirm over the open MenuHelper menu. a_done runs once on the UI thread; false when it could not be shown.
	bool RequestConfirm(MenuHelper_BoxId a_id, std::string a_text, std::function<void(bool)> a_done);
}
