#include "JsonMenus.h"

#include "Injector.h"
#include "Papyrus.h"
#include "Registry.h"
#include "SavedValues.h"
#include "Translations.h"

#include <glaze/glaze.hpp>

namespace MenuHelper::JsonMenus
{
	// glaze reflection needs external linkage.
	namespace Schema
	{
		struct JsonItem
		{
			std::string                                            type;
			std::string                                            label;
			std::optional<std::string>                             description;
			std::optional<std::string>                             text;
			std::optional<std::string>                             saveKey;
			std::optional<std::variant<bool, double, std::string>> defaultValue;
			std::optional<double>                                  min;
			std::optional<double>                                  max;
			std::optional<double>                                  step;
			std::optional<std::string>                             format;
			std::optional<std::uint32_t>                           decimals;
			std::optional<std::vector<std::string>>                options;
			bool                                                   modifiers{ false };
			bool                                                   mouse{ false };
			bool                                                   gamepad{ false };
			bool                                                   alpha{ false };
			std::optional<std::string>                             action;
			std::optional<std::string>                             papyrus;
			std::optional<std::string>                             menu;
			std::optional<std::string>                             image;
			std::uint32_t                                          imageWidth{ 0 };
			std::uint32_t                                          imageHeight{ 0 };

			struct glaze
			{
				using T = JsonItem;
				static constexpr auto value = glz::object(
					"type", &T::type, "label", &T::label, "description", &T::description, "text", &T::text,
					"saveKey", &T::saveKey, "default", &T::defaultValue, "min", &T::min, "max", &T::max, "step", &T::step,
					"format", &T::format, "decimals", &T::decimals, "options", &T::options, "modifiers", &T::modifiers,
					"mouse", &T::mouse, "gamepad", &T::gamepad, "alpha", &T::alpha, "action", &T::action,
					"papyrus", &T::papyrus, "menu", &T::menu, "image", &T::image, "imageWidth", &T::imageWidth,
					"imageHeight", &T::imageHeight);
			};
		};

		struct JsonMenu
		{
			std::string           kind{ "items" };
			std::vector<JsonItem> items;
		};

		struct JsonEntry
		{
			std::string                                         label;
			std::vector<std::string>                            menus{ "main", "pause" };
			std::optional<std::variant<std::int32_t, std::string>> insertBefore;
			std::optional<std::string>                          menu;
		};

		struct JsonConfig
		{
			std::int32_t                    version{ 0 };
			std::vector<JsonEntry>          entries;
			std::map<std::string, JsonMenu> menus;
		};
	}

	namespace
	{
		using namespace Schema;

		constexpr auto         kDirectory = "Data/MenuHelper/Config"sv;
		constexpr auto         kFileName = "menu.json"sv;
		constexpr auto         kUtf8Bom = "\xEF\xBB\xBF"sv;
		constexpr std::int32_t kVersion = 1;
		constexpr std::int32_t kDefaultInsertBefore = MENUHELPER_VANILLA_QUIT;
		constexpr std::uint32_t kOpaque = 0xFF000000;

		constexpr std::array kItemTypes{
			std::pair{ "button"sv, MENUHELPER_ITEM_BUTTON },
			std::pair{ "checkbox"sv, MENUHELPER_ITEM_CHECKBOX },
			std::pair{ "stepper"sv, MENUHELPER_ITEM_STEPPER },
			std::pair{ "slider"sv, MENUHELPER_ITEM_SLIDER },
			std::pair{ "dropdown"sv, MENUHELPER_ITEM_DROPDOWN },
			std::pair{ "hotkey"sv, MENUHELPER_ITEM_HOTKEY },
			std::pair{ "color"sv, MENUHELPER_ITEM_COLOR },
			std::pair{ "header"sv, MENUHELPER_ITEM_HEADER },
			std::pair{ "text"sv, MENUHELPER_ITEM_TEXT },
			std::pair{ "topic"sv, MENUHELPER_ITEM_TOPIC },
		};

