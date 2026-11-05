#include "Submenus.h"

#include "Dialogs.h"
#include "GFxUtil.h"
#include "Hotkeys.h"
#include "Injector.h"
#include "JsonMenus.h"
#include "Registry.h"
#include "SavedValues.h"
#include "Translations.h"

namespace MenuHelper::Submenus
{
	namespace
	{
		using GFx::Value;

		constexpr const char* kTemplateUrl = "MenuHelper/MenuHelper.swf";
		constexpr const char* kCodeObjMember = "MenuHelperCodeObj";
		constexpr const char* kUIPath = "root.MenuHelperUI";
		constexpr const char* kModName = "MenuHelper";
		constexpr const char* kColorTargetKind = "color";
		constexpr const char* kPopupBackingKind = "popupBacking";
		constexpr const char* kDefaultFormat = "{value}";
		// Vanilla string, translated by the game.
		constexpr const char* kResetPrompt = "$Reset settings to default values?";
		constexpr const char* kDecimalSeparatorText = "$MenuHelper_DecimalSeparator";
		constexpr const char* kHotkeyPromptText = "$MenuHelper_HotkeyPrompt";
		constexpr std::int32_t kNoSwatch = -1;
		// At 1.0 the color filter doubles every channel; 0.5 shows the color as given.
		constexpr float        kTrueColorBrightness = 0.5F;
		// Not in CommonLibF4. Found from MainMenu's DLC banner loading (Fallout4.exe 1.11.240).
		constexpr std::uint64_t kLoadTextureID = 2274877;
		constexpr std::uint64_t kSetExternalTextureID = 2287494;
		constexpr std::uint64_t kReleaseExternalTextureID = 2287493;

		struct ColorChannel
		{
			const char*   label;
			std::uint32_t shift;
		};

		constexpr std::array kColorChannels{
			ColorChannel{ "$MenuHelper_Red", 16 },
			ColorChannel{ "$MenuHelper_Green", 8 },
			ColorChannel{ "$MenuHelper_Blue", 0 },
			ColorChannel{ "$MenuHelper_Alpha", 24 },
		};

		enum class Function : std::uintptr_t
		{
			kReady,
			kTemplateError,
			kLoadError,
			kMenuOpened,
			kMenuClosed,
			kItemPress,
			kValueChange,
			kItemSelect,
			kResetRequest,
			kConfirmResult,
			kGetButtonHintData,
			kAddShaderTarget,
			kRemoveShaderTarget,
			kSetShaderTargetColor,
			kInjectMainEntries,
			kPrepareImage,
			kLog
		};

		constexpr std::array kCodeObjFunctions{
			std::pair{ "OnReady", Function::kReady },
			std::pair{ "OnTemplateError", Function::kTemplateError },
			std::pair{ "OnMenuOpened", Function::kMenuOpened },
			std::pair{ "OnMenuClosed", Function::kMenuClosed },
			std::pair{ "OnItemPress", Function::kItemPress },
			std::pair{ "OnValueChange", Function::kValueChange },
			std::pair{ "OnItemSelect", Function::kItemSelect },
			std::pair{ "OnResetRequest", Function::kResetRequest },
			std::pair{ "OnConfirmResult", Function::kConfirmResult },
			std::pair{ "GetButtonHintData", Function::kGetButtonHintData },
			std::pair{ "AddShaderTarget", Function::kAddShaderTarget },
			std::pair{ "RemoveShaderTarget", Function::kRemoveShaderTarget },
			std::pair{ "SetShaderTargetColor", Function::kSetShaderTargetColor },
			std::pair{ "InjectMainEntries", Function::kInjectMainEntries },
			std::pair{ "PrepareImage", Function::kPrepareImage },
			std::pair{ "Log", Function::kLog },
		};

		// CommonLibF4's ProcessEvent passes the whole object to the game function, which expects its event sink
		// base and crashes the game when the HUD color changes.
		class GameTarget : public RE::BSGFxShaderFXTarget
		{
		public:
			using BSGFxShaderFXTarget::BSGFxShaderFXTarget;

