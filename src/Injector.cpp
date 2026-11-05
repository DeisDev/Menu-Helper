#include "Injector.h"

#include "Config.h"
#include "GFxUtil.h"
#include "Hotkeys.h"
#include "Registry.h"
#include "Submenus.h"
#include "Translations.h"

namespace MenuHelper::Injector
{
	namespace
	{
		using GFx::Value;

		constexpr std::size_t        kInitMainListVFuncIdx = 0x15;
		constexpr std::size_t        kShouldHandleEventVFuncIdx = 0x01;
		constexpr std::size_t        kOnButtonEventVFuncIdx = 0x08;
		constexpr std::size_t        kInputEventUserOffset = 0x10;
		constexpr double             kInjectedIndexBase = 1000.0;
		constexpr MenuHelper_EntryId kGroupEntryId = MENUHELPER_INVALID_ID;
		constexpr const char*        kModName = "MenuHelper";
		constexpr const char*        kGroupLabel = "MODS";
		constexpr const char*        kEntryIdMember = "menuHelperId";
		constexpr const char*        kVanillaMarkerMember = "menuHelperVanilla";
		constexpr const char*        kVanillaEntriesMember = "MenuHelperVanillaEntries";
		constexpr const char*        kRootMarkerMember = "MenuHelperPressListener";
		constexpr const char*        kItemPressEvent = "BSScrollingList::itemPress";
		constexpr const char*        kMainState = "MAIN_STATE";
		constexpr int                kMaxBackOutSteps = 16;

		struct TrackedRow
		{
			std::int32_t entry;
			const char*  updateFunction;
		};

		// MainMenu.as greys these entries out by row number.
		constexpr std::array kTrackedRows{
			TrackedRow{ MENUHELPER_VANILLA_CREATIONCLUB, "UpdateCClubIndex" },
			TrackedRow{ MENUHELPER_VANILLA_BETHESDANET, "UpdateModsIndex" },
			TrackedRow{ MENUHELPER_VANILLA_INSTALLED_CONTENT, "UpdateInstalledContentIndex" },
		};

		struct Row
		{
			Value  object;
			double index{ 0.0 };
			bool   mod{ false };
		};

		std::array<MenuHelper_MenuId, 2> g_groupMenus{};
		double                           g_nextBaseId{ 1.0 };

		std::size_t HostIndex(std::uint32_t a_hostBit)
		{
			return a_hostBit == MENUHELPER_MENU_PAUSE ? 1 : 0;
		}

		void PlayMenuSound(Value& a_menuObj, const char* a_function)
		{
			Value codeObj;
			if (a_menuObj.GetMember("BGSCodeObj", &codeObj) && codeObj.IsObject()) {
				codeObj.Invoke(a_function);
			}
		}

		// Mirrors MainMenu.onMainListItemPress: leave any vanilla submenu before handling a main list press.
		void BackOutToMainState(Value& a_menuObj)
		{
			for (int i = 0; i < kMaxBackOutSteps; ++i) {
				Value state;
				if (!a_menuObj.GetMember("currentState", &state) || !state.IsString() || std::string_view{ state.GetString() } == kMainState) {
					break;
				}
				a_menuObj.Invoke("onCancelPress", std::array{ Value(true), Value(false) });
			}
			Value holder;
			if (a_menuObj.GetMember("SaveLoadHolder_mc", &holder) && holder.IsObject()) {
				holder.Invoke("gotoAndPlay", std::array{ Value("end") });
			}
			a_menuObj.Invoke("ShowSecondPanelBackground", std::array{ Value(false) });
		}

		class PressHandler : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (a_params.argCount < 1 || !a_params.movie) {
					return;
				}
				const auto hostBit = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(a_params.userData));
				Value&     event = a_params.args[0];
				Value      target, entry, idValue, menuObj;
				if (!event.IsObject() || !event.GetMember("target", &target) || !target.IsObject() ||
					!target.GetMember("selectedEntry", &entry) || !entry.IsObject() ||
					!a_params.movie->GetVariable(&menuObj, "root.Menu_mc") || !menuObj.IsObject()) {
					return;
				}

				Submenus::CloseAll(hostBit);
				const auto id = entry.GetMember(kEntryIdMember, &idValue) ? GFx::ToNumber(idValue) : std::nullopt;
				if (!id) {
					return;
				}

