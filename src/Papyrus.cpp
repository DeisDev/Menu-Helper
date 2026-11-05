#include "Papyrus.h"

#include "API.h"
#include "Translations.h"

namespace MenuHelper::Papyrus
{
	namespace
	{
		constexpr auto kScript = "MenuHelper"sv;
		constexpr auto kEntryPressEvent = "MenuHelper_OnEntryPress"sv;
		constexpr auto kItemChangeEvent = "MenuHelper_OnItemChange"sv;
		constexpr auto kItemSelectEvent = "MenuHelper_OnItemSelect"sv;
		constexpr auto kHotkeyEvent = "MenuHelper_OnHotkey"sv;
		constexpr auto kMenuOpenEvent = "MenuHelper_OnMenuOpen"sv;
		constexpr auto kMenuCloseEvent = "MenuHelper_OnMenuClose"sv;
		constexpr auto kMenuResetEvent = "MenuHelper_OnMenuReset"sv;
		constexpr auto kMessageBoxEvent = "MenuHelper_OnMessageBoxButton"sv;
		constexpr auto kConfirmEvent = "MenuHelper_OnConfirm"sv;
		constexpr auto kSettingChangeEvent = "MenuHelper_OnSettingChange"sv;
		constexpr auto kActionEvent = "MenuHelper_OnAction"sv;

		std::mutex                       g_lock;
		std::vector<MenuHelper_EntryId>  g_entries;
		std::vector<MenuHelper_MenuId>   g_menus;
		std::vector<MenuHelper_ChangeId> g_changes;

		const MenuHelper_API& Api()
		{
			static const auto api = MenuHelper_GetAPI(MENUHELPER_API_VERSION);
			return *api;
		}

		RE::BSScript::IVirtualMachine* VM()
		{
			const auto game = RE::GameVM::GetSingleton();
			return game ? game->GetVM().get() : nullptr;
		}

		template <class... Args>
		void SendEvent(std::string_view a_event, Args... a_args)
		{
			const auto vm = VM();
			if (!vm) {
				return;
			}
			auto dispatch = [&](std::uint64_t a_handle, const char* a_scriptName, const char* a_callbackName) {
				vm->DispatchMethodCall(a_handle, a_scriptName, a_callbackName, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{}, a_args...);
			};
			using Dispatch = decltype(dispatch);
			F4SE::GetPapyrusInterface()->GetExternalEventRegistrations(
				a_event, &dispatch,
				[](std::uint64_t a_handle, const char* a_scriptName, const char* a_callbackName, void* a_data) {
					(*static_cast<Dispatch*>(a_data))(a_handle, a_scriptName, a_callbackName);
				});
		}