		constexpr std::array kVanillaEntries{
			std::pair{ "append"sv, MENUHELPER_VANILLA_APPEND },
			std::pair{ "continue"sv, MENUHELPER_VANILLA_CONTINUE },
			std::pair{ "new"sv, MENUHELPER_VANILLA_NEW },
			std::pair{ "quicksave"sv, MENUHELPER_VANILLA_QUICKSAVE },
			std::pair{ "save"sv, MENUHELPER_VANILLA_SAVE },
			std::pair{ "load"sv, MENUHELPER_VANILLA_LOAD },
			std::pair{ "dlc"sv, MENUHELPER_VANILLA_DLC },
			std::pair{ "bethesdanet"sv, MENUHELPER_VANILLA_BETHESDANET },
			std::pair{ "help"sv, MENUHELPER_VANILLA_HELP },
			std::pair{ "settings"sv, MENUHELPER_VANILLA_SETTINGS },
			std::pair{ "credits"sv, MENUHELPER_VANILLA_CREDITS },
			std::pair{ "quit"sv, MENUHELPER_VANILLA_QUIT },
			std::pair{ "creationclub"sv, MENUHELPER_VANILLA_CREATIONCLUB },
			std::pair{ "transfersave"sv, MENUHELPER_VANILLA_TRANSFERSAVE },
			std::pair{ "installed_content"sv, MENUHELPER_VANILLA_INSTALLED_CONTENT },
		};

		template <class Callback>
		struct Listener
		{
			std::string modName;
			Callback    callback;
			void*       user;
		};

		std::mutex                                             g_lock;
		std::vector<Listener<MenuHelper_SettingCallback>>      g_settingListeners;
		std::vector<Listener<MenuHelper_ActionCallback>>       g_actionListeners;

		class Problems
		{
		public:
			explicit Problems(std::string a_file) :
				_file(std::move(a_file))
			{}

			void Add(std::string_view a_field, std::string_view a_message)
			{
				REX::ERROR("{}: {}: {}", _file, a_field, a_message);
				_ok = false;
			}

			bool Ok() const { return _ok; }

		private:
			std::string _file;
			bool        _ok{ true };
		};

		std::optional<std::uint32_t> ParseColor(std::string_view a_text)
		{
			if (!a_text.starts_with('#') || (a_text.size() != 7 && a_text.size() != 9)) {
				return std::nullopt;
			}
			std::uint32_t value{ 0 };
			const auto [end, error] = std::from_chars(a_text.data() + 1, a_text.data() + a_text.size(), value, 16);
			if (error != std::errc{} || end != a_text.data() + a_text.size()) {
				return std::nullopt;
			}
			return a_text.size() == 7 ? kOpaque | value : (value >> 8) | (value << 24);
		}

		std::optional<double> DefaultValue(const JsonItem& a_json, std::uint32_t a_type, Problems& a_problems, const std::string& a_field)
		{
			if (!a_json.defaultValue) {
				return a_type == MENUHELPER_ITEM_COLOR ? std::optional{ static_cast<double>(kOpaque) } : std::optional{ 0.0 };
			}
			const auto& value = *a_json.defaultValue;
			if (const auto* flag = std::get_if<bool>(&value); flag && a_type == MENUHELPER_ITEM_CHECKBOX) {
				return *flag ? 1.0 : 0.0;
			}
			if (const auto* number = std::get_if<double>(&value); number && a_type != MENUHELPER_ITEM_CHECKBOX) {
				return *number;
			}
			if (const auto* text = std::get_if<std::string>(&value); text && a_type == MENUHELPER_ITEM_COLOR) {
				if (const auto color = ParseColor(*text)) {
					return static_cast<double>(*color);
				}
			}
			a_problems.Add(a_field + ".default", "checkboxes need true or false, colors \"#RRGGBB\" or \"#RRGGBBAA\", other items a number");
			return std::nullopt;
		}

