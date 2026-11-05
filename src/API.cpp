#include "API.h"

#include "Injector.h"
#include "Registry.h"
#include "Submenus.h"
#include "Translations.h"

namespace
{
	using MenuHelper::Registry;

	template <class T>
	const T* ReadDesc(const T* a_desc)
	{
		return a_desc && a_desc->structSize >= sizeof(T) ? a_desc : nullptr;
	}

	int RefreshMenuIf(MenuHelper_MenuId a_menu)
	{
		if (a_menu == MENUHELPER_INVALID_ID) {
			return false;
		}
		MenuHelper::Submenus::RequestRefresh(a_menu);
		return true;
	}

	int RefreshEntriesIf(bool a_changed)
	{
		if (a_changed) {
			MenuHelper::Injector::RefreshOpenMenus();
		}
		return a_changed;
	}

	MenuHelper_EntryId AddEntry(const MenuHelper_EntryDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		const auto id = desc ? Registry::Get().AddEntry(*desc) : MENUHELPER_INVALID_ID;
		RefreshEntriesIf(id != MENUHELPER_INVALID_ID);
		return id;
	}

	int RemoveEntry(MenuHelper_EntryId a_entry)
	{
		return RefreshEntriesIf(Registry::Get().RemoveEntry(a_entry));
	}

	int SetEntryLabel(MenuHelper_EntryId a_entry, const char* a_label)
	{
		return RefreshEntriesIf(a_label && Registry::Get().SetEntryLabel(a_entry, a_label));
	}

	int SetEntryEnabled(MenuHelper_EntryId a_entry, int a_enabled)
	{
		return RefreshEntriesIf(Registry::Get().SetEntryEnabled(a_entry, a_enabled != 0));
	}

	MenuHelper_MenuId CreateMenu(const MenuHelper_MenuDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		return desc ? Registry::Get().CreateMenu(*desc) : MENUHELPER_INVALID_ID;
	}

	int DestroyMenu(MenuHelper_MenuId a_menu)
	{
		return RefreshMenuIf(Registry::Get().DestroyMenu(a_menu) ? a_menu : MENUHELPER_INVALID_ID);
	}

	MenuHelper_ItemId AddItem(MenuHelper_MenuId a_menu, const MenuHelper_ItemDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		const auto id = desc ? Registry::Get().AddItem(a_menu, *desc) : MENUHELPER_INVALID_ID;
		if (id != MENUHELPER_INVALID_ID) {
			RefreshMenuIf(a_menu);
		}
		return id;
	}

	int RemoveItem(MenuHelper_ItemId a_item)
	{
		return RefreshMenuIf(Registry::Get().RemoveItem(a_item));
	}

	int SetItemLabel(MenuHelper_ItemId a_item, const char* a_label)
	{
		return a_label && RefreshMenuIf(Registry::Get().SetItemLabel(a_item, a_label));
	}

	int SetItemDescription(MenuHelper_ItemId a_item, const char* a_description)
	{
		return RefreshMenuIf(Registry::Get().SetItemDescription(a_item, a_description ? a_description : ""));
	}

	int SetItemEnabled(MenuHelper_ItemId a_item, int a_enabled)
	{
		return RefreshMenuIf(Registry::Get().SetItemEnabled(a_item, a_enabled != 0));
	}

	int SetItemValue(MenuHelper_ItemId a_item, float a_value)
	{
		return RefreshMenuIf(Registry::Get().SetItemValue(a_item, a_value));
	}

	float GetItemValue(MenuHelper_ItemId a_item)
	{
		const auto item = Registry::Get().FindItem(a_item);
		return item ? item->value : 0.0F;
	}

	int OpenMenu(MenuHelper_MenuId a_menu)
	{
		return MenuHelper::Submenus::RequestOpen(a_menu);
	}

	int CloseMenu(MenuHelper_MenuId a_menu)
	{
		return MenuHelper::Submenus::RequestClose(a_menu);
	}

	MenuHelper_ChangeId ChangeVanillaEntry(const MenuHelper_VanillaChangeDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		const auto id = desc ? Registry::Get().AddVanillaChange(*desc) : MENUHELPER_INVALID_ID;
		RefreshEntriesIf(id != MENUHELPER_INVALID_ID);
		return id;
	}

	int RemoveVanillaChange(MenuHelper_ChangeId a_change)
	{
		return RefreshEntriesIf(Registry::Get().RemoveVanillaChange(a_change));
	}

	const char* Translate(const char* a_modName, const char* a_text)
	{
		if (!a_modName || !a_text || !MenuHelper::Translations::IsValidModName(a_modName)) {
			return a_text;
		}
		return MenuHelper::Translations::Lookup(a_modName, a_text).data();
	}

	constexpr MenuHelper_API g_api{
		.version = MENUHELPER_API_VERSION,
		.AddEntry = AddEntry,
		.RemoveEntry = RemoveEntry,
		.SetEntryLabel = SetEntryLabel,
		.SetEntryEnabled = SetEntryEnabled,
		.CreateMenu = CreateMenu,
		.DestroyMenu = DestroyMenu,
		.AddItem = AddItem,
		.RemoveItem = RemoveItem,
		.SetItemLabel = SetItemLabel,
		.SetItemDescription = SetItemDescription,
		.SetItemEnabled = SetItemEnabled,
		.SetItemValue = SetItemValue,
		.GetItemValue = GetItemValue,
		.OpenMenu = OpenMenu,
		.CloseMenu = CloseMenu,
		.ChangeVanillaEntry = ChangeVanillaEntry,
		.RemoveVanillaChange = RemoveVanillaChange,
		.Translate = Translate,
	};
}

const MenuHelper_API* MenuHelper_GetAPI(std::uint32_t a_requestedVersion)
{
	return a_requestedVersion == MENUHELPER_API_VERSION ? &g_api : nullptr;
}