			RE::BSEventNotifyControl ProcessEvent(const RE::ApplyColorUpdateEvent& a_event, RE::BSTEventSource<RE::ApplyColorUpdateEvent>* a_source) override
			{
				using Sink = RE::BSTEventSink<RE::ApplyColorUpdateEvent>;
				using func_t = RE::BSEventNotifyControl (*)(Sink*, const RE::ApplyColorUpdateEvent&, RE::BSTEventSource<RE::ApplyColorUpdateEvent>*);
				static REL::Relocation<func_t> func{ RE::ID::BSGFxShaderFXTarget::ProcessEvent };
				return func(static_cast<Sink*>(this), a_event, a_source);
			}
		};

		// Shows its object in a fixed color instead of a HUD color, also after the player changes the HUD color.
		class FixedColorTarget : public RE::BSGFxShaderFXTarget
		{
		public:
			FixedColorTarget(const Value& a_object, std::uint32_t a_rgb) :
				BSGFxShaderFXTarget(a_object)
			{
				SetColor(a_rgb);
			}

			void SetColor(std::uint32_t a_rgb)
			{
				_rgb = a_rgb;
				CreateAndSetFiltersToColor(
					static_cast<std::uint8_t>(a_rgb >> 16), static_cast<std::uint8_t>(a_rgb >> 8), static_cast<std::uint8_t>(a_rgb), kTrueColorBrightness);
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::ApplyColorUpdateEvent&, RE::BSTEventSource<RE::ApplyColorUpdateEvent>*) override
			{
				SetColor(_rgb);
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			std::uint32_t _rgb{ 0 };
		};

		struct ShaderTarget
		{
			std::uint32_t                             handle;
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
			FixedColorTarget*                         fixed{ nullptr };
		};

		struct PendingOpen
		{
			MenuHelper_MenuId menu;
			std::int32_t      selection;
		};

		struct Host
		{
			bool                           ready{ false };
			std::vector<PendingOpen>       pending;
			std::vector<MenuHelper_MenuId> open;
			const Scaleform::GFx::Movie*   targetMovie{ nullptr };
			std::vector<ShaderTarget>      targets;
			std::uint32_t                  nextTarget{ 1 };
		};

		std::mutex           g_lock;
		std::array<Host, 2>  g_hosts;
		constexpr std::array kHostBits{ MENUHELPER_MENU_MAIN, MENUHELPER_MENU_PAUSE };
		// MenuHelper's own menus, one per host, reused so menus are never leaked.
		std::array<MenuHelper_MenuId, 2> g_pickers{};
		std::array<MenuHelper_MenuId, 2> g_colorEditors{};
		std::array<MenuHelper_ItemId, 2> g_colorParents{};
		std::map<MenuHelper_BoxId, std::function<void(bool)>> g_confirms;
		// Page images, kept while a host menu is open. The game's image loader only serves registered textures.
		struct Image
		{
			std::unique_ptr<RE::BSScaleformExternalTexture> texture;
			std::string                                      name;
		};

		std::map<std::string, Image, std::less<>> g_images;

		bool IsDds(std::string_view a_path)
		{
			constexpr auto extension = ".dds"sv;
			return a_path.size() >= extension.size() &&
			       std::ranges::equal(a_path.substr(a_path.size() - extension.size()), extension, [](char a_lhs, char a_rhs) {
					   return std::tolower(static_cast<unsigned char>(a_lhs)) == a_rhs;
				   });
		}

