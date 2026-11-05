#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include "MenuHelper/MenuHelper.h"

namespace
{
	constexpr const char* kModName = "MenuHelperExample";

	const MenuHelper_API*           g_api{ nullptr };
	MenuHelper_ItemId               g_checkbox{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId               g_stepper{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId               g_slider{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId               g_hideCreations{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId               g_loadOffset{ MENUHELPER_INVALID_ID };
	std::vector<MenuHelper_ChangeId> g_hideChanges;
	std::vector<MenuHelper_ChangeId> g_moveChanges;
	std::vector<MenuHelper_EntryId>  g_extraEntries;
	int                             g_presses{ 0 };

	const char* const kDifficulties[] = { "EASY", "NORMAL", "HARD" };
	const char* const kLoadOffsets[] = { "DEFAULT", "1 ROW DOWN", "2 ROWS DOWN" };

	std::string Format(const char* a_text, std::string_view a_name, std::string_view a_value)
	{
		std::string result = g_api->Translate(kModName, a_text);
		const auto  placeholder = std::format("{{{}}}", a_name);
		if (const auto at = result.find(placeholder); at != std::string::npos) {
			result.replace(at, placeholder.size(), a_value);
		}
		return result;
	}

	void OnMenuOpen(MenuHelper_MenuId a_menu, void*) { REX::INFO("menu {} opened", a_menu); }
	void OnMenuClose(MenuHelper_MenuId a_menu, void*) { REX::INFO("menu {} closed", a_menu); }
	void OnValue(MenuHelper_ItemId a_item, float a_value, void*) { REX::INFO("item {} changed to {}", a_item, a_value); }

	void ChangeVanilla(std::vector<MenuHelper_ChangeId>& a_changes, std::int32_t a_main, std::int32_t a_pause, bool a_hidden, std::int32_t a_move)
	{
		for (const auto change : a_changes) {
			g_api->RemoveVanillaChange(change);
		}
		a_changes.clear();
		if (!a_hidden && a_move == 0) {
			return;
		}
		for (const auto [menu, entry] : { std::pair{ MENUHELPER_MENU_MAIN, a_main }, std::pair{ MENUHELPER_MENU_PAUSE, a_pause } }) {
			MenuHelper_VanillaChangeDesc desc{};
			desc.structSize = sizeof(desc);
			desc.menus = menu;
			desc.entry = entry;
			desc.hidden = a_hidden;
			desc.move = a_move;
			a_changes.push_back(g_api->ChangeVanillaEntry(&desc));
		}
	}

	void ApplyHideCreations()
	{
		const bool hidden = g_api->GetItemValue(g_hideCreations) != 0.0F;
		ChangeVanilla(g_hideChanges, MENUHELPER_VANILLA_CREATIONCLUB, MENUHELPER_VANILLA_BETHESDANET, hidden, 0);
	}

	void ApplyLoadOffset()
	{
		const auto rows = static_cast<std::int32_t>(g_api->GetItemValue(g_loadOffset));
		ChangeVanilla(g_moveChanges, MENUHELPER_VANILLA_LOAD, MENUHELPER_VANILLA_LOAD, false, rows);
	}

	void OnHideCreations(MenuHelper_ItemId, float, void*) { ApplyHideCreations(); }
	void OnLoadOffset(MenuHelper_ItemId, float, void*) { ApplyLoadOffset(); }

	void OnCounter(MenuHelper_ItemId a_item, float, void*)
	{
		const auto label = Format("PRESSED {count}", "count", std::to_string(++g_presses));
		g_api->SetItemLabel(a_item, label.c_str());
		REX::INFO("counter pressed {} times", g_presses);
	}

	void OnReset(MenuHelper_ItemId, float, void*)
	{
		g_api->SetItemValue(g_checkbox, 1.0F);
		g_api->SetItemValue(g_stepper, 1.0F);
		g_api->SetItemValue(g_slider, 50.0F);
		g_api->SetItemValue(g_hideCreations, 0.0F);
		g_api->SetItemValue(g_loadOffset, 0.0F);
		ApplyHideCreations();
		ApplyLoadOffset();
		REX::INFO("options reset");
	}

	void OnEntryPress(MenuHelper_EntryId a_entry, std::uint32_t a_menu, void*)
	{
		REX::INFO("entry {} pressed in {} menu", a_entry, a_menu == MENUHELPER_MENU_PAUSE ? "pause" : "main");
	}

	void OnAddEntry(MenuHelper_ItemId, float, void*)
	{
		const auto label = Format("EXTRA ENTRY {number}", "number", std::to_string(g_extraEntries.size() + 1));
		MenuHelper_EntryDesc entry{};
		entry.structSize = sizeof(entry);
		entry.modName = kModName;
		entry.label = label.c_str();
		entry.menus = MENUHELPER_MENU_ALL;
		entry.insertBefore = MENUHELPER_VANILLA_QUIT;
		entry.onPress = OnEntryPress;
		g_extraEntries.push_back(g_api->AddEntry(&entry));
	}

	void OnRemoveEntries(MenuHelper_ItemId, float, void*)
	{
		for (const auto entry : g_extraEntries) {
			g_api->RemoveEntry(entry);
		}
		g_extraEntries.clear();
	}

	MenuHelper_ItemDesc ItemDesc(std::uint32_t a_type, const char* a_label, MenuHelper_ItemCallback a_onChange = OnValue)
	{
		MenuHelper_ItemDesc desc{};
		desc.structSize = sizeof(desc);
		desc.type = a_type;
		desc.label = a_label;
		desc.onChange = a_onChange;
		return desc;
	}

	MenuHelper_MenuId CreateMenu()
	{
		MenuHelper_MenuDesc desc{};
		desc.structSize = sizeof(desc);
		desc.modName = kModName;
		desc.onOpen = OnMenuOpen;
		desc.onClose = OnMenuClose;
		return g_api->CreateMenu(&desc);
	}

	MenuHelper_MenuId CreateOptionsMenu()
	{
		const auto menu = CreateMenu();

		auto checkbox = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "SHOW MARKERS");
		checkbox.description = "An example checkbox. Its value is saved.";
		checkbox.value = 1.0F;
		checkbox.saveKey = "bShowMarkers";
		g_checkbox = g_api->AddItem(menu, &checkbox);

		auto stepper = ItemDesc(MENUHELPER_ITEM_STEPPER, "DIFFICULTY");
		stepper.description = "An example stepper. Its value is saved.";
		stepper.options = kDifficulties;
		stepper.optionCount = static_cast<std::uint32_t>(std::size(kDifficulties));
		stepper.value = 1.0F;
		stepper.saveKey = "iDifficulty";
		g_stepper = g_api->AddItem(menu, &stepper);

		auto slider = ItemDesc(MENUHELPER_ITEM_SLIDER, "VOLUME");
		slider.description = "An example slider from 0 to 100 in steps of 5. Its value is saved.";
		slider.min = 0.0F;
		slider.max = 100.0F;
		slider.step = 5.0F;
		slider.value = 50.0F;
		slider.saveKey = "fVolume";
		g_slider = g_api->AddItem(menu, &slider);

		auto hideCreations = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "HIDE CREATIONS", OnHideCreations);
		hideCreations.description = "Hides the vanilla CREATIONS entry.";
		hideCreations.saveKey = "bHideCreations";
		g_hideCreations = g_api->AddItem(menu, &hideCreations);

		auto loadOffset = ItemDesc(MENUHELPER_ITEM_STEPPER, "LOAD POSITION", OnLoadOffset);
		loadOffset.description = "Moves the vanilla LOAD entry down.";
		loadOffset.options = kLoadOffsets;
		loadOffset.optionCount = static_cast<std::uint32_t>(std::size(kLoadOffsets));
		loadOffset.saveKey = "iLoadOffset";
		g_loadOffset = g_api->AddItem(menu, &loadOffset);

		auto reset = ItemDesc(MENUHELPER_ITEM_BUTTON, "RESET TO DEFAULTS", OnReset);
		reset.description = "Buttons can live on options pages too.";
		g_api->AddItem(menu, &reset);

		ApplyHideCreations();
		ApplyLoadOffset();
		return menu;
	}

	void Register()
	{
		const auto options = CreateOptionsMenu();
		const auto root = CreateMenu();

		auto openOptions = ItemDesc(MENUHELPER_ITEM_BUTTON, "OPTIONS");
		openOptions.submenu = options;
		g_api->AddItem(root, &openOptions);

		const auto counter = ItemDesc(MENUHELPER_ITEM_BUTTON, "PRESS ME", OnCounter);
		g_api->AddItem(root, &counter);

		const auto addEntry = ItemDesc(MENUHELPER_ITEM_BUTTON, "ADD ENTRY", OnAddEntry);
		g_api->AddItem(root, &addEntry);

		const auto removeEntries = ItemDesc(MENUHELPER_ITEM_BUTTON, "REMOVE ADDED ENTRIES", OnRemoveEntries);
		g_api->AddItem(root, &removeEntries);

		const auto unavailable = ItemDesc(MENUHELPER_ITEM_BUTTON, "UNAVAILABLE");
		g_api->SetItemEnabled(g_api->AddItem(root, &unavailable), 0);

		MenuHelper_EntryDesc entry{};
		entry.structSize = sizeof(entry);
		entry.modName = kModName;
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
