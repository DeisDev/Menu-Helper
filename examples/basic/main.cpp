#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include "MenuHelper/MenuHelper.h"

namespace
{
	constexpr const char* kModName = "MenuHelperExample";

	const MenuHelper_API*            g_api{ nullptr };
	MenuHelper_ItemId                g_hideCreations{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId                g_loadOffset{ MENUHELPER_INVALID_ID };
	MenuHelper_ItemId                g_extra{ MENUHELPER_INVALID_ID };
	std::vector<MenuHelper_ChangeId> g_hideChanges;
	std::vector<MenuHelper_ChangeId> g_moveChanges;
	std::vector<MenuHelper_EntryId>  g_extraEntries;
	int                              g_presses{ 0 };
	std::size_t                      g_boxes{ 0 };

	const char* const kDifficulties[] = { "EASY", "NORMAL", "HARD" };
	const char* const kLoadOffsets[] = { "DEFAULT", "1 ROW DOWN", "2 ROWS DOWN" };
	const char* const kQualities[] = { "LOW", "HIGH" };
	const char* const kModes[] = { "OFF", "SLOW", "NORMAL", "FAST", "INSTANT" };
	const char* const kMonths[] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
	const char* const kBoxButtons[] = { "YES", "NO", "MAYBE", "LATER" };
	constexpr std::uint32_t kBoxButtonCounts[] = { 1, 2, 4 };

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
	void OnValue(MenuHelper_ItemId a_item, double a_value, void*) { REX::INFO("item {} changed to {}", a_item, a_value); }
	void OnSelect(MenuHelper_ItemId a_item, double, void*) { REX::INFO("item {} selected", a_item); }

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

	void ApplyVanillaChanges()
	{
		const bool hidden = g_api->GetItemValue(g_hideCreations) != 0.0;
		ChangeVanilla(g_hideChanges, MENUHELPER_VANILLA_CREATIONCLUB, MENUHELPER_VANILLA_BETHESDANET, hidden, 0);
		const auto rows = static_cast<std::int32_t>(g_api->GetItemValue(g_loadOffset));
		ChangeVanilla(g_moveChanges, MENUHELPER_VANILLA_LOAD, MENUHELPER_VANILLA_LOAD, false, rows);
	}

	void OnVanillaOption(MenuHelper_ItemId, double, void*) { ApplyVanillaChanges(); }

	void OnShowExtra(MenuHelper_ItemId, double a_value, void*) { g_api->SetItemVisible(g_extra, a_value != 0.0); }

	void OnReset(MenuHelper_MenuId, void*)
	{
		ApplyVanillaChanges();
		g_api->SetItemVisible(g_extra, 0);
		REX::INFO("options reset to defaults");
	}

	void OnHotkey(MenuHelper_ItemId a_item, double a_value, void*)
	{
		const auto text = Format("HOTKEY {key} PRESSED", "key", g_api->GetKeyName(static_cast<std::uint32_t>(a_value)));
		g_api->ShowNotification(kModName, text.c_str(), "UIMenuOK", 0);
		REX::INFO("hotkey item {} pressed", a_item);
	}

	void OnConfirmResult(MenuHelper_BoxId a_confirm, int a_result, void*) { REX::INFO("confirm {} answered {}", a_confirm, a_result); }

	void OnConfirmTest(MenuHelper_ItemId, double, void*)
	{
		MenuHelper_ConfirmDesc desc{};
		desc.structSize = sizeof(desc);
		desc.modName = kModName;
		desc.text = "Do you really want to test the confirm?";
		desc.onResult = OnConfirmResult;
		g_api->ShowConfirm(&desc);
	}

	void OnBoxButton(MenuHelper_BoxId a_box, std::uint32_t a_button, void*) { REX::INFO("message box {} button {}", a_box, a_button); }

	void OnMessageBox(MenuHelper_ItemId, double, void*)
	{
		MenuHelper_MessageBoxDesc desc{};
		desc.structSize = sizeof(desc);
		desc.modName = kModName;
		desc.body = "This is a vanilla message box with 1, 2 or 4 buttons. Do you like it?";
		desc.buttons = kBoxButtons;
		desc.buttonCount = kBoxButtonCounts[g_boxes++ % std::size(kBoxButtonCounts)];
		desc.onButton = OnBoxButton;
		g_api->ShowMessageBox(&desc);
	}

	void OnCounter(MenuHelper_ItemId a_item, double, void*)
	{
		const auto label = Format("PRESSED {count}", "count", std::to_string(++g_presses));
		g_api->SetItemLabel(a_item, label.c_str());
		REX::INFO("counter pressed {} times", g_presses);
	}

	void OnEntryPress(MenuHelper_EntryId a_entry, std::uint32_t a_menu, void*)
	{
		REX::INFO("entry {} pressed in {} menu", a_entry, a_menu == MENUHELPER_MENU_PAUSE ? "pause" : "main");
	}

	void OnAddEntry(MenuHelper_ItemId, double, void*)
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

	void OnRemoveEntries(MenuHelper_ItemId, double, void*)
	{
		for (const auto entry : g_extraEntries) {
			g_api->RemoveEntry(entry);
		}
		g_extraEntries.clear();
	}

	void OnSetting(const char* a_modName, const char* a_saveKey, double a_value, void*)
	{
		REX::INFO("setting {}:{} is now {}", a_modName, a_saveKey, a_value);
	}

	void OnAction(const char* a_modName, const char* a_action, void*)
	{
		REX::INFO("action {} of {}", a_action, a_modName);
	}

	MenuHelper_ItemDesc ItemDesc(std::uint32_t a_type, const char* a_label, MenuHelper_ItemCallback a_onChange = OnValue)
	{
		MenuHelper_ItemDesc desc{};
		desc.structSize = sizeof(desc);
		desc.type = a_type;
		desc.label = a_label;
		desc.onChange = a_onChange;
		desc.onSelect = OnSelect;
		return desc;
	}

	MenuHelper_MenuId CreateMenu(std::uint32_t a_kind = MENUHELPER_MENU_ITEMS, MenuHelper_MenuCallback a_onReset = nullptr)
	{
		MenuHelper_MenuDesc desc{};
		desc.structSize = sizeof(desc);
		desc.modName = kModName;
		desc.kind = a_kind;
		desc.onOpen = OnMenuOpen;
		desc.onClose = OnMenuClose;
		desc.onReset = a_onReset;
		return g_api->CreateMenu(&desc);
	}

	void AddOptions(MenuHelper_MenuId a_menu, const char* a_label, std::uint32_t a_type, const char* const* a_options, std::uint32_t a_count, const char* a_saveKey)
	{
		auto desc = ItemDesc(a_type, a_label);
		desc.options = a_options;
		desc.optionCount = a_count;
		desc.saveKey = a_saveKey;
		g_api->AddItem(a_menu, &desc);
	}

	void AddHeader(MenuHelper_MenuId a_menu, const char* a_label)
	{
		const auto desc = ItemDesc(MENUHELPER_ITEM_HEADER, a_label, nullptr);
		g_api->AddItem(a_menu, &desc);
	}

	MenuHelper_MenuId CreateOptionsMenu()
	{
		const auto menu = CreateMenu(MENUHELPER_MENU_ITEMS, OnReset);

		AddHeader(menu, "GENERAL");

		auto checkbox = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "SHOW MARKERS");
		checkbox.description = "An example checkbox. Its value is saved.";
		checkbox.value = 1.0;
		checkbox.saveKey = "bShowMarkers";
		g_api->AddItem(menu, &checkbox);

		auto stepper = ItemDesc(MENUHELPER_ITEM_STEPPER, "DIFFICULTY");
		stepper.description = "An example stepper. Its value is saved.";
		stepper.options = kDifficulties;
		stepper.optionCount = static_cast<std::uint32_t>(std::size(kDifficulties));
		stepper.value = 1.0;
		stepper.saveKey = "iDifficulty";
		g_api->AddItem(menu, &stepper);

		auto volume = ItemDesc(MENUHELPER_ITEM_SLIDER, "VOLUME");
		volume.description = "A slider from 0 to 100 in steps of 5, shown in percent.";
		volume.min = 0.0;
		volume.max = 100.0;
		volume.step = 5.0;
		volume.value = 50.0;
		volume.format = "{value} %";
		volume.saveKey = "fVolume";
		g_api->AddItem(menu, &volume);

		auto speed = ItemDesc(MENUHELPER_ITEM_SLIDER, "WALKING SPEED MULTIPLIER");
		speed.description = "A slider from 0.5 to 2 in steps of 0.05.";
		speed.min = 0.5;
		speed.max = 2.0;
		speed.step = 0.05;
		speed.value = 1.0;
		speed.format = "{value}x";
		speed.saveKey = "fSpeed";
		g_api->AddItem(menu, &speed);

		AddOptions(menu, "QUALITY", MENUHELPER_ITEM_DROPDOWN, kQualities, static_cast<std::uint32_t>(std::size(kQualities)), "iQuality");
		AddOptions(menu, "MODE", MENUHELPER_ITEM_DROPDOWN, kModes, static_cast<std::uint32_t>(std::size(kModes)), "iMode");
		AddOptions(menu, "MONTH", MENUHELPER_ITEM_DROPDOWN, kMonths, static_cast<std::uint32_t>(std::size(kMonths)), "iMonth");

		AddHeader(menu, "CONTROLS");

		auto toggle = ItemDesc(MENUHELPER_ITEM_HOTKEY, "NOTIFY KEY");
		toggle.description = "Shows a notification in game. Shift, Ctrl, Alt and mouse buttons are allowed.";
		toggle.flags = MENUHELPER_FLAG_HOTKEY_MODIFIERS | MENUHELPER_FLAG_HOTKEY_MOUSE;
		toggle.value = 'N';
		toggle.saveKey = "iNotifyKey";
		toggle.onHotkey = OnHotkey;
		g_api->AddItem(menu, &toggle);

		auto plain = ItemDesc(MENUHELPER_ITEM_HOTKEY, "PLAIN KEY");
		plain.description = "A keyboard key without modifiers.";
		plain.saveKey = "iPlainKey";
		plain.onHotkey = OnHotkey;
		g_api->AddItem(menu, &plain);

		AddHeader(menu, "LOOK");

		auto tint = ItemDesc(MENUHELPER_ITEM_COLOR, "TINT");
		tint.description = "A color without alpha.";
		tint.value = 0xFFFF8000;
		tint.saveKey = "uTint";
		g_api->AddItem(menu, &tint);

		auto overlay = ItemDesc(MENUHELPER_ITEM_COLOR, "OVERLAY");
		overlay.description = "A color with alpha.";
		overlay.flags = MENUHELPER_FLAG_HASALPHA;
		overlay.value = 0x8000A0FF;
		overlay.saveKey = "uOverlay";
		g_api->AddItem(menu, &overlay);

		auto info = ItemDesc(MENUHELPER_ITEM_TEXT, "ABOUT THESE OPTIONS", nullptr);
		info.description = "Text rows show a description and do nothing when pressed.";
		g_api->AddItem(menu, &info);

		AddHeader(menu, "VANILLA ENTRIES");

		auto hideCreations = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "HIDE CREATIONS", OnVanillaOption);
		hideCreations.description = "Hides the vanilla CREATIONS entry.";
		hideCreations.saveKey = "bHideCreations";
		g_hideCreations = g_api->AddItem(menu, &hideCreations);

