#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include "MenuHelper/MenuHelper.h"

namespace
{
	const MenuHelper_API* g_api{ nullptr };
	MenuHelper_ItemId     g_checkbox{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId     g_stepper{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId     g_slider{ MENUHELPER_INVALID_ID };
	int                   g_presses{ 0 };

	const char* const kDifficulties[] = { "EASY", "NORMAL", "HARD" };

	void OnMenuOpen(MenuHelper_MenuId a_menu, void*) { REX::INFO("menu {} opened", a_menu); }
	void OnMenuClose(MenuHelper_MenuId a_menu, void*) { REX::INFO("menu {} closed", a_menu); }
	void OnValue(MenuHelper_ItemId a_item, float a_value, void*) { REX::INFO("item {} changed to {}", a_item, a_value); }

	void OnCounter(MenuHelper_ItemId a_item, float, void*)
	{
		const auto label = std::format("PRESSED {}", ++g_presses);
		g_api->SetItemLabel(a_item, label.c_str());
		REX::INFO("counter pressed {} times", g_presses);
	}

	void OnReset(MenuHelper_ItemId, float, void*)
	{
		g_api->SetItemValue(g_checkbox, 1.0F);
		g_api->SetItemValue(g_stepper, 1.0F);
		g_api->SetItemValue(g_slider, 50.0F);
		REX::INFO("options reset");
	}

	void OnEntryPress(MenuHelper_EntryId a_entry, std::uint32_t a_menu, void*)
	{
		REX::INFO("entry {} pressed in {} menu", a_entry, a_menu == MENUHELPER_MENU_PAUSE ? "pause" : "main");
	}

	MenuHelper_ItemDesc ItemDesc(std::uint32_t a_type, const char* a_label)
	{
		MenuHelper_ItemDesc desc{};
		desc.structSize = sizeof(desc);
		desc.type = a_type;
		desc.label = a_label;
		desc.onChange = OnValue;
		return desc;
	}

	MenuHelper_MenuId CreateMenu()
	{
		MenuHelper_MenuDesc desc{};
		desc.structSize = sizeof(desc);
		desc.onOpen = OnMenuOpen;
		desc.onClose = OnMenuClose;
		return g_api->CreateMenu(&desc);
	}

	MenuHelper_MenuId CreateOptionsMenu()
	{
		const auto menu = CreateMenu();

		auto checkbox = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "SHOW MARKERS");
		checkbox.description = "An example checkbox.";
		checkbox.value = 1.0F;
		g_checkbox = g_api->AddItem(menu, &checkbox);

		auto stepper = ItemDesc(MENUHELPER_ITEM_STEPPER, "DIFFICULTY");
		stepper.description = "An example stepper.";
		stepper.options = kDifficulties;
		stepper.optionCount = static_cast<std::uint32_t>(std::size(kDifficulties));
		stepper.value = 1.0F;
		g_stepper = g_api->AddItem(menu, &stepper);

		auto slider = ItemDesc(MENUHELPER_ITEM_SLIDER, "VOLUME");
		slider.description = "An example slider from 0 to 100 in steps of 5.";
		slider.min = 0.0F;
		slider.max = 100.0F;
		slider.step = 5.0F;
		slider.value = 50.0F;
		g_slider = g_api->AddItem(menu, &slider);

		auto reset = ItemDesc(MENUHELPER_ITEM_BUTTON, "RESET TO DEFAULTS");
		reset.description = "Buttons can live on options pages too.";
		reset.onChange = OnReset;
		g_api->AddItem(menu, &reset);

		return menu;
	}

	void Register()
	{
		const auto options = CreateOptionsMenu();
		const auto root = CreateMenu();

		auto openOptions = ItemDesc(MENUHELPER_ITEM_BUTTON, "OPTIONS");
		openOptions.submenu = options;
		g_api->AddItem(root, &openOptions);

		auto counter = ItemDesc(MENUHELPER_ITEM_BUTTON, "PRESS ME");
		counter.onChange = OnCounter;
		g_api->AddItem(root, &counter);

		const auto unavailable = ItemDesc(MENUHELPER_ITEM_BUTTON, "UNAVAILABLE");
		g_api->SetItemEnabled(g_api->AddItem(root, &unavailable), 0);

		MenuHelper_EntryDesc entry{};
		entry.structSize = sizeof(entry);
		entry.label = "MENU HELPER";
		entry.menus = MENUHELPER_MENU_ALL;
		entry.insertBefore = MENUHELPER_VANILLA_QUIT;
		entry.onPress = OnEntryPress;
		entry.submenu = root;
		REX::INFO("registered entry {}", g_api->AddEntry(&entry));
	}

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type != F4SE::MessagingInterface::kPostLoad) {
			return;
		}
		const auto module = REX::W32::GetModuleHandleW(L"MenuHelper.dll");
		const auto getApi = module ? reinterpret_cast<MenuHelper_GetAPI_t>(REX::W32::GetProcAddress(module, "MenuHelper_GetAPI")) : nullptr;
		g_api = getApi ? getApi(MENUHELPER_API_VERSION) : nullptr;
		if (!g_api) {
			REX::ERROR("MenuHelper.dll not loaded or API version mismatch");
			return;
		}
		Register();
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage, "F4SE");
	return true;
}