		std::optional<Item> MakeItem(const JsonItem& a_json, std::uint32_t a_menuKind, const std::map<std::string, JsonMenu>& a_menus, Problems& a_problems, const std::string& a_field)
		{
			const auto type = std::ranges::find(kItemTypes, std::string_view{ a_json.type }, &std::pair<std::string_view, MenuHelper_ItemType>::first);
			if (type == kItemTypes.end()) {
				a_problems.Add(a_field + ".type", std::format("unknown type \"{}\"", a_json.type));
				return std::nullopt;
			}
			MenuHelper_ItemDesc desc{};
			desc.structSize = sizeof(desc);
			desc.type = type->second;
			const bool topic = desc.type == MENUHELPER_ITEM_TOPIC;
			const bool valueItem = desc.type >= MENUHELPER_ITEM_CHECKBOX && desc.type <= MENUHELPER_ITEM_COLOR;
			if (topic != (a_menuKind == MENUHELPER_MENU_PAGES)) {
				a_problems.Add(a_field + ".type", "pages menus hold topics only, and topics need a pages menu");
			}
			if (valueItem && !a_json.saveKey) {
				a_problems.Add(a_field + ".saveKey", "value items need a save key");
			}
			if (a_json.saveKey && (!valueItem || !SavedValues::IsValidKey(*a_json.saveKey))) {
				a_problems.Add(a_field + ".saveKey", "only value items have a save key, and it must be a valid INI key");
			}
			if (desc.type == MENUHELPER_ITEM_SLIDER && (!a_json.min || !a_json.max || !a_json.step)) {
				a_problems.Add(a_field, "sliders need min, max and step");
			}
			if ((desc.type == MENUHELPER_ITEM_STEPPER || desc.type == MENUHELPER_ITEM_DROPDOWN) && (!a_json.options || a_json.options->empty())) {
				a_problems.Add(a_field + ".options", "steppers and dropdowns need at least one option");
			}
			if (a_json.menu && (desc.type != MENUHELPER_ITEM_BUTTON || !a_menus.contains(*a_json.menu))) {
				a_problems.Add(a_field + ".menu", "only buttons open menus, and the menu must be defined in this file");
			}
			if (a_json.papyrus) {
				const auto dot = a_json.papyrus->find('.');
				if (dot == 0 || dot == std::string::npos || dot + 1 == a_json.papyrus->size() || a_json.papyrus->find('.', dot + 1) != std::string::npos) {
					a_problems.Add(a_field + ".papyrus", "must be \"Script.Function\"");
				}
			}
			const auto value = valueItem ? DefaultValue(a_json, desc.type, a_problems, a_field) : std::optional{ 0.0 };
			if (!a_problems.Ok() || !value) {
				return std::nullopt;
			}

			std::vector<const char*> options;
			if (a_json.options) {
				std::ranges::transform(*a_json.options, std::back_inserter(options), &std::string::c_str);
			}
			const auto* text = topic && a_json.text ? &*a_json.text : a_json.description ? &*a_json.description : nullptr;
			desc.label = a_json.label.c_str();
			desc.description = text ? text->c_str() : nullptr;
			desc.flags = (a_json.modifiers ? MENUHELPER_FLAG_HOTKEY_MODIFIERS : 0) | (a_json.mouse ? MENUHELPER_FLAG_HOTKEY_MOUSE : 0) |
			             (a_json.gamepad ? MENUHELPER_FLAG_HOTKEY_GAMEPAD : 0) | (a_json.alpha ? MENUHELPER_FLAG_HASALPHA : 0);
			desc.value = *value;
			desc.min = a_json.min.value_or(0.0);
			desc.max = a_json.max.value_or(0.0);
			desc.step = a_json.step.value_or(0.0);
			desc.format = a_json.format ? a_json.format->c_str() : nullptr;
			desc.decimals = a_json.decimals.value_or(0);
			desc.options = options.data();
			desc.optionCount = static_cast<std::uint32_t>(options.size());
			desc.saveKey = a_json.saveKey ? a_json.saveKey->c_str() : nullptr;
			desc.image = a_json.image ? a_json.image->c_str() : nullptr;
			desc.imageWidth = a_json.imageWidth;
			desc.imageHeight = a_json.imageHeight;
			auto item = Registry::MakeItem(desc);
			if (!item) {
				a_problems.Add(a_field, "invalid item; see the warning above");
				return std::nullopt;
			}
			item->action = a_json.action.value_or("");
			item->papyrusFunction = a_json.papyrus.value_or("");
			return item;
		}