		auto loadOffset = ItemDesc(MENUHELPER_ITEM_STEPPER, "LOAD POSITION", OnVanillaOption);
		loadOffset.description = "Moves the vanilla LOAD entry down.";
		loadOffset.options = kLoadOffsets;
		loadOffset.optionCount = static_cast<std::uint32_t>(std::size(kLoadOffsets));
		loadOffset.saveKey = "iLoadOffset";
		g_loadOffset = g_api->AddItem(menu, &loadOffset);

		AddHeader(menu, "OTHER");

		auto showExtra = ItemDesc(MENUHELPER_ITEM_CHECKBOX, "SHOW EXTRA ROW", OnShowExtra);
		showExtra.description = "Shows or hides the next row.";
		g_api->AddItem(menu, &showExtra);

		auto extra = ItemDesc(MENUHELPER_ITEM_BUTTON, "EXTRA ROW");
		extra.description = "Only visible while SHOW EXTRA ROW is checked.";
		extra.flags = MENUHELPER_FLAG_HIDDEN;
		g_extra = g_api->AddItem(menu, &extra);

		auto confirm = ItemDesc(MENUHELPER_ITEM_BUTTON, "ASK ME", OnConfirmTest);
		confirm.description = "Shows a confirm over this page.";
		g_api->AddItem(menu, &confirm);

