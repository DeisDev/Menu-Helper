#include "Submenus.h"

#include "GFxUtil.h"
#include "Injector.h"
#include "Registry.h"
#include "Translations.h"

namespace MenuHelper::Submenus
{
	namespace
	{
		using GFx::Value;

		constexpr const char* kTemplateUrl = "MenuHelper/MenuHelper.swf";
		constexpr const char* kCodeObjMember = "MenuHelperCodeObj";
		constexpr const char* kUIPath = "root.MenuHelperUI";

		enum class Function : std::uintptr_t
		{
			kReady,
			kTemplateError,
			kLoadError,
			kMenuOpened,
			kMenuClosed,
			kItemPress,
			kValueChange,
			kGetButtonHintData,
			kAddShaderTarget,
			kRemoveShaderTarget,
			kInjectMainEntries
		};

		constexpr std::array kCodeObjFunctions{
			std::pair{ "OnReady", Function::kReady },
			std::pair{ "OnTemplateError", Function::kTemplateError },
			std::pair{ "OnMenuOpened", Function::kMenuOpened },
			std::pair{ "OnMenuClosed", Function::kMenuClosed },
			std::pair{ "OnItemPress", Function::kItemPress },
			std::pair{ "OnValueChange", Function::kValueChange },
			std::pair{ "GetButtonHintData", Function::kGetButtonHintData },
			std::pair{ "AddShaderTarget", Function::kAddShaderTarget },
			std::pair{ "RemoveShaderTarget", Function::kRemoveShaderTarget },
			std::pair{ "InjectMainEntries", Function::kInjectMainEntries },
		};

		struct ShaderTarget
		{
			std::uint32_t                             handle;
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
		};

		struct Host
		{
			bool                           ready{ false };
			std::vector<MenuHelper_MenuId> pending;
			std::vector<MenuHelper_MenuId> open;
			const Scaleform::GFx::Movie*   targetMovie{ nullptr };
			std::vector<ShaderTarget>      targets;
			std::uint32_t                  nextTarget{ 1 };
		};

		std::mutex           g_lock;
		std::array<Host, 2>  g_hosts;
		constexpr std::array kHostBits{ MENUHELPER_MENU_MAIN, MENUHELPER_MENU_PAUSE };

		Host& HostFor(std::uint32_t a_hostBit)
		{
			return g_hosts[a_hostBit == MENUHELPER_MENU_PAUSE ? 1 : 0];
		}

		std::string_view HostName(std::uint32_t a_hostBit)
		{
			return a_hostBit == MENUHELPER_MENU_PAUSE ? RE::PauseMenu::MENU_NAME : RE::MainMenu::MENU_NAME;
		}

		Scaleform::Ptr<RE::IMenu> GetHostMenu(std::uint32_t a_hostBit)
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			auto menu = ui->GetMenu(HostName(a_hostBit));
			return menu && menu->uiMovie ? menu : nullptr;
		}

