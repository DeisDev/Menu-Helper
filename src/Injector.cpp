#include "Injector.h"

#include "GFxUtil.h"
#include "Registry.h"
#include "Submenus.h"

namespace MenuHelper::Injector
{
	namespace
	{
		using GFx::Value;

		constexpr std::size_t kInitMainListVFuncIdx = 0x15;
		constexpr std::size_t kOnButtonEventVFuncIdx = 0x08;
		constexpr std::size_t kInputEventUserOffset = 0x10;
		constexpr double      kInjectedIndexBase = 1000.0;
		constexpr const char* kEntryIdMember = "menuHelperId";
		constexpr const char* kRootMarkerMember = "MenuHelperPressListener";
		constexpr const char* kItemPressEvent = "BSScrollingList::itemPress";
		constexpr const char* kMainState = "MAIN_STATE";
		constexpr int         kMaxBackOutSteps = 16;

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

		void AttachToMovie(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit, Value& a_list)
		{
			auto* movie = a_menu.uiMovie.get();
			Value root;
			if (!movie->GetVariable(&root, "root") || !root.IsObject()) {
				REX::WARN("MainMenu.swf root not found");
				return;
			}
			if (root.HasMember(kRootMarkerMember)) {
				return;
			}

			if (!g_pressHandler) {
				g_pressHandler = new PressHandler();
			}
			Value listener;
			movie->CreateFunction(&listener, g_pressHandler, reinterpret_cast<void*>(static_cast<std::uintptr_t>(a_menuBit)));
			if (!a_list.Invoke("addEventListener", std::array{ Value(kItemPressEvent), listener })) {
				REX::WARN("failed to attach itemPress listener");
				return;
			}
			root.SetMember(kRootMarkerMember, listener);
			Submenus::Attach(a_menu, a_menuBit);
		}

		std::int32_t FindVanillaIndex(Value& a_entries, std::int32_t a_vanillaIndex)
		{
			const auto size = a_entries.GetArraySize();
			for (std::uint32_t i = 0; i < size; ++i) {
				Value element, index;
				if (a_entries.GetElement(i, &element) && element.IsObject() && element.GetMember("index", &index)) {
					const auto number = GFx::ToNumber(index);
					if (number && static_cast<std::int32_t>(*number) == a_vanillaIndex) {
						return static_cast<std::int32_t>(i);
					}
				}
			}
			return -1;
		}

		bool RemoveInjected(Value& a_entries)
		{
			bool removed = false;
			for (auto i = static_cast<std::int64_t>(a_entries.GetArraySize()) - 1; i >= 0; --i) {
				Value element;
				if (a_entries.GetElement(static_cast<std::uint32_t>(i), &element) && element.IsObject() && element.HasMember(kEntryIdMember)) {
					a_entries.Invoke("splice", std::array{ Value(static_cast<double>(i)), Value(1.0) });
					removed = true;
				}
			}
			return removed;
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
				func(a_this);
				InjectEntries(*a_this, MENU_BIT);
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

			static bool Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ Menu::VTABLE[1] };
				if (!HasInputUserVtable(vtbl.address())) {
					REX::ERROR("{} vtable layout differs from the expected one; submenus will not handle Back", Menu::MENU_NAME);
					return false;
				}
				func = vtbl.write_vfunc(kOnButtonEventVFuncIdx, &thunk);
				return true;
			}
		};
	}

	// Idempotent: vanilla rebuilds the list from several native and AS3 paths, not only InitMainList.
	void InjectEntries(RE::StartMenuBase& a_menu, std::uint32_t a_menuBit)
	{
		auto* movie = a_menu.uiMovie.get();
		if (!movie || !a_menu.menuObj.IsObject()) {
			return;
		}

		Value panel, list, entries;
		if (!a_menu.menuObj.GetMember("MainPanel_mc", &panel) || !panel.IsObject() ||
			!panel.GetMember("List_mc", &list) || !list.IsObject() ||
			!list.GetMember("entryList", &entries) || !entries.IsArray()) {
			REX::WARN("Menu_mc.MainPanel_mc.List_mc.entryList not found; a replaced MainMenu.swf is probably installed");
			return;
		}

		AttachToMovie(a_menu, a_menuBit, list);

		const bool removed = RemoveInjected(entries);
		const auto registered = Registry::Get().Entries(a_menuBit);
		for (const auto& entry : registered) {
			const auto label = GFx::EscapeHtml(entry.label);
			Value      obj;
			movie->CreateObject(&obj);
			obj.SetMember("text", Value(label.c_str()));
			obj.SetMember("index", Value(kInjectedIndexBase + entry.id));
			obj.SetMember("disabled", Value(!entry.enabled));
			obj.SetMember(kEntryIdMember, Value(static_cast<double>(entry.id)));

			const auto position = entry.insertBefore >= 0 ? FindVanillaIndex(entries, entry.insertBefore) : -1;
			if (position >= 0) {
				entries.Invoke("splice", std::array{ Value(static_cast<double>(position)), Value(0.0), obj });
			} else {
				entries.PushBack(obj);
			}
		}

		if (removed || !registered.empty()) {
			list.Invoke("InvalidateData");
		}
		Submenus::OnMainListRebuilt(a_menu, static_cast<std::uint32_t>(registered.size()));
	}

	void Install()
	{
		InitMainListHook<RE::MainMenu, MENUHELPER_MENU_MAIN>::Install();
		InitMainListHook<RE::PauseMenu, MENUHELPER_MENU_PAUSE>::Install();
		ButtonEventHook<RE::MainMenu>::Install();
		ButtonEventHook<RE::PauseMenu>::Install();
		REX::INFO("menu hooks installed");
	}

	void RefreshOpenMenus()
	{
		F4SE::GetTaskInterface()->AddUITask([]() {
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return;
			}
			for (const auto name : { RE::MainMenu::MENU_NAME, RE::PauseMenu::MENU_NAME }) {
				if (const auto menu = ui->GetMenu(name); menu && menu->uiMovie) {
					static_cast<RE::StartMenuBase*>(menu.get())->InitMainList();
				}
			}
		});
	}
}