		ApplyVanillaChanges();
		return menu;
	}

	MenuHelper_MenuId CreatePagesMenu(bool a_single)
	{
		const auto menu = CreateMenu(MENUHELPER_MENU_PAGES);
		auto       topic = ItemDesc(MENUHELPER_ITEM_TOPIC, "ABOUT", nullptr);
		topic.description =
			"<b>MenuHelper Example</b><br><br>This page is a MENUHELPER_MENU_PAGES menu with one topic, so it shows without a topic list."
			"<br><br>Text can be <b>bold</b> or <i>italic</i>.";
		topic.image = "Textures/MenuHelperExample/test.dds";
		topic.imageWidth = 256;
		topic.imageHeight = 64;
		g_api->AddItem(menu, &topic);
		if (a_single) {
			return menu;
		}
		auto second = ItemDesc(MENUHELPER_ITEM_TOPIC, "LONG TEXT", nullptr);
		std::string text = "A long page to try scrolling with the keys and the mouse wheel.<br>";
		for (int i = 1; i <= 40; ++i) {
			text += std::format("<br>Line {}", i);
		}
		second.description = text.c_str();
		g_api->AddItem(menu, &second);
		auto missing = ItemDesc(MENUHELPER_ITEM_TOPIC, "MISSING IMAGE", nullptr);
		missing.description = "This topic names an image file that does not exist. The page shows without it.";
		missing.image = "Textures/MenuHelperExample/missing.dds";
		g_api->AddItem(menu, &missing);
		for (int i = 4; i <= 20; ++i) {
			const auto title = std::format("TOPIC {}", i);
			const auto body = std::format("Page {} of the topic list.", i);
			auto       filler = ItemDesc(MENUHELPER_ITEM_TOPIC, title.c_str(), nullptr);
			filler.description = body.c_str();
			g_api->AddItem(menu, &filler);
		}
		return menu;
	}

	void Register()
	{
		const auto options = CreateOptionsMenu();
		const auto root = CreateMenu();

		auto openOptions = ItemDesc(MENUHELPER_ITEM_BUTTON, "OPTIONS");
		openOptions.submenu = options;
		g_api->AddItem(root, &openOptions);

		auto about = ItemDesc(MENUHELPER_ITEM_BUTTON, "ABOUT");
		about.submenu = CreatePagesMenu(true);
		g_api->AddItem(root, &about);

		auto guide = ItemDesc(MENUHELPER_ITEM_BUTTON, "GUIDE");
		guide.submenu = CreatePagesMenu(false);
		g_api->AddItem(root, &guide);

		const auto counter = ItemDesc(MENUHELPER_ITEM_BUTTON, "PRESS ME", OnCounter);
		g_api->AddItem(root, &counter);

		const auto messageBox = ItemDesc(MENUHELPER_ITEM_BUTTON, "MESSAGE BOX", OnMessageBox);
		g_api->AddItem(root, &messageBox);

		const auto addEntry = ItemDesc(MENUHELPER_ITEM_BUTTON, "ADD ENTRY", OnAddEntry);
		g_api->AddItem(root, &addEntry);

		const auto removeEntries = ItemDesc(MENUHELPER_ITEM_BUTTON, "REMOVE ADDED ENTRIES", OnRemoveEntries);
		g_api->AddItem(root, &removeEntries);

		const auto unavailable = ItemDesc(MENUHELPER_ITEM_BUTTON, "UNAVAILABLE");
		g_api->SetItemEnabled(g_api->AddItem(root, &unavailable), 0);

		g_api->RegisterSettingListener(kModName, OnSetting, nullptr);
		g_api->RegisterActionListener(kModName, OnAction, nullptr);

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