		// Papyrus has no unsigned int; colors keep all 32 bits through a signed int.
		std::int32_t ToPapyrusBits(double a_value) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a_value)); }
		double       FromPapyrusBits(std::int32_t a_value) { return static_cast<double>(static_cast<std::uint32_t>(a_value)); }

		std::int32_t ToInt(MenuHelper_EntryId a_id) { return static_cast<std::int32_t>(a_id); }

		void OnEntryPress(MenuHelper_EntryId a_entry, std::uint32_t, void*) { SendEvent(kEntryPressEvent, ToInt(a_entry)); }
		// Papyrus floats cannot hold every color; scripts read colors with GetItemColor.
		void OnItemChange(MenuHelper_ItemId a_item, double a_value, void*) { SendEvent(kItemChangeEvent, ToInt(a_item), static_cast<float>(a_value)); }
		void OnItemSelect(MenuHelper_ItemId a_item, double, void*) { SendEvent(kItemSelectEvent, ToInt(a_item)); }
		void OnHotkey(MenuHelper_ItemId a_item, double, void*) { SendEvent(kHotkeyEvent, ToInt(a_item)); }
		void OnMenuOpen(MenuHelper_MenuId a_menu, void*) { SendEvent(kMenuOpenEvent, ToInt(a_menu)); }
		void OnMenuClose(MenuHelper_MenuId a_menu, void*) { SendEvent(kMenuCloseEvent, ToInt(a_menu)); }
		void OnMenuReset(MenuHelper_MenuId a_menu, void*) { SendEvent(kMenuResetEvent, ToInt(a_menu)); }
		void OnMessageBoxButton(MenuHelper_BoxId a_box, std::uint32_t a_button, void*) { SendEvent(kMessageBoxEvent, ToInt(a_box), static_cast<std::int32_t>(a_button)); }
		void OnConfirm(MenuHelper_BoxId a_confirm, int a_result, void*) { SendEvent(kConfirmEvent, ToInt(a_confirm), a_result != 0); }

		std::uint32_t ToId(std::int32_t a_id)
		{
			return a_id > 0 ? static_cast<std::uint32_t>(a_id) : MENUHELPER_INVALID_ID;
		}

		std::int32_t GetAPIVersion(std::monostate) { return static_cast<std::int32_t>(Api().version); }

		std::int32_t AddEntry(std::monostate, std::string_view a_modName, std::string_view a_label, std::int32_t a_submenu, std::int32_t a_insertBefore)
		{
			const std::string    modName{ a_modName };
			const std::string    label{ a_label };
			MenuHelper_EntryDesc desc{};
			desc.structSize = sizeof(desc);
			desc.modName = modName.c_str();
			desc.label = label.c_str();
			desc.menus = MENUHELPER_MENU_PAUSE;
			desc.insertBefore = a_insertBefore;
			desc.onPress = OnEntryPress;
			desc.submenu = ToId(a_submenu);
			const auto id = Api().AddEntry(&desc);
			if (id != MENUHELPER_INVALID_ID) {
				const std::scoped_lock l{ g_lock };
				g_entries.push_back(id);
			}
			return ToInt(id);
		}

		bool RemoveEntry(std::monostate, std::int32_t a_entry) { return Api().RemoveEntry(ToId(a_entry)) != 0; }

		bool SetEntryLabel(std::monostate, std::int32_t a_entry, std::string_view a_label)
		{
			return Api().SetEntryLabel(ToId(a_entry), std::string{ a_label }.c_str()) != 0;
		}

		bool SetEntryEnabled(std::monostate, std::int32_t a_entry, bool a_enabled) { return Api().SetEntryEnabled(ToId(a_entry), a_enabled) != 0; }

		std::int32_t CreateMenu(std::monostate, std::string_view a_modName, bool a_pages)
		{
			const std::string   modName{ a_modName };
			MenuHelper_MenuDesc desc{};
			desc.structSize = sizeof(desc);
			desc.modName = modName.c_str();
			desc.kind = a_pages ? MENUHELPER_MENU_PAGES : MENUHELPER_MENU_ITEMS;
			desc.onOpen = OnMenuOpen;
			desc.onClose = OnMenuClose;
			desc.onReset = OnMenuReset;
			const auto id = Api().CreateMenu(&desc);
			if (id != MENUHELPER_INVALID_ID) {
				const std::scoped_lock l{ g_lock };
				g_menus.push_back(id);
			}
			return ToInt(id);
		}

		bool DestroyMenu(std::monostate, std::int32_t a_menu) { return Api().DestroyMenu(ToId(a_menu)) != 0; }

		struct ItemText
		{
			std::string_view label;
			std::string_view description;
			std::string_view saveKey;
		};

		std::int32_t AddItem(std::int32_t a_menu, MenuHelper_ItemDesc& a_desc, const ItemText& a_text)
		{
			const std::string label{ a_text.label };
			const std::string description{ a_text.description };
			const std::string saveKey{ a_text.saveKey };
			a_desc.structSize = sizeof(a_desc);
			a_desc.label = label.c_str();
			a_desc.description = description.c_str();
			a_desc.saveKey = saveKey.empty() ? nullptr : saveKey.c_str();
			a_desc.onChange = OnItemChange;
			a_desc.onSelect = OnItemSelect;
			a_desc.onHotkey = a_desc.type == MENUHELPER_ITEM_HOTKEY ? OnHotkey : nullptr;
			return ToInt(Api().AddItem(ToId(a_menu), &a_desc));
		}

		MenuHelper_ItemDesc Desc(std::uint32_t a_type)
		{
			MenuHelper_ItemDesc desc{};
			desc.type = a_type;
			return desc;
		}

		std::int32_t AddButton(std::monostate, std::int32_t a_menu, std::string_view a_label, std::string_view a_description, std::int32_t a_submenu)
		{
			auto desc = Desc(MENUHELPER_ITEM_BUTTON);
			desc.submenu = ToId(a_submenu);
			return AddItem(a_menu, desc, { a_label, a_description });
		}

		std::int32_t AddCheckbox(std::monostate, std::int32_t a_menu, std::string_view a_label, bool a_checked, std::string_view a_description, std::string_view a_saveKey)
		{
			auto desc = Desc(MENUHELPER_ITEM_CHECKBOX);
			desc.value = a_checked ? 1.0 : 0.0;
			return AddItem(a_menu, desc, { a_label, a_description, a_saveKey });
		}

		std::int32_t AddOptions(std::uint32_t a_type, std::int32_t a_menu, std::string_view a_label, const std::vector<std::string>& a_options, std::int32_t a_index, std::string_view a_description, std::string_view a_saveKey)
		{
			std::vector<const char*> options;
			std::ranges::transform(a_options, std::back_inserter(options), &std::string::c_str);
			auto desc = Desc(a_type);
			desc.options = options.data();
			desc.optionCount = static_cast<std::uint32_t>(options.size());
			desc.value = a_index;
			return AddItem(a_menu, desc, { a_label, a_description, a_saveKey });
		}

		std::int32_t AddStepper(std::monostate, std::int32_t a_menu, std::string_view a_label, std::vector<std::string> a_options, std::int32_t a_index, std::string_view a_description, std::string_view a_saveKey)
		{
			return AddOptions(MENUHELPER_ITEM_STEPPER, a_menu, a_label, a_options, a_index, a_description, a_saveKey);
		}

		std::int32_t AddDropdown(std::monostate, std::int32_t a_menu, std::string_view a_label, std::vector<std::string> a_options, std::int32_t a_index, std::string_view a_description, std::string_view a_saveKey)
		{
			return AddOptions(MENUHELPER_ITEM_DROPDOWN, a_menu, a_label, a_options, a_index, a_description, a_saveKey);
		}

		std::int32_t AddSlider(std::monostate, std::int32_t a_menu, std::string_view a_label, float a_value, float a_min, float a_max, float a_step, std::string_view a_description, std::string_view a_saveKey, std::string_view a_format, std::int32_t a_decimals)
		{
			const std::string format{ a_format };
			auto              desc = Desc(MENUHELPER_ITEM_SLIDER);
			desc.value = a_value;
			desc.min = a_min;
			desc.max = a_max;
			desc.step = a_step;
			desc.format = format.empty() ? nullptr : format.c_str();
			desc.decimals = static_cast<std::uint32_t>(std::max(a_decimals, 0));
			return AddItem(a_menu, desc, { a_label, a_description, a_saveKey });
		}

		std::int32_t AddHotkey(std::monostate, std::int32_t a_menu, std::string_view a_label, std::int32_t a_hotkey, std::string_view a_description, std::string_view a_saveKey, bool a_modifiers, bool a_mouse, bool a_gamepad)
		{
			auto desc = Desc(MENUHELPER_ITEM_HOTKEY);
			desc.value = FromPapyrusBits(a_hotkey);
			desc.flags = (a_modifiers ? MENUHELPER_FLAG_HOTKEY_MODIFIERS : 0) | (a_mouse ? MENUHELPER_FLAG_HOTKEY_MOUSE : 0) | (a_gamepad ? MENUHELPER_FLAG_HOTKEY_GAMEPAD : 0);
			return AddItem(a_menu, desc, { a_label, a_description, a_saveKey });
		}

		std::int32_t AddColor(std::monostate, std::int32_t a_menu, std::string_view a_label, std::int32_t a_color, std::string_view a_description, std::string_view a_saveKey, bool a_alpha)
		{
			auto desc = Desc(MENUHELPER_ITEM_COLOR);
			desc.value = FromPapyrusBits(a_color);
			desc.flags = a_alpha ? MENUHELPER_FLAG_HASALPHA : 0;
			return AddItem(a_menu, desc, { a_label, a_description, a_saveKey });
		}

		std::int32_t AddHeader(std::monostate, std::int32_t a_menu, std::string_view a_label)
		{
			auto desc = Desc(MENUHELPER_ITEM_HEADER);
			return AddItem(a_menu, desc, { a_label, {} });
		}

		std::int32_t AddText(std::monostate, std::int32_t a_menu, std::string_view a_label, std::string_view a_description)
		{
			auto desc = Desc(MENUHELPER_ITEM_TEXT);
			return AddItem(a_menu, desc, { a_label, a_description });
		}

		std::int32_t AddTopic(std::monostate, std::int32_t a_menu, std::string_view a_title, std::string_view a_text, std::string_view a_image, std::int32_t a_imageWidth, std::int32_t a_imageHeight)
		{
			const std::string image{ a_image };
			auto              desc = Desc(MENUHELPER_ITEM_TOPIC);
			desc.image = image.empty() ? nullptr : image.c_str();
			desc.imageWidth = static_cast<std::uint32_t>(std::max(a_imageWidth, 0));
			desc.imageHeight = static_cast<std::uint32_t>(std::max(a_imageHeight, 0));
			return AddItem(a_menu, desc, { a_title, a_text });
		}

		bool RemoveItem(std::monostate, std::int32_t a_item) { return Api().RemoveItem(ToId(a_item)) != 0; }

		bool SetItemLabel(std::monostate, std::int32_t a_item, std::string_view a_label)
		{
			return Api().SetItemLabel(ToId(a_item), std::string{ a_label }.c_str()) != 0;
		}

		bool SetItemDescription(std::monostate, std::int32_t a_item, std::string_view a_description)
		{
			return Api().SetItemDescription(ToId(a_item), std::string{ a_description }.c_str()) != 0;
		}

		bool         SetItemEnabled(std::monostate, std::int32_t a_item, bool a_enabled) { return Api().SetItemEnabled(ToId(a_item), a_enabled) != 0; }
		bool         SetItemVisible(std::monostate, std::int32_t a_item, bool a_visible) { return Api().SetItemVisible(ToId(a_item), a_visible) != 0; }
		bool         SetItemValue(std::monostate, std::int32_t a_item, float a_value) { return Api().SetItemValue(ToId(a_item), a_value) != 0; }
		float        GetItemValue(std::monostate, std::int32_t a_item) { return static_cast<float>(Api().GetItemValue(ToId(a_item))); }
		bool         SetItemBits(std::monostate, std::int32_t a_item, std::int32_t a_value) { return Api().SetItemValue(ToId(a_item), FromPapyrusBits(a_value)) != 0; }
		std::int32_t GetItemBits(std::monostate, std::int32_t a_item) { return ToPapyrusBits(Api().GetItemValue(ToId(a_item))); }
		bool         OpenMenu(std::monostate, std::int32_t a_menu) { return Api().OpenMenu(ToId(a_menu)) != 0; }
		bool         CloseMenu(std::monostate, std::int32_t a_menu) { return Api().CloseMenu(ToId(a_menu)) != 0; }

		std::int32_t ChangeVanillaEntry(std::monostate, std::int32_t a_entry, bool a_hidden, std::int32_t a_move)
		{
			MenuHelper_VanillaChangeDesc desc{};
			desc.structSize = sizeof(desc);
			desc.menus = MENUHELPER_MENU_PAUSE;
			desc.entry = a_entry;
			desc.hidden = a_hidden;
			desc.move = a_move;
			const auto id = Api().ChangeVanillaEntry(&desc);
			if (id != MENUHELPER_INVALID_ID) {
				const std::scoped_lock l{ g_lock };
				g_changes.push_back(id);
			}
			return ToInt(id);
		}

		bool RemoveVanillaChange(std::monostate, std::int32_t a_change) { return Api().RemoveVanillaChange(ToId(a_change)) != 0; }

		std::string Translate(std::monostate, std::string_view a_modName, std::string_view a_text)
		{
			return std::string{ Translations::IsValidModName(a_modName) ? Translations::Lookup(a_modName, a_text) : a_text };
		}

		std::string FillPlaceholder(std::monostate, std::string_view a_text, std::string_view a_name, std::string_view a_value)
		{
			std::string text{ a_text };
			const auto  placeholder = std::format("{{{}}}", a_name);
			for (auto at = text.find(placeholder); at != std::string::npos; at = text.find(placeholder, at + a_value.size())) {
				text.replace(at, placeholder.size(), a_value);
			}
			return text;
		}

		std::string GetKeyName(std::monostate, std::int32_t a_hotkey) { return Api().GetKeyName(static_cast<std::uint32_t>(a_hotkey)); }

		std::int32_t ShowMessageBox(std::monostate, std::string_view a_modName, std::string_view a_body, std::vector<std::string> a_buttons)
		{
			const std::string        modName{ a_modName };
			const std::string        body{ a_body };
			std::vector<const char*> buttons;
			std::ranges::transform(a_buttons, std::back_inserter(buttons), &std::string::c_str);
			MenuHelper_MessageBoxDesc desc{};
			desc.structSize = sizeof(desc);
			desc.modName = modName.c_str();
			desc.body = body.c_str();
			desc.buttons = buttons.data();
			desc.buttonCount = static_cast<std::uint32_t>(buttons.size());
			desc.onButton = OnMessageBoxButton;
			return ToInt(Api().ShowMessageBox(&desc));
		}

		std::int32_t ShowConfirm(std::monostate, std::string_view a_modName, std::string_view a_text)
		{
			const std::string      modName{ a_modName };
			const std::string      text{ a_text };
			MenuHelper_ConfirmDesc desc{};
			desc.structSize = sizeof(desc);
			desc.modName = modName.c_str();
			desc.text = text.c_str();
			desc.onResult = OnConfirm;
			return ToInt(Api().ShowConfirm(&desc));
		}

		bool ShowNotification(std::monostate, std::string_view a_modName, std::string_view a_text, std::string_view a_sound, bool a_warning)
		{
			return Api().ShowNotification(std::string{ a_modName }.c_str(), std::string{ a_text }.c_str(), std::string{ a_sound }.c_str(), a_warning) != 0;
		}

		std::optional<double> GetSaved(std::string_view a_modName, std::string_view a_saveKey)
		{
			double value{};
			return Api().GetSavedValue(std::string{ a_modName }.c_str(), std::string{ a_saveKey }.c_str(), &value) ? std::optional{ value } : std::nullopt;
		}

		bool SetSaved(std::string_view a_modName, std::string_view a_saveKey, double a_value)
		{
			return Api().SetSavedValue(std::string{ a_modName }.c_str(), std::string{ a_saveKey }.c_str(), a_value) != 0;
		}

		float GetSavedFloat(std::monostate, std::string_view a_modName, std::string_view a_saveKey, float a_default)
		{
			return static_cast<float>(GetSaved(a_modName, a_saveKey).value_or(a_default));
		}

		std::int32_t GetSavedInt(std::monostate, std::string_view a_modName, std::string_view a_saveKey, std::int32_t a_default)
		{
			const auto value = GetSaved(a_modName, a_saveKey);
			return value ? static_cast<std::int32_t>(std::clamp(*value, -2147483648.0, 2147483647.0)) : a_default;
		}

		bool GetSavedBool(std::monostate, std::string_view a_modName, std::string_view a_saveKey, bool a_default)
		{
			const auto value = GetSaved(a_modName, a_saveKey);
			return value ? *value != 0.0 : a_default;
		}

		std::int32_t GetSavedColor(std::monostate, std::string_view a_modName, std::string_view a_saveKey, std::int32_t a_default)
		{
			const auto value = GetSaved(a_modName, a_saveKey);
			return value ? ToPapyrusBits(*value) : a_default;
		}

		bool SetSavedFloat(std::monostate, std::string_view a_modName, std::string_view a_saveKey, float a_value) { return SetSaved(a_modName, a_saveKey, a_value); }
		bool SetSavedInt(std::monostate, std::string_view a_modName, std::string_view a_saveKey, std::int32_t a_value) { return SetSaved(a_modName, a_saveKey, a_value); }
		bool SetSavedBool(std::monostate, std::string_view a_modName, std::string_view a_saveKey, bool a_value) { return SetSaved(a_modName, a_saveKey, a_value ? 1.0 : 0.0); }
		bool SetSavedColor(std::monostate, std::string_view a_modName, std::string_view a_saveKey, std::int32_t a_value) { return SetSaved(a_modName, a_saveKey, FromPapyrusBits(a_value)); }
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->BindNativeMethod(kScript, "GetAPIVersion"sv, GetAPIVersion, true);
		a_vm->BindNativeMethod(kScript, "AddEntry"sv, AddEntry, true);
		a_vm->BindNativeMethod(kScript, "RemoveEntry"sv, RemoveEntry, true);
		a_vm->BindNativeMethod(kScript, "SetEntryLabel"sv, SetEntryLabel, true);
		a_vm->BindNativeMethod(kScript, "SetEntryEnabled"sv, SetEntryEnabled, true);
		a_vm->BindNativeMethod(kScript, "CreateMenu"sv, CreateMenu, true);
		a_vm->BindNativeMethod(kScript, "DestroyMenu"sv, DestroyMenu, true);
		a_vm->BindNativeMethod(kScript, "AddButton"sv, AddButton, true);
		a_vm->BindNativeMethod(kScript, "AddCheckbox"sv, AddCheckbox, true);
		a_vm->BindNativeMethod(kScript, "AddStepper"sv, AddStepper, true);
		a_vm->BindNativeMethod(kScript, "AddDropdown"sv, AddDropdown, true);
		a_vm->BindNativeMethod(kScript, "AddSlider"sv, AddSlider, true);
		a_vm->BindNativeMethod(kScript, "AddHotkey"sv, AddHotkey, true);
		a_vm->BindNativeMethod(kScript, "AddColor"sv, AddColor, true);
		a_vm->BindNativeMethod(kScript, "AddHeader"sv, AddHeader, true);
		a_vm->BindNativeMethod(kScript, "AddText"sv, AddText, true);
		a_vm->BindNativeMethod(kScript, "AddTopic"sv, AddTopic, true);
		a_vm->BindNativeMethod(kScript, "RemoveItem"sv, RemoveItem, true);
		a_vm->BindNativeMethod(kScript, "SetItemLabel"sv, SetItemLabel, true);
		a_vm->BindNativeMethod(kScript, "SetItemDescription"sv, SetItemDescription, true);
		a_vm->BindNativeMethod(kScript, "SetItemEnabled"sv, SetItemEnabled, true);
		a_vm->BindNativeMethod(kScript, "SetItemVisible"sv, SetItemVisible, true);
		a_vm->BindNativeMethod(kScript, "SetItemValue"sv, SetItemValue, true);
		a_vm->BindNativeMethod(kScript, "GetItemValue"sv, GetItemValue, true);
		a_vm->BindNativeMethod(kScript, "SetItemColor"sv, SetItemBits, true);
		a_vm->BindNativeMethod(kScript, "GetItemColor"sv, GetItemBits, true);
		a_vm->BindNativeMethod(kScript, "SetItemHotkey"sv, SetItemBits, true);
		a_vm->BindNativeMethod(kScript, "GetItemHotkey"sv, GetItemBits, true);
		a_vm->BindNativeMethod(kScript, "OpenMenu"sv, OpenMenu, true);
		a_vm->BindNativeMethod(kScript, "CloseMenu"sv, CloseMenu, true);
		a_vm->BindNativeMethod(kScript, "ChangeVanillaEntry"sv, ChangeVanillaEntry, true);
		a_vm->BindNativeMethod(kScript, "RemoveVanillaChange"sv, RemoveVanillaChange, true);
		a_vm->BindNativeMethod(kScript, "Translate"sv, Translate, true);
		a_vm->BindNativeMethod(kScript, "FillPlaceholder"sv, FillPlaceholder, true);
		a_vm->BindNativeMethod(kScript, "GetKeyName"sv, GetKeyName, true);
		a_vm->BindNativeMethod(kScript, "ShowMessageBox"sv, ShowMessageBox, true);
		a_vm->BindNativeMethod(kScript, "ShowConfirm"sv, ShowConfirm, true);
		a_vm->BindNativeMethod(kScript, "ShowNotification"sv, ShowNotification, true);
		a_vm->BindNativeMethod(kScript, "GetSavedFloat"sv, GetSavedFloat, true);
		a_vm->BindNativeMethod(kScript, "GetSavedInt"sv, GetSavedInt, true);
		a_vm->BindNativeMethod(kScript, "GetSavedBool"sv, GetSavedBool, true);
		a_vm->BindNativeMethod(kScript, "GetSavedColor"sv, GetSavedColor, true);
		a_vm->BindNativeMethod(kScript, "SetSavedFloat"sv, SetSavedFloat, true);
		a_vm->BindNativeMethod(kScript, "SetSavedInt"sv, SetSavedInt, true);
		a_vm->BindNativeMethod(kScript, "SetSavedBool"sv, SetSavedBool, true);
		a_vm->BindNativeMethod(kScript, "SetSavedColor"sv, SetSavedColor, true);
		return true;
	}

	void ClearRegistrations()
	{
		std::vector<MenuHelper_EntryId>  entries;
		std::vector<MenuHelper_MenuId>   menus;
		std::vector<MenuHelper_ChangeId> changes;
		{
			const std::scoped_lock l{ g_lock };
			entries = std::exchange(g_entries, {});
			menus = std::exchange(g_menus, {});
			changes = std::exchange(g_changes, {});
		}
		for (const auto entry : entries) {
			Api().RemoveEntry(entry);
		}
		for (const auto menu : menus) {
			Api().DestroyMenu(menu);
		}
		for (const auto change : changes) {
			Api().RemoveVanillaChange(change);
		}
	}

	void SendSettingChange(const std::string& a_modName, const std::string& a_saveKey, double a_value)
	{
		SendEvent(kSettingChangeEvent, a_modName, a_saveKey, static_cast<float>(a_value));
	}

	void SendAction(const std::string& a_modName, const std::string& a_action)
	{
		SendEvent(kActionEvent, a_modName, a_action);
	}

	bool CallGlobal(const std::string& a_script, const std::string& a_function, const std::string& a_modName, const std::string& a_action)
	{
		const auto vm = VM();
		return vm && vm->DispatchStaticCall(RE::BSFixedString(a_script), RE::BSFixedString(a_function), RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{}, a_modName, a_action);
	}
}
