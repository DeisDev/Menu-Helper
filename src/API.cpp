#include "API.h"

#include "Dialogs.h"
#include "Hotkeys.h"
#include "Injector.h"
#include "JsonMenus.h"
#include "Registry.h"
#include "SavedValues.h"
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

	bool IsModName(const char* a_modName)
	{
		return a_modName && MenuHelper::Translations::IsValidModName(a_modName);
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
		auto       item = desc ? Registry::MakeItem(*desc) : std::nullopt;
		const auto id = item ? Registry::Get().AddItem(a_menu, std::move(*item)) : MENUHELPER_INVALID_ID;
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

	int SetItemVisible(MenuHelper_ItemId a_item, int a_visible)
	{
		return RefreshMenuIf(Registry::Get().SetItemVisible(a_item, a_visible != 0));
	}

	int SetItemValue(MenuHelper_ItemId a_item, double a_value)
	{
		return RefreshMenuIf(Registry::Get().SetItemValue(a_item, a_value));
	}

	double GetItemValue(MenuHelper_ItemId a_item)
	{
		const auto item = Registry::Get().FindItem(a_item);
		return item ? item->value : 0.0;
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
		if (!a_text || !IsModName(a_modName)) {
			return a_text;
		}
		return MenuHelper::Translations::Lookup(a_modName, a_text).data();
	}

	const char* GetKeyName(std::uint32_t a_hotkey)
	{
		return MenuHelper::Hotkeys::KeyNameStable(a_hotkey);
	}

	MenuHelper_BoxId ShowMessageBox(const MenuHelper_MessageBoxDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		if (!desc || !desc->body || !desc->buttons || !IsModName(desc->modName) ||
			!std::all_of(desc->buttons, desc->buttons + desc->buttonCount, [](const char* a_button) { return a_button != nullptr; })) {
			return MENUHELPER_INVALID_ID;
		}
		const std::vector<std::string> buttons(desc->buttons, desc->buttons + desc->buttonCount);
		auto onButton = [callback = desc->onButton, user = desc->user](MenuHelper_BoxId a_box, std::uint32_t a_button) {
			if (callback) {
				callback(a_box, a_button, user);
			}
		};
		return MenuHelper::Dialogs::ShowMessageBox(desc->modName, desc->body, buttons, std::move(onButton));
	}

	MenuHelper_BoxId ShowConfirm(const MenuHelper_ConfirmDesc* a_desc)
	{
		const auto desc = ReadDesc(a_desc);
		if (!desc || !desc->text || !IsModName(desc->modName)) {
			return MENUHELPER_INVALID_ID;
		}
		auto onResult = [callback = desc->onResult, user = desc->user](MenuHelper_BoxId a_confirm, bool a_result) {
			if (callback) {
				callback(a_confirm, a_result, user);
			}
		};
		return MenuHelper::Dialogs::ShowConfirm(desc->modName, desc->text, std::move(onResult));
	}

	int ShowNotification(const char* a_modName, const char* a_text, const char* a_sound, int a_warning)
	{
		return a_text && IsModName(a_modName) && MenuHelper::Dialogs::ShowNotification(a_modName, a_text, a_sound ? a_sound : "", a_warning != 0);
	}

	int GetSavedValue(const char* a_modName, const char* a_saveKey, double* a_value)
	{
		if (!a_value || !a_saveKey || !IsModName(a_modName)) {
			return false;
		}
		const auto value = MenuHelper::SavedValues::Load(a_modName, a_saveKey);
		if (value) {
			*a_value = *value;
		}
		return value.has_value();
	}

	int SetSavedValue(const char* a_modName, const char* a_saveKey, double a_value)
	{
		if (!a_saveKey || !IsModName(a_modName) || !MenuHelper::SavedValues::IsValidKey(a_saveKey) || !std::isfinite(a_value)) {
			return false;
		}
		MenuHelper::SavedValues::Save(a_modName, a_saveKey, a_value);
		for (const auto menu : Registry::Get().SetSavedItemValues(a_modName, a_saveKey, a_value)) {
			RefreshMenuIf(menu);
		}
		return true;
	}

	int RegisterSettingListener(const char* a_modName, MenuHelper_SettingCallback a_callback, void* a_user)
	{
		return a_callback && IsModName(a_modName) && MenuHelper::JsonMenus::RegisterSettingListener(a_modName, a_callback, a_user);
	}

	int RegisterActionListener(const char* a_modName, MenuHelper_ActionCallback a_callback, void* a_user)
	{
		return a_callback && IsModName(a_modName) && MenuHelper::JsonMenus::RegisterActionListener(a_modName, a_callback, a_user);
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
		.SetItemVisible = SetItemVisible,
		.SetItemValue = SetItemValue,
		.GetItemValue = GetItemValue,
		.OpenMenu = OpenMenu,
		.CloseMenu = CloseMenu,
		.ChangeVanillaEntry = ChangeVanillaEntry,
		.RemoveVanillaChange = RemoveVanillaChange,
		.Translate = Translate,
		.GetKeyName = GetKeyName,
		.ShowMessageBox = ShowMessageBox,
		.ShowConfirm = ShowConfirm,
		.ShowNotification = ShowNotification,
		.GetSavedValue = GetSavedValue,
		.SetSavedValue = SetSavedValue,
		.RegisterSettingListener = RegisterSettingListener,
		.RegisterActionListener = RegisterActionListener,
	};
}

const MenuHelper_API* MenuHelper_GetAPI(std::uint32_t a_requestedVersion)
{
	return a_requestedVersion == MENUHELPER_API_VERSION ? &g_api : nullptr;
}