		// The texture name is the key img:// URLs are looked up by. Empty when the file cannot be loaded.
		// The returned text stays valid until the images are released, so Scaleform can copy it.
		const char* PrepareImage(std::string_view a_path)
		{
			if (const auto it = g_images.find(a_path); it != g_images.end()) {
				return it->second.name.c_str();
			}
			using LoadTexture_t = std::int32_t(const char*, RE::NiPointer<RE::NiTexture>&, bool, bool, std::uint32_t, bool, bool, bool);
			using SetTexture_t = bool(RE::BSScaleformExternalTexture*, RE::NiTexture*);
			static REL::Relocation<LoadTexture_t*> loadTexture{ REL::ID(kLoadTextureID) };
			static REL::Relocation<SetTexture_t*>  setTexture{ REL::ID(kSetExternalTextureID) };

			const std::string path{ a_path };
			// The texture loader crashes on other file types.
			if (!IsDds(path)) {
				REX::WARN("page image {} is not a .dds file", path);
				return "";
			}
			RE::NiPointer<RE::NiTexture> texture;
			// Same arguments as MainMenu uses for its DLC banners.
			if (loadTexture(path.c_str(), texture, true, false, 0, false, false, true) != 0 || !texture) {
				return "";
			}
			auto external = std::make_unique<RE::BSScaleformExternalTexture>();
			external->renderTarget = 0;
			setTexture(external.get(), texture.get());
			std::string name{ texture->name.c_str() };
			return g_images.emplace(path, Image{ std::move(external), std::move(name) }).first->second.name.c_str();
		}

		void ReleaseImages()
		{
			using ReleaseTexture_t = void(RE::BSScaleformExternalTexture*);
			static REL::Relocation<ReleaseTexture_t*> releaseTexture{ REL::ID(kReleaseExternalTextureID) };
			for (auto& [path, image] : g_images) {
				releaseTexture(image.texture.get());
			}
			g_images.clear();
		}

		std::size_t HostIndex(std::uint32_t a_hostBit)
		{
			return a_hostBit == MENUHELPER_MENU_PAUSE ? 1 : 0;
		}