		std::optional<std::int32_t> InsertBefore(const JsonEntry& a_entry, Problems& a_problems, const std::string& a_field)
		{
			if (!a_entry.insertBefore) {
				return kDefaultInsertBefore;
			}
			if (const auto* index = std::get_if<std::int32_t>(&*a_entry.insertBefore)) {
				return *index;
			}
			const auto& name = std::get<std::string>(*a_entry.insertBefore);
			const auto  it = std::ranges::find(kVanillaEntries, std::string_view{ name }, &std::pair<std::string_view, MenuHelper_VanillaEntry>::first);
			if (it == kVanillaEntries.end()) {
				a_problems.Add(a_field + ".insertBefore", std::format("unknown vanilla entry \"{}\"", name));
				return std::nullopt;
			}
			return it->second;
		}

		std::uint32_t MenuBits(const JsonEntry& a_entry, Problems& a_problems, const std::string& a_field)
		{
			std::uint32_t bits = 0;
			for (const auto& name : a_entry.menus) {
				if (name == "main") {
					bits |= MENUHELPER_MENU_MAIN;
				} else if (name == "pause") {
					bits |= MENUHELPER_MENU_PAUSE;
				} else {
					a_problems.Add(a_field + ".menus", std::format("\"{}\" is not \"main\" or \"pause\"", name));
				}
			}
			if (bits == 0) {
				a_problems.Add(a_field + ".menus", "needs \"main\", \"pause\" or both");
			}
			return bits;
		}

		struct Prototype
		{
			std::string menu;
			Item        item;
			std::string submenu;
		};

		void LoadFile(const std::string& a_modName, const std::filesystem::path& a_path)
		{
			const auto display = std::format("{}/{}/{}", kDirectory, a_modName, kFileName);
			std::ifstream file(a_path, std::ios::binary);
			std::string   buffer{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			if (buffer.starts_with(kUtf8Bom)) {
				buffer.erase(0, kUtf8Bom.size());
			}
			JsonConfig config;
			if (const auto error = glz::read<glz::opts{ .comments = true }>(config, buffer)) {
				REX::ERROR("{}: {}; the file is skipped", display, glz::format_error(error, buffer));
				return;
			}

			Problems problems{ display };
			if (config.version != kVersion) {
				problems.Add("version", std::format("must be {}", kVersion));
			}
			std::vector<std::pair<std::string, std::uint32_t>> menus;
			std::vector<Prototype>                             items;
			for (const auto& [name, menu] : config.menus) {
				const auto field = std::format("menus.{}", name);
				const auto kind = menu.kind == "pages" ? MENUHELPER_MENU_PAGES : MENUHELPER_MENU_ITEMS;
				if (menu.kind != "items" && menu.kind != "pages") {
					problems.Add(field + ".kind", "must be \"items\" or \"pages\"");
				}
				menus.emplace_back(name, kind);
				for (std::size_t i = 0; i < menu.items.size(); ++i) {
					const auto& json = menu.items[i];
					if (auto item = MakeItem(json, kind, config.menus, problems, std::format("{}.items[{}]", field, i))) {
						items.push_back({ name, std::move(*item), json.menu.value_or("") });
					}
				}
			}
			std::vector<std::pair<MenuHelper_EntryDesc, std::string>> entries;
			for (std::size_t i = 0; i < config.entries.size(); ++i) {
				const auto& json = config.entries[i];
				const auto  field = std::format("entries[{}]", i);
				if (json.menu && !config.menus.contains(*json.menu)) {
					problems.Add(field + ".menu", std::format("menu \"{}\" is not defined in this file", *json.menu));
				}
				MenuHelper_EntryDesc desc{};
				desc.structSize = sizeof(desc);
				desc.label = json.label.c_str();
				desc.menus = MenuBits(json, problems, field);
				desc.insertBefore = InsertBefore(json, problems, field).value_or(kDefaultInsertBefore);
				entries.emplace_back(desc, json.menu.value_or(""));
			}
			if (!problems.Ok()) {
				REX::ERROR("{}: the file is skipped", display);
				return;
			}

			auto&                                    registry = Registry::Get();
			std::map<std::string, MenuHelper_MenuId> ids;
			for (const auto& [name, kind] : menus) {
				MenuHelper_MenuDesc desc{};
				desc.structSize = sizeof(desc);
				desc.modName = a_modName.c_str();
				desc.kind = kind;
				ids[name] = registry.CreateMenu(desc);
			}
			for (auto& [menu, item, submenu] : items) {
				item.submenu = submenu.empty() ? MENUHELPER_INVALID_ID : ids[submenu];
				if (registry.AddItem(ids[menu], std::move(item)) == MENUHELPER_INVALID_ID) {
					REX::ERROR("{}: an item of menu {} could not be added", display, menu);
				}
			}
			for (auto& [desc, menu] : entries) {
				desc.modName = a_modName.c_str();
				desc.submenu = menu.empty() ? MENUHELPER_INVALID_ID : ids[menu];
				registry.AddEntry(desc);
			}
			REX::INFO("loaded {}: {} menus, {} items, {} entries", display, menus.size(), items.size(), entries.size());
		}

		void CallPapyrus(const Item& a_item)
		{
			const auto ui = RE::UI::GetSingleton();
			if (ui && ui->GetMenuOpen(RE::MainMenu::MENU_NAME)) {
				static std::once_flag logged;
				std::call_once(logged, [] { REX::INFO("Papyrus functions of JSON menus are not called at the main menu; no game is loaded"); });
				return;
			}
			const auto dot = a_item.papyrusFunction.find('.');
			const auto script = a_item.papyrusFunction.substr(0, dot);
			const auto function = a_item.papyrusFunction.substr(dot + 1);
			if (!Papyrus::CallGlobal(script, function, a_item.modName, a_item.action)) {
				REX::ERROR("{}: could not call global Papyrus function {} for action \"{}\"", a_item.modName, a_item.papyrusFunction, a_item.action);
			}
		}
	}

