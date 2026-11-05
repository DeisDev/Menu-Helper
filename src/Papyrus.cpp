#include "Papyrus.h"

#include "API.h"

namespace MenuHelper::Papyrus
{
	namespace
	{
		constexpr auto kScript = "MenuHelper"sv;
		constexpr auto kEntryPressEvent = "MenuHelper_OnEntryPress"sv;
		constexpr auto kItemChangeEvent = "MenuHelper_OnItemChange"sv;
		constexpr auto kMenuOpenEvent = "MenuHelper_OnMenuOpen"sv;
		constexpr auto kMenuCloseEvent = "MenuHelper_OnMenuClose"sv;

		std::mutex                     g_lock;
		std::vector<MenuHelper_EntryId> g_entries;
		std::vector<MenuHelper_MenuId>  g_menus;

		const MenuHelper_API& Api()
		{
			static const auto api = MenuHelper_GetAPI(MENUHELPER_API_VERSION);
			return *api;
		}

		template <class... Args>
		void SendEvent(std::string_view a_event, Args... a_args)
		{
			const auto game = RE::GameVM::GetSingleton();
			const auto vm = game ? game->GetVM() : nullptr;
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

		void OnEntryPress(MenuHelper_EntryId a_entry, std::uint32_t, void*) { SendEvent(kEntryPressEvent, static_cast<std::int32_t>(a_entry)); }
		void OnItemChange(MenuHelper_ItemId a_item, float a_value, void*) { SendEvent(kItemChangeEvent, static_cast<std::int32_t>(a_item), a_value); }
		void OnMenuOpen(MenuHelper_MenuId a_menu, void*) { SendEvent(kMenuOpenEvent, static_cast<std::int32_t>(a_menu)); }
		void OnMenuClose(MenuHelper_MenuId a_menu, void*) { SendEvent(kMenuCloseEvent, static_cast<std::int32_t>(a_menu)); }

		std::uint32_t ToId(std::int32_t a_id)
		{
			return a_id > 0 ? static_cast<std::uint32_t>(a_id) : MENUHELPER_INVALID_ID;
		}

		std::int32_t AddEntry(std::monostate, std::string_view a_label, std::int32_t a_submenu, std::int32_t a_insertBefore)
		{
			const std::string    label{ a_label };
			MenuHelper_EntryDesc desc{};
			desc.structSize = sizeof(desc);
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
			return static_cast<std::int32_t>(id);
		}

		bool RemoveEntry(std::monostate, std::int32_t a_entry) { return Api().RemoveEntry(ToId(a_entry)) != 0; }

		bool SetEntryLabel(std::monostate, std::int32_t a_entry, std::string_view a_label)
		{
			return Api().SetEntryLabel(ToId(a_entry), std::string{ a_label }.c_str()) != 0;
		}

		bool SetEntryEnabled(std::monostate, std::int32_t a_entry, bool a_enabled) { return Api().SetEntryEnabled(ToId(a_entry), a_enabled) != 0; }

		std::int32_t CreateMenu(std::monostate)
		{
			MenuHelper_MenuDesc desc{};
			desc.structSize = sizeof(desc);
			desc.onOpen = OnMenuOpen;
			desc.onClose = OnMenuClose;
			const auto id = Api().CreateMenu(&desc);
			if (id != MENUHELPER_INVALID_ID) {
				const std::scoped_lock l{ g_lock };
				g_menus.push_back(id);
			}
			return static_cast<std::int32_t>(id);
		}

		bool DestroyMenu(std::monostate, std::int32_t a_menu) { return Api().DestroyMenu(ToId(a_menu)) != 0; }

		std::int32_t AddItem(std::int32_t a_menu, MenuHelper_ItemDesc& a_desc, std::string_view a_label, std::string_view a_description)
		{
			const std::string label{ a_label };
			const std::string description{ a_description };
			a_desc.structSize = sizeof(a_desc);
			a_desc.label = label.c_str();
			a_desc.description = description.c_str();
			a_desc.onChange = OnItemChange;
			return static_cast<std::int32_t>(Api().AddItem(ToId(a_menu), &a_desc));
		}

		std::int32_t AddButton(std::monostate, std::int32_t a_menu, std::string_view a_label, std::string_view a_description, std::int32_t a_submenu)
		{
			MenuHelper_ItemDesc desc{};
			desc.type = MENUHELPER_ITEM_BUTTON;
			desc.submenu = ToId(a_submenu);
			return AddItem(a_menu, desc, a_label, a_description);
		}

		std::int32_t AddCheckbox(std::monostate, std::int32_t a_menu, std::string_view a_label, bool a_checked, std::string_view a_description)
		{
			MenuHelper_ItemDesc desc{};
			desc.type = MENUHELPER_ITEM_CHECKBOX;
			desc.value = a_checked ? 1.0F : 0.0F;
			return AddItem(a_menu, desc, a_label, a_description);
		}

		std::int32_t AddStepper(std::monostate, std::int32_t a_menu, std::string_view a_label, std::vector<std::string> a_options, std::int32_t a_index, std::string_view a_description)
		{
			std::vector<const char*> options;
			std::ranges::transform(a_options, std::back_inserter(options), &std::string::c_str);
			MenuHelper_ItemDesc desc{};
			desc.type = MENUHELPER_ITEM_STEPPER;
			desc.options = options.data();
			desc.optionCount = static_cast<std::uint32_t>(options.size());
			desc.value = static_cast<float>(a_index);
			return AddItem(a_menu, desc, a_label, a_description);
		}

		std::int32_t AddSlider(std::monostate, std::int32_t a_menu, std::string_view a_label, float a_value, float a_min, float a_max, float a_step, std::string_view a_description)
		{
			MenuHelper_ItemDesc desc{};
			desc.type = MENUHELPER_ITEM_SLIDER;
			desc.value = a_value;
			desc.min = a_min;
			desc.max = a_max;
			desc.step = a_step;
			return AddItem(a_menu, desc, a_label, a_description);
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

		bool  SetItemEnabled(std::monostate, std::int32_t a_item, bool a_enabled) { return Api().SetItemEnabled(ToId(a_item), a_enabled) != 0; }
		bool  SetItemValue(std::monostate, std::int32_t a_item, float a_value) { return Api().SetItemValue(ToId(a_item), a_value) != 0; }
		float GetItemValue(std::monostate, std::int32_t a_item) { return Api().GetItemValue(ToId(a_item)); }
		bool  OpenMenu(std::monostate, std::int32_t a_menu) { return Api().OpenMenu(ToId(a_menu)) != 0; }
		bool  CloseMenu(std::monostate, std::int32_t a_menu) { return Api().CloseMenu(ToId(a_menu)) != 0; }
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->BindNativeMethod(kScript, "AddEntry"sv, AddEntry, true);
		a_vm->BindNativeMethod(kScript, "RemoveEntry"sv, RemoveEntry, true);
		a_vm->BindNativeMethod(kScript, "SetEntryLabel"sv, SetEntryLabel, true);
		a_vm->BindNativeMethod(kScript, "SetEntryEnabled"sv, SetEntryEnabled, true);
		a_vm->BindNativeMethod(kScript, "CreateMenu"sv, CreateMenu, true);
		a_vm->BindNativeMethod(kScript, "DestroyMenu"sv, DestroyMenu, true);
		a_vm->BindNativeMethod(kScript, "AddButton"sv, AddButton, true);
		a_vm->BindNativeMethod(kScript, "AddCheckbox"sv, AddCheckbox, true);
		a_vm->BindNativeMethod(kScript, "AddStepper"sv, AddStepper, true);
		a_vm->BindNativeMethod(kScript, "AddSlider"sv, AddSlider, true);
		a_vm->BindNativeMethod(kScript, "RemoveItem"sv, RemoveItem, true);
		a_vm->BindNativeMethod(kScript, "SetItemLabel"sv, SetItemLabel, true);
		a_vm->BindNativeMethod(kScript, "SetItemDescription"sv, SetItemDescription, true);
		a_vm->BindNativeMethod(kScript, "SetItemEnabled"sv, SetItemEnabled, true);
		a_vm->BindNativeMethod(kScript, "SetItemValue"sv, SetItemValue, true);
		a_vm->BindNativeMethod(kScript, "GetItemValue"sv, GetItemValue, true);
		a_vm->BindNativeMethod(kScript, "OpenMenu"sv, OpenMenu, true);
		a_vm->BindNativeMethod(kScript, "CloseMenu"sv, CloseMenu, true);
		return true;
	}

	void ClearRegistrations()
	{
		std::vector<MenuHelper_EntryId> entries;
		std::vector<MenuHelper_MenuId>  menus;
		{
			const std::scoped_lock l{ g_lock };
			entries = std::exchange(g_entries, {});
			menus = std::exchange(g_menus, {});
		}
		for (const auto entry : entries) {
			Api().RemoveEntry(entry);
		}
		for (const auto menu : menus) {
			Api().DestroyMenu(menu);
		}
	}
}