		Host& HostFor(std::uint32_t a_hostBit)
		{
			return g_hosts[HostIndex(a_hostBit)];
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

		bool InvokeUI(RE::IMenu& a_host, const char* a_method, std::span<const Value> a_args = {}, Value* a_result = nullptr)
		{
			Value ui;
			if (!a_host.uiMovie || !a_host.uiMovie->GetVariable(&ui, kUIPath) || !ui.IsObject()) {
				return false;
			}
			return ui.Invoke(a_method, a_result, a_args.data(), a_args.size());
		}

		void NotifyClosed(MenuHelper_MenuId a_menu)
		{
			const auto menu = Registry::Get().FindMenu(a_menu);
			if (menu && menu->onClose) {
				menu->onClose(a_menu, menu->user);
			}
		}

		void ResolveConfirm(MenuHelper_BoxId a_id, bool a_result)
		{
			std::function<void(bool)> done;
			{
				const std::scoped_lock l{ g_lock };
				const auto             it = g_confirms.find(a_id);
				if (it == g_confirms.end()) {
					return;
				}
				done = std::move(it->second);
				g_confirms.erase(it);
			}
			if (done) {
				done(a_result);
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
			std::vector<MenuHelper_BoxId>  confirms;
			{
				const std::scoped_lock l{ g_lock };
				auto& host = HostFor(a_hostBit);
				open = std::move(host.open);
				targets = std::move(host.targets);
				movie = host.targetMovie;
				host = Host{};
				std::ranges::copy(g_confirms | std::views::keys, std::back_inserter(confirms));
			}
			Hotkeys::CancelCapture();
			ReleaseTargets(a_hostBit, movie, std::move(targets));
			for (const auto id : confirms) {
				ResolveConfirm(id, false);
			}
			std::ranges::for_each(open | std::views::reverse, NotifyClosed);
			ReleaseImages();
			SavedValues::Flush();
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
			if (a_kind == "confirmText") {
				return a_menu.confirmText.get();
			}
			if (a_kind == "helpListBackground") {
				return a_menu.helpListBackground.get();
			}
			return nullptr;
		}

		std::uint32_t AddShaderTarget(std::uint32_t a_hostBit, const Value& a_object, std::string_view a_kind, std::uint32_t a_rgb)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (!host || !a_object.IsObject()) {
				return 0;
			}
			auto&                                    menu = static_cast<RE::StartMenuBase&>(*host);
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
			FixedColorTarget*                        fixed{ nullptr };
			if (a_kind == kColorTargetKind) {
				auto colored = std::make_unique<FixedColorTarget>(a_object, a_rgb);
				fixed = colored.get();
				target = std::move(colored);
			} else if (a_kind == kPopupBackingKind) {
				// Like vanilla panel backgrounds: no color filter on the object, a shaded background behind it.
				target = std::make_unique<GameTarget>(a_object);
				target->EnableShadedBackground(RE::HUDColorTypes::kMenuNoColorBackground);
			} else {
				const auto vanilla = VanillaTarget(menu, a_kind);
				if (!vanilla) {
					REX::ERROR("{} has no {} shader target", HostName(a_hostBit), a_kind);
					return 0;
				}
				target = std::make_unique<GameTarget>(a_object);
				target->CreateAndSetFiltersToHUD(vanilla->HUDColorType.get());
				if (const auto background = vanilla->backgroundColorType.get(); background != RE::HUDColorTypes::kNoColorMultiplier) {
					target->EnableShadedBackground(background);
				}
			}
			menu.shaderFXObjects.push_back(target.get());

			const std::scoped_lock l{ g_lock };
			auto& state = HostFor(a_hostBit);
			state.targetMovie = host->uiMovie.get();
			const auto handle = state.nextTarget++;
			state.targets.push_back({ handle, std::move(target), fixed });
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

		void SetShaderTargetColor(std::uint32_t a_hostBit, std::uint32_t a_handle, std::uint32_t a_rgb)
		{
			const std::scoped_lock l{ g_lock };
			auto&      state = HostFor(a_hostBit);
			const auto it = std::ranges::find(state.targets, a_handle, &ShaderTarget::handle);
			if (it != state.targets.end() && it->fixed) {
				it->fixed->SetColor(a_rgb);
			}
		}

		std::string ColorText(const Item& a_item)
		{
			const auto color = static_cast<std::uint32_t>(a_item.value);
			const auto rgb = std::format("#{:02X}{:02X}{:02X}", (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
			return (a_item.flags & MENUHELPER_FLAG_HASALPHA) ? std::format("{}{:02X}", rgb, color >> 24) : rgb;
		}

		// Steppers show all options. Dropdowns, hotkeys and colors show their value as the only option.
		std::vector<std::string> OptionTexts(const Item& a_item)
		{
			switch (a_item.type) {
				case MENUHELPER_ITEM_STEPPER:
					{
						std::vector<std::string> texts;
						for (const auto& option : a_item.options) {
							texts.emplace_back(Translations::Lookup(a_item.modName, option));
						}
						return texts;
					}
				case MENUHELPER_ITEM_DROPDOWN:
					return { std::string{ Translations::Lookup(a_item.modName, a_item.options[static_cast<std::size_t>(a_item.value)]) } };
				case MENUHELPER_ITEM_HOTKEY:
					return { Hotkeys::KeyName(static_cast<std::uint32_t>(a_item.value)) };
				case MENUHELPER_ITEM_COLOR:
					return { ColorText(a_item) };
				default:
					return {};
			}
		}

		Value BuildItems(Scaleform::GFx::Movie& a_movie, const Menu& a_menu)
		{
			const std::string separator{ Translations::Lookup(kModName, kDecimalSeparatorText) };
			Value             items;
			a_movie.CreateArray(&items);
			for (const auto& item : a_menu.items) {
				if (!item.Visible()) {
					continue;
				}
				const auto        label = GFx::EscapeHtml(Translations::Lookup(item.modName, item.label));
				const std::string description{ Translations::Lookup(item.modName, item.description) };
				const auto        range = item.max - item.min;
				const bool        slider = item.type == MENUHELPER_ITEM_SLIDER;
				const bool        directValue = item.type == MENUHELPER_ITEM_CHECKBOX || item.type == MENUHELPER_ITEM_STEPPER;

				Value obj;
				a_movie.CreateObject(&obj);
				obj.SetMember("itemId", Value(item.id));
				obj.SetMember("itemType", Value(item.type));
				obj.SetMember("text", Value(label.c_str()));
				obj.SetMember("description", Value(description.c_str()));
				obj.SetMember("disabled", Value(!item.enabled));
				obj.SetMember("value", Value(slider ? (item.value - item.min) / range : directValue ? item.value : 0.0));
				obj.SetMember("step", Value(slider ? item.step / range : 0.0));
				if (const auto texts = OptionTexts(item); !texts.empty()) {
					Value options;
					a_movie.CreateArray(&options);
					for (const auto& text : texts) {
						options.PushBack(Value(text.c_str()));
					}
					obj.SetMember("options", options);
				}
				if (item.type == MENUHELPER_ITEM_TOPIC) {
					std::string image = item.image;
					std::ranges::replace(image, '/', '\\');
					obj.SetMember("title", Value(std::string{ Translations::Lookup(item.modName, item.label) }.c_str()));
					obj.SetMember("image", Value(image.c_str()));
					obj.SetMember("imageWidth", Value(item.imageWidth));
					obj.SetMember("imageHeight", Value(item.imageHeight));
				}
				if (slider) {
					const std::string format{ Translations::Lookup(item.modName, item.format.empty() ? kDefaultFormat : item.format) };
					obj.SetMember("minValue", Value(item.min));
					obj.SetMember("maxValue", Value(item.max));
					obj.SetMember("stepValue", Value(item.step));
					obj.SetMember("decimals", Value(item.Decimals()));
					obj.SetMember("format", Value(format.c_str()));
					obj.SetMember("decimalSeparator", Value(separator.c_str()));
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

		std::int32_t SwatchFor(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu)
		{
			MenuHelper_ItemId parent{ MENUHELPER_INVALID_ID };
			{
				const std::scoped_lock l{ g_lock };
				const auto             index = HostIndex(a_hostBit);
				if (g_colorEditors[index] != a_menu) {
					return kNoSwatch;
				}
				parent = g_colorParents[index];
			}
			const auto item = Registry::Get().FindItem(parent);
			return item ? static_cast<std::int32_t>(static_cast<std::uint32_t>(item->value) & 0xFFFFFF) : kNoSwatch;
		}

		void OpenNow(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu, std::int32_t a_selection)
		{
			const auto host = GetHostMenu(a_hostBit);
			if (!host) {
				return;
			}
			{
				const std::scoped_lock l{ g_lock };
				auto& state = HostFor(a_hostBit);
				if (!state.ready) {
					state.pending.push_back({ a_menu, a_selection });
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

			const std::array args{
				Value(a_menu),
				BuildItems(*host->uiMovie, *menu),
				Value(static_cast<std::uint32_t>(menu->GetLayout())),
				Value(menu->popup),
				Value(a_selection),
				Value(menu->HasDefaults()),
				Value(SwatchFor(a_hostBit, a_menu))
			};
			Value result;
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
			const std::array args{
				Value(a_menu),
				BuildItems(*host->uiMovie, *menu),
				Value(static_cast<std::uint32_t>(menu->GetLayout())),
				Value(menu->HasDefaults())
			};
			InvokeUI(*host, "UpdateMenu", args);
		}

		void CommitValue(MenuHelper_ItemId a_item, double a_value, bool a_save = true)
		{
			if (Registry::Get().SetItemValue(a_item, a_value, a_save) == MENUHELPER_INVALID_ID) {
				return;
			}
			if (const auto item = Registry::Get().FindItem(a_item)) {
				NotifyChanged(*item);
			}
		}

		void OnPickerChoice(MenuHelper_ItemId a_choice, double, void* a_user)
		{
			const auto data = reinterpret_cast<std::uintptr_t>(a_user);
			const auto parent = Registry::Get().FindItem(static_cast<MenuHelper_ItemId>(data >> 32));
			const auto index = static_cast<double>(data & 0xFFFFFFFF);
			if (parent && parent->enabled && parent->value != index) {
				CommitValue(parent->id, index);
				RequestRefresh(parent->menu);
			}
			if (const auto choice = Registry::Get().FindItem(a_choice)) {
				RequestClose(choice->menu);
			}
		}

		void OpenPicker(std::uint32_t a_hostBit, const Item& a_item)
		{
			auto& picker = g_pickers[HostIndex(a_hostBit)];
			if (picker == MENUHELPER_INVALID_ID) {
				picker = Registry::Get().CreateInternalMenu(true);
			}
			std::vector<Item> choices;
			for (std::uintptr_t i = 0; i < a_item.options.size(); ++i) {
				Item choice;
				choice.modName = a_item.modName;
				choice.label = a_item.options[i];
				choice.onChange = OnPickerChoice;
				choice.user = reinterpret_cast<void*>((static_cast<std::uintptr_t>(a_item.id) << 32) | i);
				choices.push_back(std::move(choice));
			}
			Registry::Get().ReplaceItems(picker, std::move(choices));
			QueueOpen(a_hostBit, picker, static_cast<std::int32_t>(a_item.value));
		}

		void OnColorChannel(MenuHelper_ItemId, double a_value, void* a_user)
		{
			const auto        shift = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(a_user));
			const auto        hostBit = ActiveHost();
			MenuHelper_ItemId parentId{ MENUHELPER_INVALID_ID };
			MenuHelper_MenuId editor{ MENUHELPER_INVALID_ID };
			{
				const std::scoped_lock l{ g_lock };
				parentId = g_colorParents[HostIndex(hostBit)];
				editor = g_colorEditors[HostIndex(hostBit)];
			}
			const auto parent = Registry::Get().FindItem(parentId);
			if (!parent || !parent->enabled) {
				return;
			}
			auto color = static_cast<std::uint32_t>(parent->value);
			color = (color & ~(0xFFu << shift)) | (static_cast<std::uint32_t>(a_value) << shift);
			// Saved once when the editor closes.
			CommitValue(parentId, color, false);
			RequestRefresh(parent->menu);
			F4SE::GetTaskInterface()->AddUITask([=]() {
				if (const auto host = GetHostMenu(hostBit)) {
					InvokeUI(*host, "SetSwatchColor", std::array{ Value(editor), Value(color & 0xFFFFFF) });
				}
			});
		}

		void OnColorEditorClosed(MenuHelper_MenuId a_editor, void*)
		{
			MenuHelper_ItemId parentId{ MENUHELPER_INVALID_ID };
			{
				const std::scoped_lock l{ g_lock };
				for (std::size_t i = 0; i < g_colorEditors.size(); ++i) {
					if (g_colorEditors[i] == a_editor) {
						parentId = std::exchange(g_colorParents[i], MENUHELPER_INVALID_ID);
					}
				}
			}
			const auto parent = Registry::Get().FindItem(parentId);
			if (parent && !parent->saveKey.empty()) {
				SavedValues::Save(parent->modName, parent->saveKey, parent->value);
			}
		}

		void OpenColorEditor(std::uint32_t a_hostBit, const Item& a_item)
		{
			const auto index = HostIndex(a_hostBit);
			auto&      editor = g_colorEditors[index];
			if (editor == MENUHELPER_INVALID_ID) {
				editor = Registry::Get().CreateInternalMenu(false, OnColorEditorClosed);
			}
			{
				const std::scoped_lock l{ g_lock };
				g_colorParents[index] = a_item.id;
			}
			const auto        color = static_cast<std::uint32_t>(a_item.value);
			const bool        alpha = (a_item.flags & MENUHELPER_FLAG_HASALPHA) != 0;
			std::vector<Item> sliders;
			for (const auto& [label, shift] : kColorChannels) {
				if (shift == 24 && !alpha) {
					continue;
				}
				Item slider;
				slider.type = MENUHELPER_ITEM_SLIDER;
				slider.modName = kModName;
				slider.label = label;
				slider.min = 0.0;
				slider.max = 255.0;
				slider.step = 1.0;
				slider.value = static_cast<double>((color >> shift) & 0xFF);
				slider.onChange = OnColorChannel;
				slider.user = reinterpret_cast<void*>(static_cast<std::uintptr_t>(shift));
				sliders.push_back(std::move(slider));
			}
			Registry::Get().ReplaceItems(editor, std::move(sliders));
			QueueOpen(a_hostBit, editor);
		}

		void StartCapture(std::uint32_t a_hostBit, const Item& a_item)
		{
			const Hotkeys::Capture capture{ a_hostBit, a_item.menu, a_item.id, a_item.flags };
			F4SE::GetTaskInterface()->AddUITask([=]() {
				const auto        host = GetHostMenu(a_hostBit);
				const std::string prompt{ Translations::Lookup(kModName, kHotkeyPromptText) };
				Value             result;
				if (host && InvokeUI(*host, "BeginCapture", std::array{ Value(capture.menu), Value(prompt.c_str()) }, &result) && GFx::IsTrue(result)) {
					Hotkeys::BeginCapture(capture);
				}
			});
		}

		void FinishCapture(RE::IMenu& a_host, const Hotkeys::CaptureResult& a_result)
		{
			InvokeUI(a_host, "EndCapture");
			if (a_result.value) {
				CommitValue(a_result.capture.item, *a_result.value);
				RequestRefresh(a_result.capture.menu);
			}
		}

		void ResetMenu(MenuHelper_MenuId a_menu)
		{
			const auto menu = Registry::Get().FindMenu(a_menu);
			if (!menu) {
				return;
			}
			for (const auto& item : menu->items) {
				if (item.Visible() && item.HasValue() && item.value != item.defaultValue) {
					CommitValue(item.id, item.defaultValue);
				}
			}
			RequestRefresh(a_menu);
			if (menu->onReset) {
				menu->onReset(a_menu, menu->user);
			}
		}

		void OnResetRequest(MenuHelper_MenuId a_menu)
		{
			const auto menu = Registry::Get().FindMenu(a_menu);
			if (menu && menu->HasDefaults()) {
				RequestConfirm(Dialogs::NextId(), kResetPrompt, [a_menu](bool a_confirmed) {
					if (a_confirmed) {
						ResetMenu(a_menu);
					}
				});
			}
		}

		void OnItemPress(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu, MenuHelper_ItemId a_item)
		{
			const auto item = Registry::Get().FindItem(a_item);
			if (!item || item->menu != a_menu || !item->enabled) {
				return;
			}
			switch (item->type) {
				case MENUHELPER_ITEM_BUTTON:
					if (item->onChange) {
						item->onChange(item->id, 0.0, item->user);
					}
					JsonMenus::RunAction(*item);
					if (item->submenu != MENUHELPER_INVALID_ID) {
						QueueOpen(a_hostBit, item->submenu);
					}
					break;
				case MENUHELPER_ITEM_DROPDOWN:
					OpenPicker(a_hostBit, *item);
					break;
				case MENUHELPER_ITEM_HOTKEY:
					StartCapture(a_hostBit, *item);
					break;
				case MENUHELPER_ITEM_COLOR:
					OpenColorEditor(a_hostBit, *item);
					break;
				default:
					break;
			}
		}

		void OnValueChange(MenuHelper_MenuId a_menu, MenuHelper_ItemId a_item, double a_raw)
		{
			const auto item = Registry::Get().FindItem(a_item);
			if (!item || item->menu != a_menu || !item->enabled) {
				return;
			}
			switch (item->type) {
				case MENUHELPER_ITEM_CHECKBOX:
				case MENUHELPER_ITEM_STEPPER:
					CommitValue(a_item, a_raw);
					break;
				case MENUHELPER_ITEM_SLIDER:
					CommitValue(a_item, item->min + a_raw * (item->max - item->min));
					break;
				default:
					break;
			}
		}

		void OnItemSelect(MenuHelper_MenuId a_menu, MenuHelper_ItemId a_item)
		{
			const auto item = Registry::Get().FindItem(a_item);
			if (item && item->menu == a_menu && item->onSelect) {
				item->onSelect(item->id, item->value, item->user);
			}
		}

		void OnReady(std::uint32_t a_hostBit)
		{
			REX::INFO("submenu template ready in {}", HostName(a_hostBit));
			std::vector<PendingOpen> pending;
			{
				const std::scoped_lock l{ g_lock };
				auto& host = HostFor(a_hostBit);
				host.ready = true;
				pending = std::move(host.pending);
			}
			for (const auto& [menu, selection] : pending) {
				QueueOpen(a_hostBit, menu, selection);
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
			Hotkeys::CancelCapture(a_menu);
			NotifyClosed(a_menu);
			SavedValues::Flush();
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

		std::string_view ArgString(const Scaleform::GFx::FunctionHandler::Params& a_params, std::uint32_t a_index)
		{
			return a_params.argCount > a_index && a_params.args[a_index].IsString() ? a_params.args[a_index].GetString() : "";
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
				const auto third = ArgId(a_params, 2);

				switch (static_cast<Function>(data & 0xFF)) {
					case Function::kReady:
						OnReady(hostBit);
						break;
					case Function::kTemplateError:
						REX::ERROR("submenu template cannot run in {}: missing {}", HostName(hostBit), ArgString(a_params, 0));
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
					case Function::kItemSelect:
						if (first && second) {
							OnItemSelect(*first, *second);
						}
						break;
					case Function::kResetRequest:
						if (first) {
							OnResetRequest(*first);
						}
						break;
					case Function::kConfirmResult:
						if (first && a_params.argCount > 1) {
							ResolveConfirm(*first, GFx::IsTrue(a_params.args[1]));
						}
						break;
					case Function::kGetButtonHintData:
						if (a_params.retVal) {
							GetButtonHintData(hostBit, *a_params.retVal);
						}
						break;
					case Function::kAddShaderTarget:
						if (a_params.retVal && a_params.argCount > 1 && a_params.args[1].IsString()) {
							*a_params.retVal = Value(AddShaderTarget(hostBit, a_params.args[0], a_params.args[1].GetString(), third.value_or(0)));
						}
						break;
					case Function::kRemoveShaderTarget:
						if (first) {
							RemoveShaderTarget(hostBit, *first);
						}
						break;
					case Function::kSetShaderTargetColor:
						if (first && second) {
							SetShaderTargetColor(hostBit, *first, *second);
						}
						break;
					case Function::kInjectMainEntries:
						if (const auto host = GetHostMenu(hostBit)) {
							Injector::InjectEntries(static_cast<RE::StartMenuBase&>(*host), hostBit);
						}
						break;
					case Function::kPrepareImage:
						if (a_params.retVal) {
							*a_params.retVal = Value(PrepareImage(ArgString(a_params, 0)));
						}
						break;
					case Function::kLog:
						REX::WARN("{}: {}", HostName(hostBit), ArgString(a_params, 0));
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
			static HostCloseSink  sink;
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
		if (Hotkeys::IsCapturing()) {
			if (const auto result = Hotkeys::Feed(a_event)) {
				FinishCapture(a_host, *result);
			}
			return true;
		}
		const std::array args{ Value(a_event.QUserEvent().c_str()), Value(a_event.QReleased()), Value(a_event.QJustPressed()) };
		Value            result;
		return InvokeUI(a_host, "ProcessUserEvent", args, &result) && GFx::IsTrue(result);
	}

	void QueueOpen(std::uint32_t a_hostBit, MenuHelper_MenuId a_menu, std::int32_t a_selection)
	{
		F4SE::GetTaskInterface()->AddUITask([=]() { OpenNow(a_hostBit, a_menu, a_selection); });
	}

	void NotifyChanged(const Item& a_item)
	{
		if (a_item.onChange) {
			a_item.onChange(a_item.id, a_item.value, a_item.user);
		}
		if (!a_item.saveKey.empty()) {
			JsonMenus::NotifySetting(a_item.modName, a_item.saveKey, a_item.value);
		}
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

	bool RequestConfirm(MenuHelper_BoxId a_id, std::string a_text, std::function<void(bool)> a_done)
	{
		const auto hostBit = ActiveHost();
		{
			const std::scoped_lock l{ g_lock };
			if (hostBit == 0 || HostFor(hostBit).open.empty()) {
				return false;
			}
			g_confirms.emplace(a_id, std::move(a_done));
		}
		F4SE::GetTaskInterface()->AddUITask([hostBit, a_id, text = std::move(a_text)]() {
			const auto host = GetHostMenu(hostBit);
			Value      result;
			if (!host || !InvokeUI(*host, "ShowConfirm", std::array{ Value(a_id), Value(text.c_str()) }, &result) || !GFx::IsTrue(result)) {
				ResolveConfirm(a_id, false);
			}
		});
		return true;
	}
}