		std::uint32_t ActiveHost()
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return 0;
			}
			if (ui->GetMenuOpen(RE::PauseMenu::MENU_NAME)) {
				return MENUHELPER_MENU_PAUSE;
			}
			return ui->GetMenuOpen(RE::MainMenu::MENU_NAME) ? MENUHELPER_MENU_MAIN : 0;
		}

		void NotifyClosed(MenuHelper_MenuId a_menu)
		{
			const auto menu = Registry::Get().FindMenu(a_menu);
			if (menu && menu->onClose) {
				menu->onClose(a_menu, menu->user);
			}
		}

		// Targets hold Scaleform values, so they may only be destroyed while their movie is alive.
		void ReleaseTargets(std::uint32_t a_hostBit, const Scaleform::GFx::Movie* a_movie, std::vector<ShaderTarget> a_targets)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (host && host->uiMovie.get() == a_movie) {
				auto& registered = static_cast<RE::GameMenuBase*>(host.get())->shaderFXObjects;
				for (const auto& entry : a_targets) {
					if (const auto it = std::ranges::find(registered, entry.target.get()); it != registered.end()) {
						registered.erase(it);
					}
				}
				return;
			}
			for (auto& entry : a_targets) {
				if (const auto source = RE::ApplyColorUpdateEvent::GetEventSource()) {
					source->UnregisterSink(entry.target.get());
				}
				[[maybe_unused]] const auto leaked = entry.target.release();
			}
		}

		void ResetHost(std::uint32_t a_hostBit)
		{
			std::vector<MenuHelper_MenuId> open;
			std::vector<ShaderTarget>      targets;
			const Scaleform::GFx::Movie*   movie{ nullptr };
			{
				const std::scoped_lock l{ g_lock };
				auto& host = HostFor(a_hostBit);
				open = std::move(host.open);
				targets = std::move(host.targets);
				movie = host.targetMovie;
				host = Host{};
			}
			ReleaseTargets(a_hostBit, movie, std::move(targets));
			std::ranges::for_each(open | std::views::reverse, NotifyClosed);
		}

		const RE::BSGFxShaderFXTarget* VanillaTarget(const RE::StartMenuBase& a_menu, std::string_view a_kind)
		{
			if (a_kind == "settingsCategoryList") {
				return a_menu.settingsCategoryList.get();
			}
			if (a_kind == "settingsList") {
				return a_menu.settingsList.get();
			}
			if (a_kind == "settingGuideText") {
				return a_menu.settingGuideText.get();
			}
			return nullptr;
		}

		std::uint32_t AddShaderTarget(std::uint32_t a_hostBit, const Value& a_object, std::string_view a_kind)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (!host || !a_object.IsObject()) {
				return 0;
			}
			auto&      menu = static_cast<RE::StartMenuBase&>(*host);
			const auto vanilla = VanillaTarget(menu, a_kind);
			if (!vanilla) {
				REX::ERROR("{} has no {} shader target", HostName(a_hostBit), a_kind);
				return 0;
			}
			auto target = std::make_unique<RE::BSGFxShaderFXTarget>(a_object);
			target->CreateAndSetFiltersToHUD(vanilla->HUDColorType.get());
			menu.shaderFXObjects.push_back(target.get());

			const std::scoped_lock l{ g_lock };
			auto& state = HostFor(a_hostBit);
			state.targetMovie = host->uiMovie.get();
			const auto handle = state.nextTarget++;
			state.targets.push_back({ handle, std::move(target) });
			return handle;
		}

		void RemoveShaderTarget(std::uint32_t a_hostBit, std::uint32_t a_handle)
		{
			std::vector<ShaderTarget>    removed;
			const Scaleform::GFx::Movie* movie{ nullptr };
			{
				const std::scoped_lock l{ g_lock };
				auto&      state = HostFor(a_hostBit);
				const auto it = std::ranges::find(state.targets, a_handle, &ShaderTarget::handle);
				if (it == state.targets.end()) {
					return;
				}
				removed.push_back(std::move(*it));
				state.targets.erase(it);
				movie = state.targetMovie;
			}
			ReleaseTargets(a_hostBit, movie, std::move(removed));
		}

		bool InvokeUI(RE::IMenu& a_host, const char* a_method, std::span<const Value> a_args = {}, Value* a_result = nullptr)
		{
			Value ui;
			if (!a_host.uiMovie || !a_host.uiMovie->GetVariable(&ui, kUIPath) || !ui.IsObject()) {
				return false;
			}
			return ui.Invoke(a_method, a_result, a_args.data(), a_args.size());
		}

		Value BuildItems(Scaleform::GFx::Movie& a_movie, const Menu& a_menu)
		{
			Value items;
			a_movie.CreateArray(&items);
			for (const auto& item : a_menu.items) {
				const auto label = GFx::EscapeHtml(Translations::Lookup(item.modName, item.label));
				const std::string description{ Translations::Lookup(item.modName, item.description) };
				const auto range = item.max - item.min;
				const bool slider = item.type == MENUHELPER_ITEM_SLIDER;

				Value obj;
				a_movie.CreateObject(&obj);
				obj.SetMember("itemId", Value(item.id));
				obj.SetMember("itemType", Value(item.type));
				obj.SetMember("text", Value(label.c_str()));
				obj.SetMember("description", Value(description.c_str()));
				obj.SetMember("disabled", Value(!item.enabled));
				obj.SetMember("value", Value(static_cast<double>(slider ? (item.value - item.min) / range : item.value)));
				obj.SetMember("step", Value(static_cast<double>(slider ? item.step / range : 0.0F)));
				if (item.type == MENUHELPER_ITEM_STEPPER) {
					Value options;
					a_movie.CreateArray(&options);
					for (const auto& option : item.options) {
						options.PushBack(Value(std::string{ Translations::Lookup(item.modName, option) }.c_str()));
					}
					obj.SetMember("options", options);
				}
				items.PushBack(obj);
			}
			return items;
		}

		bool IsOpenIn(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			const std::scoped_lock l{ g_lock };
			return std::ranges::contains(HostFor(a_hostBit).open, a_menu);
		}

		void OpenNow(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (!host) {
				return;
			}
			{
				const std::scoped_lock l{ g_lock };
				auto& state = HostFor(a_hostBit);
				if (!state.ready) {
					state.pending.push_back(a_menu);
					return;
				}
			}

			auto menu = Registry::Get().FindMenu(a_menu);
			if (!menu) {
				REX::WARN("cannot open menu {}: it does not exist", a_menu);
				return;
			}
			if (IsOpenIn(a_hostBit, a_menu)) {
				InvokeUI(*host, "CloseMenu", std::array{ Value(a_menu) });
			}
			if (menu->onOpen) {
				menu->onOpen(a_menu, menu->user);
				menu = Registry::Get().FindMenu(a_menu);
				if (!menu) {
					return;
				}
			}

			const std::array args{ Value(a_menu), BuildItems(*host->uiMovie, *menu), Value(menu->UsesOptionsLayout()) };
			Value             result;
			if (!InvokeUI(*host, "OpenMenu", args, &result) || !GFx::IsTrue(result)) {
				REX::ERROR("menu {} could not be shown; see earlier MenuHelper errors", a_menu);
			}
		}

		void RefreshNow(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (!host || !IsOpenIn(a_hostBit, a_menu)) {
				return;
			}
			const auto menu = Registry::Get().FindMenu(a_menu);
			if (!menu) {
				InvokeUI(*host, "CloseMenu", std::array{ Value(a_menu) });
				return;
			}
			const std::array args{ Value(a_menu), BuildItems(*host->uiMovie, *menu), Value(menu->UsesOptionsLayout()) };
			InvokeUI(*host, "UpdateMenu", args);
		}

		void OnItemPress(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu, MenuHelper_ItemId a_item)
		{
			const auto item = Registry::Get().FindItem(a_item);
			if (!item || item->menu != a_menu || !item->enabled) {
				return;
			}
			if (item->onChange) {
				item->onChange(item->id, 0.0F, item->user);
			}
			if (item->submenu != MENUHELPER_INVALID_ID) {
				QueueOpen(a_hostBit, item->submenu);
			}
		}

		void OnValueChange(MenuHelper_MenuId a_menu, MenuHelper_ItemId a_item, double a_raw)
		{
			const auto item = Registry::Get().FindItem(a_item);
			if (!item || item->menu != a_menu || !item->enabled || item->type == MENUHELPER_ITEM_BUTTON) {
				return;
			}
			auto value = static_cast<float>(a_raw);
			if (item->type == MENUHELPER_ITEM_SLIDER) {
				value = item->min + value * (item->max - item->min);
			}
			Registry::Get().SetItemValue(a_item, value);
			const auto updated = Registry::Get().FindItem(a_item);
			if (updated && updated->onChange) {
				updated->onChange(updated->id, updated->value, updated->user);
			}
		}

		void OnReady(std::uint32_t a_hostBit)
		{
			REX::INFO("submenu template ready in {}", HostName(a_hostBit));
			std::vector<MenuHelper_MenuId> pending;
			{
				const std::scoped_lock l{ g_lock };
				auto& host = HostFor(a_hostBit);
				host.ready = true;
				pending = std::move(host.pending);
			}
			for (const auto menu : pending) {
				QueueOpen(a_hostBit, menu);
			}
		}

		void OnMenuOpened(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			const std::scoped_lock l{ g_lock };
			HostFor(a_hostBit).open.push_back(a_menu);
		}

		void OnMenuClosed(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			{
				const std::scoped_lock l{ g_lock };
				auto&      open = HostFor(a_hostBit).open;
				const auto it = std::ranges::find(open | std::views::reverse, a_menu);
				if (it == open.rend()) {
					return;
				}
				open.erase(std::next(it).base());
			}
			NotifyClosed(a_menu);
		}

		void GetButtonHintData(std::uint32_t a_hostBit, Value& a_result)
		{
			const auto host = GetHostMenu(a_hostBit);
			const auto bar = host ? static_cast<RE::GameMenuBase*>(host.get())->buttonHintBar.get() : nullptr;
			if (bar && bar->sourceButtons.IsObject()) {
				a_result = bar->sourceButtons;
			} else {
				REX::WARN("{} has no native button hint bar; submenus will not show a Back hint", HostName(a_hostBit));
			}
		}

		std::optional<std::uint32_t> ArgId(const Scaleform::GFx::FunctionHandler::Params& a_params, std::uint32_t a_index)
		{
			if (a_params.argCount <= a_index) {
				return std::nullopt;
			}
			const auto number = GFx::ToNumber(a_params.args[a_index]);
			return number ? std::optional{ static_cast<std::uint32_t>(*number) } : std::nullopt;
		}

		class CodeObjHandler : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const auto data = reinterpret_cast<std::uintptr_t>(a_params.userData);
				const auto hostBit = static_cast<std::uint32_t>(data >> 8);
				const auto first = ArgId(a_params, 0);
				const auto second = ArgId(a_params, 1);

				switch (static_cast<Function>(data & 0xFF)) {
					case Function::kReady:
						OnReady(hostBit);
						break;
					case Function::kTemplateError:
						REX::ERROR("submenu template cannot run in {}: missing {}", HostName(hostBit),
							a_params.argCount > 0 && a_params.args[0].IsString() ? a_params.args[0].GetString() : "?");
						break;
					case Function::kLoadError:
						REX::ERROR("could not load Interface/{}; is the MenuHelper Interface folder installed?", kTemplateUrl);
						break;
					case Function::kMenuOpened:
						if (first) {
							OnMenuOpened(hostBit, *first);
						}
						break;
					case Function::kMenuClosed:
						if (first) {
							OnMenuClosed(hostBit, *first);
						}
						break;
					case Function::kItemPress:
						if (first && second) {
							OnItemPress(hostBit, *first, *second);
						}
						break;
					case Function::kValueChange:
						if (first && second && a_params.argCount > 2) {
							if (const auto value = GFx::ToNumber(a_params.args[2])) {
								OnValueChange(*first, *second, *value);
							}
						}
						break;
					case Function::kGetButtonHintData:
						if (a_params.retVal) {
							GetButtonHintData(hostBit, *a_params.retVal);
						}
						break;
					case Function::kAddShaderTarget:
						if (a_params.retVal && a_params.argCount > 1 && a_params.args[1].IsString()) {
							*a_params.retVal = Value(AddShaderTarget(hostBit, a_params.args[0], a_params.args[1].GetString()));
						}
						break;
					case Function::kRemoveShaderTarget:
						if (first) {
							RemoveShaderTarget(hostBit, *first);
						}
						break;
					case Function::kInjectMainEntries:
						if (const auto host = GetHostMenu(hostBit)) {
							Injector::InjectEntries(static_cast<RE::StartMenuBase&>(*host), hostBit);
						}
						break;
				}
			}
		};

		class HostCloseSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event.opening) {
					for (const auto bit : kHostBits) {
						if (a_event.menuName == HostName(bit)) {
							ResetHost(bit);
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		CodeObjHandler* g_codeObjHandler{ nullptr };

		Value MakeFunction(Scaleform::GFx::Movie& a_movie, std::uint32_t a_hostBit, Function a_function)
		{
			Value function;
			const auto data = (static_cast<std::uintptr_t>(a_hostBit) << 8) | static_cast<std::uintptr_t>(a_function);
			a_movie.CreateFunction(&function, g_codeObjHandler, reinterpret_cast<void*>(data));
			return function;
		}

		void RegisterCloseSink()
		{
			static HostCloseSink sink;
			static std::once_flag once;
			std::call_once(once, [] { RE::UI::GetSingleton()->RegisterSink<RE::MenuOpenCloseEvent>(&sink); });
		}
	}

	void Attach(RE::IMenu& a_host, std::uint32_t a_hostBit)
	{
		auto* movie = a_host.uiMovie.get();
		Value root;
		if (!movie || !movie->GetVariable(&root, "root") || !root.IsObject() || root.HasMember(kCodeObjMember)) {
			return;
		}

		RegisterCloseSink();
		ResetHost(a_hostBit);
		if (!g_codeObjHandler) {
			g_codeObjHandler = new CodeObjHandler();
		}

		Value codeObj;
		movie->CreateObject(&codeObj);
		for (const auto& [name, function] : kCodeObjFunctions) {
			codeObj.SetMember(name, MakeFunction(*movie, a_hostBit, function));
		}
		root.SetMember(kCodeObjMember, codeObj);

		Value       loader, request, loaderInfo;
		const Value url(kTemplateUrl);
		movie->CreateObject(&loader, "flash.display.Loader");
		movie->CreateObject(&request, "flash.net.URLRequest", &url, 1);
		if (!loader.IsObject() || !request.IsObject() || !loader.GetMember("contentLoaderInfo", &loaderInfo) || !loaderInfo.IsObject()) {
			REX::ERROR("could not create a Loader for {}", kTemplateUrl);
			return;
		}
		loaderInfo.Invoke("addEventListener", std::array{ Value("ioError"), MakeFunction(*movie, a_hostBit, Function::kLoadError) });
		a_host.menuObj.Invoke("addChild", std::array{ loader });
		loader.Invoke("load", std::array{ request });
	}

	void OnMainListRebuilt(RE::IMenu& a_host)
	{
		InvokeUI(a_host, "OnMainListInjected");
	}

	void CloseAll(std::uint32_t a_hostBit)
	{
		if (const auto host = GetHostMenu(a_hostBit)) {
			InvokeUI(*host, "CloseAll");
		}
	}

	bool HandleButtonEvent(RE::IMenu& a_host, const RE::ButtonEvent& a_event)
	{
		const std::array args{ Value(a_event.QUserEvent().c_str()), Value(a_event.QReleased()) };
		Value            result;
		return InvokeUI(a_host, "ProcessUserEvent", args, &result) && GFx::IsTrue(result);
	}

	void QueueOpen(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
	{
		F4SE::GetTaskInterface()->AddUITask([=]() { OpenNow(a_hostBit, a_menu); });
	}

	bool RequestOpen(MenuHelper_MenuId a_menu)
	{
		const auto host = ActiveHost();
		if (host == 0 || !Registry::Get().FindMenu(a_menu)) {
			return false;
		}
		QueueOpen(host, a_menu);
		return true;
	}

	bool RequestClose(MenuHelper_MenuId a_menu)
	{
		const auto hostBit = ActiveHost();
		if (hostBit == 0 || !IsOpenIn(hostBit, a_menu)) {
			return false;
		}
		F4SE::GetTaskInterface()->AddUITask([=]() {
			if (const auto host = GetHostMenu(hostBit)) {
				InvokeUI(*host, "CloseMenu", std::array{ Value(a_menu) });
			}
		});
		return true;
	}

	void RequestRefresh(MenuHelper_MenuId a_menu)
	{
		F4SE::GetTaskInterface()->AddUITask([=]() {
			for (const auto bit : kHostBits) {
				RefreshNow(bit, a_menu);
			}
		});
	}
}