	void Load()
	{
		std::error_code error;
		if (!std::filesystem::is_directory(kDirectory, error)) {
			return;
		}
		bool loaded = false;
		for (const auto& directory : std::filesystem::directory_iterator(kDirectory, error)) {
			if (!directory.is_directory(error)) {
				continue;
			}
			const auto u8name = directory.path().filename().u8string();
			const std::string modName{ reinterpret_cast<const char*>(u8name.data()), u8name.size() };
			const auto        path = directory.path() / kFileName;
			if (!std::filesystem::is_regular_file(path, error)) {
				continue;
			}
			if (!Translations::IsValidModName(modName)) {
				REX::ERROR("{}/{}: the folder name is not a valid mod name; skipped", kDirectory, modName);
				continue;
			}
			LoadFile(modName, path);
			loaded = true;
		}
		if (loaded) {
			Injector::RefreshOpenMenus();
		}
	}

	bool RegisterSettingListener(std::string_view a_modName, MenuHelper_SettingCallback a_callback, void* a_user)
	{
		const std::scoped_lock l{ g_lock };
		g_settingListeners.push_back({ std::string{ a_modName }, a_callback, a_user });
		return true;
	}

	bool RegisterActionListener(std::string_view a_modName, MenuHelper_ActionCallback a_callback, void* a_user)
	{
		const std::scoped_lock l{ g_lock };
		g_actionListeners.push_back({ std::string{ a_modName }, a_callback, a_user });
		return true;
	}

	void NotifySetting(std::string_view a_modName, std::string_view a_saveKey, double a_value)
	{
		std::vector<Listener<MenuHelper_SettingCallback>> listeners;
		{
			const std::scoped_lock l{ g_lock };
			std::ranges::copy_if(g_settingListeners, std::back_inserter(listeners), [&](const auto& a_listener) { return a_listener.modName == a_modName; });
		}
		const std::string modName{ a_modName };
		const std::string saveKey{ a_saveKey };
		for (const auto& listener : listeners) {
			listener.callback(modName.c_str(), saveKey.c_str(), a_value, listener.user);
		}
		Papyrus::SendSettingChange(modName, saveKey, a_value);
	}

	void RunAction(const Item& a_item)
	{
		if (!a_item.action.empty()) {
			std::vector<Listener<MenuHelper_ActionCallback>> listeners;
			{
				const std::scoped_lock l{ g_lock };
				std::ranges::copy_if(g_actionListeners, std::back_inserter(listeners), [&](const auto& a_listener) { return a_listener.modName == a_item.modName; });
			}
			for (const auto& listener : listeners) {
				listener.callback(a_item.modName.c_str(), a_item.action.c_str(), listener.user);
			}
			Papyrus::SendAction(a_item.modName, a_item.action);
		}
		if (!a_item.papyrusFunction.empty()) {
			CallPapyrus(a_item);
		}
	}
}