				event.Invoke("stopPropagation");
				BackOutToMainState(menuObj);

				if (static_cast<MenuHelper_EntryId>(*id) == kGroupEntryId) {
					PlayMenuSound(menuObj, "PlayOKSound");
					Submenus::QueueOpen(hostBit, g_groupMenus[HostIndex(hostBit)]);
					return;
				}
				const auto found = Registry::Get().FindEntry(static_cast<MenuHelper_EntryId>(*id));
				if (!found) {
					REX::WARN("press on removed entry {}", *id);
					return;
				}
				PlayMenuSound(menuObj, found->enabled ? "PlayOKSound" : "PlayCancelSound");
				if (!found->enabled) {
					return;
				}
				if (found->onPress) {
					found->onPress(found->id, hostBit, found->user);
				}
				if (found->submenu != MENUHELPER_INVALID_ID) {
					Submenus::QueueOpen(hostBit, found->submenu);
				}
			}
		};

		PressHandler* g_pressHandler{ nullptr };

		void AttachToMovie(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit, Value& a_root, Value& a_list)
		{
			if (a_root.HasMember(kRootMarkerMember)) {
				return;
			}
			Translations::OnGameReady();
			if (!g_pressHandler) {
				g_pressHandler = new PressHandler();
			}
			Value listener;
			a_menu.uiMovie->CreateFunction(&listener, g_pressHandler, reinterpret_cast<void*>(static_cast<std::uintptr_t>(a_menuBit)));
			if (!a_list.Invoke("addEventListener", std::array{ Value(kItemPressEvent), listener })) {
				REX::WARN("failed to attach itemPress listener");
				return;
			}
			a_root.SetMember(kRootMarkerMember, listener);
			Submenus::Attach(a_menu, a_menuBit);
		}

		bool FindMainList(Value& a_menuObj, Value& a_list, Value& a_entries)
		{
			Value panel;
			return a_menuObj.IsObject() &&
			       a_menuObj.GetMember("MainPanel_mc", &panel) && panel.IsObject() &&
			       panel.GetMember("List_mc", &a_list) && a_list.IsObject() &&
			       a_list.GetMember("entryList", &a_entries) && a_entries.IsArray();
		}

		std::optional<double> NumberMember(const Value& a_object, const char* a_name)
		{
			Value member;
			return a_object.IsObject() && a_object.GetMember(a_name, &member) ? GFx::ToNumber(member) : std::nullopt;
		}

		std::optional<double> SelectedEntryIndex(Value& a_list)
		{
			Value entry;
			return a_list.GetMember("selectedEntry", &entry) ? NumberMember(entry, "index") : std::nullopt;
		}

		// Entries MenuHelper does not own (vanilla ones and those of other mods) in their original order.
		// Vanilla rebuilds refill the same entryList array, so new entries are the ones without our marker.
		// Entries other mods add later to the arranged list are placed after the entry they follow there.
		std::vector<Value> BaseEntries(Scaleform::GFx::Movie& a_movie, Value& a_root, Value& a_entries)
		{
			std::vector<Value> current;
			for (std::uint32_t i = 0; i < a_entries.GetArraySize(); ++i) {
				Value element;
				if (a_entries.GetElement(i, &element) && element.IsObject() && !element.HasMember(kEntryIdMember)) {
					current.push_back(element);
				}
			}

			std::vector<Value> base;
			Value              stored;
			const bool         rebuilt = std::ranges::none_of(current, [](const Value& a_object) { return a_object.HasMember(kVanillaMarkerMember); });
			if (!rebuilt && a_root.GetMember(kVanillaEntriesMember, &stored) && stored.IsArray()) {
				base.resize(stored.GetArraySize());
				for (std::uint32_t i = 0; i < base.size(); ++i) {
					stored.GetElement(i, &base[i]);
				}
			}

			bool changed = false;
			auto position = base.begin();
			for (auto& element : current) {
				const auto id = NumberMember(element, kVanillaMarkerMember);
				const auto found = id ? std::ranges::find_if(base, [&](const Value& a_object) { return NumberMember(a_object, kVanillaMarkerMember) == id; }) : base.end();
				if (found != base.end()) {
					position = found + 1;
					continue;
				}
				if (!id) {
					element.SetMember(kVanillaMarkerMember, Value(g_nextBaseId++));
				}
				position = base.insert(position, element) + 1;
				changed = true;
			}
			if (changed) {
				a_movie.CreateArray(&stored);
				for (const auto& element : base) {
					stored.PushBack(element);
				}
				a_root.SetMember(kVanillaEntriesMember, stored);
			}
			return base;
		}

		Row MakeModRow(Scaleform::GFx::Movie& a_movie, MenuHelper_EntryId a_id, std::string_view a_modName, std::string_view a_label, bool a_enabled)
		{
			const auto label = GFx::EscapeHtml(Translations::Lookup(a_modName, a_label));
			Row        row{ .index = kInjectedIndexBase + a_id, .mod = true };
			a_movie.CreateObject(&row.object);
			row.object.SetMember("text", Value(label.c_str()));
			row.object.SetMember("index", Value(row.index));
			row.object.SetMember("disabled", Value(!a_enabled));
			row.object.SetMember(kEntryIdMember, Value(static_cast<double>(a_id)));
			return row;
		}

		auto FindVanillaRow(std::vector<Row>& a_rows, std::int32_t a_entry)
		{
			return std::ranges::find_if(a_rows, [&](const Row& a_row) { return !a_row.mod && static_cast<std::int32_t>(a_row.index) == a_entry; });
		}

		void OnGroupItemPress(MenuHelper_ItemId, double, void* a_user)
		{
			const auto data = reinterpret_cast<std::uintptr_t>(a_user);
			const auto entry = Registry::Get().FindEntry(static_cast<MenuHelper_EntryId>(data & 0xFFFFFFFF));
			if (entry && entry->onPress) {
				entry->onPress(entry->id, static_cast<std::uint32_t>(data >> 32), entry->user);
			}
		}

		void SyncGroupMenu(std::uint32_t a_hostBit, const std::vector<Entry>& a_entries)
		{
			auto& menu = g_groupMenus[HostIndex(a_hostBit)];
			if (menu == MENUHELPER_INVALID_ID) {
				MenuHelper_MenuDesc desc{};
				desc.structSize = sizeof(desc);
				desc.modName = kModName;
				menu = Registry::Get().CreateMenu(desc);
			}
			std::vector<Item> items;
			for (const auto& entry : a_entries) {
				Item item;
				item.modName = entry.modName;
				item.label = entry.label;
				item.enabled = entry.enabled;
				item.submenu = entry.submenu;
				item.onChange = OnGroupItemPress;
				item.user = reinterpret_cast<void*>((static_cast<std::uintptr_t>(a_hostBit) << 32) | entry.id);
				items.push_back(std::move(item));
			}
			Registry::Get().ReplaceItems(menu, std::move(items));
			Submenus::RequestRefresh(menu);
		}

		void GroupModRows(Scaleform::GFx::Movie& a_movie, std::uint32_t a_hostBit, std::vector<Row>& a_rows, const std::vector<Entry>& a_entries)
		{
			const auto position = std::ranges::find_if(a_rows, &Row::mod) - a_rows.begin();
			std::erase_if(a_rows, [](const Row& a_row) { return a_row.mod; });
			a_rows.insert(a_rows.begin() + position, MakeModRow(a_movie, kGroupEntryId, kModName, kGroupLabel, true));
			SyncGroupMenu(a_hostBit, a_entries);
		}

		void MoveRow(std::vector<Row>& a_rows, std::int32_t a_entry, std::int32_t a_move)
		{
			const auto it = FindVanillaRow(a_rows, a_entry);
			if (it == a_rows.end()) {
				return;
			}
			const auto from = it - a_rows.begin();
			const auto to = std::clamp<std::ptrdiff_t>(from + a_move, 0, std::ssize(a_rows) - 1);
			if (to > from) {
				std::rotate(a_rows.begin() + from, a_rows.begin() + from + 1, a_rows.begin() + to + 1);
			} else if (to < from) {
				std::rotate(a_rows.begin() + to, a_rows.begin() + from, a_rows.begin() + from + 1);
			}
		}

		void RestoreSelection(Value& a_list, const std::vector<Row>& a_rows, std::initializer_list<std::optional<double>> a_preferred)
		{
			auto target = a_rows.end();
			for (const auto& index : a_preferred) {
				if (index && target == a_rows.end()) {
					target = std::ranges::find(a_rows, *index, &Row::index);
				}
			}
			if (target == a_rows.end()) {
				target = std::ranges::find_if(a_rows, [](const Row& a_row) {
					Value disabled;
					return !a_row.object.GetMember("disabled", &disabled) || !GFx::IsTrue(disabled);
				});
			}
			const auto position = target != a_rows.end() ? static_cast<std::int32_t>(target - a_rows.begin()) : -1;
			if (NumberMember(a_list, "selectedIndex") != static_cast<double>(position)) {
				a_list.SetMember("selectedIndex", Value(position));
			}
		}

		void SyncTrackedRows(Value& a_menuObj, const std::vector<Value>& a_base, std::vector<Row>& a_rows, std::uint32_t a_rowCount)
		{
			std::vector<std::pair<const char*, std::int32_t>> updates;
			for (const auto& [entry, function] : kTrackedRows) {
				const bool listed = std::ranges::any_of(a_base, [&](const Value& a_object) {
					return NumberMember(a_object, "index") == static_cast<double>(entry);
				});
				if (!listed) {
					continue;
				}
				const auto it = FindVanillaRow(a_rows, entry);
				const auto row = it != a_rows.end() ? static_cast<std::int32_t>(it - a_rows.begin()) : -1;
				updates.emplace_back(function, row < static_cast<std::int32_t>(a_rowCount) ? row : -1);
			}
			// With -1, vanilla resets the old row and stores -1 before it fails on the missing row; that clears it.
			for (const auto& [function, row] : updates) {
				a_menuObj.Invoke(function, std::array{ Value(row) });
			}
			// Each update resets its old row, which may be the row another update has just set.
			for (const auto& [function, row] : updates) {
				if (row >= 0) {
					a_menuObj.Invoke(function, std::array{ Value(row) });
				}
			}
		}

		void Rebuild(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit, std::optional<double> a_selectedIndex)
		{
			auto* movie = a_menu.uiMovie.get();
			Value root, list, entries;
			if (!movie || !movie->GetVariable(&root, "root") || !root.IsObject() || !FindMainList(a_menu.menuObj, list, entries)) {
				REX::WARN("Menu_mc.MainPanel_mc.List_mc.entryList not found; a replaced MainMenu.swf is probably installed");
				return;
			}
			AttachToMovie(a_menu, a_menuBit, root, list);

			const auto vanillaSelected = SelectedEntryIndex(list);
			const bool hasSelection = NumberMember(list, "selectedIndex").value_or(-1.0) >= 0.0;
			const auto rowCount = static_cast<std::uint32_t>(NumberMember(list, "numListItems").value_or(0.0));
			const auto base = BaseEntries(*movie, root, entries);
			const auto registered = Registry::Get().Entries(a_menuBit);
			const auto changes = Registry::Get().VanillaChanges(a_menuBit);

			std::vector<Row> rows;
			for (const auto& object : base) {
				rows.push_back({ .object = object, .index = NumberMember(object, "index").value_or(-1.0) });
			}
			for (const auto& entry : registered) {
				const auto before = entry.insertBefore >= 0 ? FindVanillaRow(rows, entry.insertBefore) : rows.end();
				rows.insert(before, MakeModRow(*movie, entry.id, entry.modName, entry.label, entry.enabled));
			}
			std::erase_if(rows, [&](const Row& a_row) {
				return !a_row.mod && std::ranges::any_of(changes, [&](const VanillaChange& a_change) {
					return a_change.hidden && a_change.entry == static_cast<std::int32_t>(a_row.index);
				});
			});
			const auto folder = Config::Get().modsFolder;
			const bool overflows = rowCount > 0 && rows.size() > rowCount;
			if (!registered.empty() && (folder == Config::ModsFolder::kAlways || (folder == Config::ModsFolder::kAuto && overflows))) {
				GroupModRows(*movie, a_menuBit, rows, registered);
			}
			for (const auto& change : changes) {
				if (change.move != 0) {
					MoveRow(rows, change.entry, change.move);
				}
			}

			entries.ClearElements();
			for (const auto& row : rows) {
				entries.PushBack(row.object);
			}
			list.Invoke("InvalidateData");
			if (hasSelection) {
				const auto wasModEntry = a_selectedIndex >= kInjectedIndexBase;
				RestoreSelection(list, rows, { a_selectedIndex, wasModEntry ? std::optional{ kInjectedIndexBase } : std::nullopt, vanillaSelected });
			}
			SyncTrackedRows(a_menu.menuObj, base, rows, rowCount);
			Submenus::OnMainListRebuilt(a_menu);
		}

		bool HasInputUserVtable(std::uintptr_t a_vtable)
		{
			struct CompleteObjectLocator
			{
				std::uint32_t signature;
				std::uint32_t offset;
			};
			const auto* locator = *reinterpret_cast<const CompleteObjectLocator* const*>(a_vtable - sizeof(void*));
			return locator && locator->offset == kInputEventUserOffset;
		}

		template <class Menu, std::uint32_t MENU_BIT>
		struct InitMainListHook
		{
			static void thunk(RE::StartMenuBase* a_this)
			{
				Value list, entries;
				const auto selected = FindMainList(a_this->menuObj, list, entries) ? SelectedEntryIndex(list) : std::nullopt;
				func(a_this);
				Rebuild(*a_this, MENU_BIT, selected);
			}

			static inline REL::Relocation<decltype(&thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ Menu::VTABLE[0] };
				func = vtbl.write_vfunc(kInitMainListVFuncIdx, &thunk);
			}
		};

		template <class Menu>
		struct ButtonEventHook
		{
			static void thunk(RE::BSInputEventUser* a_this, const RE::ButtonEvent* a_event)
			{
				if (a_event && Submenus::HandleButtonEvent(*static_cast<RE::IMenu*>(a_this), *a_event)) {
					return;
				}
				func(a_this, a_event);
			}

			static inline REL::Relocation<decltype(&thunk)> func;

			// While a hotkey row waits for a key, the menu must also receive buttons it has no user event for.
			static bool ShouldHandle(RE::BSInputEventUser* a_this, const RE::InputEvent* a_event)
			{
				return (a_event && *a_event->eventType == RE::INPUT_EVENT_TYPE::kButton && Hotkeys::IsCapturing()) || shouldHandle(a_this, a_event);
			}

			static inline REL::Relocation<decltype(&ShouldHandle)> shouldHandle;

			static bool Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ Menu::VTABLE[1] };
				if (!HasInputUserVtable(vtbl.address())) {
					REX::ERROR("{} vtable layout differs from the expected one; submenus will not handle Back", Menu::MENU_NAME);
					return false;
				}
				func = vtbl.write_vfunc(kOnButtonEventVFuncIdx, &thunk);
				shouldHandle = vtbl.write_vfunc(kShouldHandleEventVFuncIdx, &ShouldHandle);
				return true;
			}
		};
	}

	// Idempotent: vanilla rebuilds the list from several native and AS3 paths, not only InitMainList.
	void InjectEntries(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit)
	{
		Rebuild(a_menu, a_menuBit, std::nullopt);
	}

	void Install()
	{
		InitMainListHook<RE::MainMenu, MENUHELPER_MENU_MAIN>::Install();
		InitMainListHook<RE::PauseMenu, MENUHELPER_MENU_PAUSE>::Install();
		ButtonEventHook<RE::MainMenu>::Install();
		ButtonEventHook<RE::PauseMenu>::Install();
		REX::INFO("menu hooks installed");
	}

	// Rearranges the list in place: a vanilla rebuild would drop entries other mods added and the selection.
	void RefreshOpenMenus()
	{
		F4SE::GetTaskInterface()->AddUITask([]() {
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return;
			}
			for (const auto& [name, bit] : { std::pair{ RE::MainMenu::MENU_NAME, MENUHELPER_MENU_MAIN }, std::pair{ RE::PauseMenu::MENU_NAME, MENUHELPER_MENU_PAUSE } }) {
				if (const auto menu = ui->GetMenu(name); menu && menu->uiMovie) {
					InjectEntries(static_cast<RE::StartMenuBase&>(*menu), bit);
				}
			}
		});
	}
}
