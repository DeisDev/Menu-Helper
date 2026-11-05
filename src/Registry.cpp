#include "Registry.h"

#include "SavedValues.h"
#include "Translations.h"

namespace MenuHelper
{
	namespace
	{
		constexpr const char*   kInternalModName = "MenuHelper";
		constexpr std::uint32_t kMaxAutoDecimals = 6;

		bool IsValidModName(const char* a_modName)
		{
			if (a_modName && Translations::IsValidModName(a_modName)) {
				return true;
			}
			REX::WARN("rejected mod name \"{}\": it must be a valid file name", a_modName ? a_modName : "");
			return false;
		}

		bool IsValueType(std::uint32_t a_type)
		{
			return a_type >= MENUHELPER_ITEM_CHECKBOX && a_type <= MENUHELPER_ITEM_COLOR;
		}

		bool HasOptions(const MenuHelper_ItemDesc& a_desc)
		{
			return a_desc.options && a_desc.optionCount > 0 &&
			       std::all_of(a_desc.options, a_desc.options + a_desc.optionCount, [](const char* a_option) { return a_option != nullptr; });
		}

		bool IsValidItem(const MenuHelper_ItemDesc& a_desc)
		{
			if (!a_desc.label) {
				REX::WARN("rejected item: it has no label");
				return false;
			}
			if (a_desc.saveKey && (!IsValueType(a_desc.type) || !SavedValues::IsValidKey(a_desc.saveKey))) {
				REX::WARN("rejected item \"{}\": only value items can have a save key, and save keys must be valid INI keys", a_desc.label);
				return false;
			}
			switch (a_desc.type) {
				case MENUHELPER_ITEM_BUTTON:
				case MENUHELPER_ITEM_CHECKBOX:
				case MENUHELPER_ITEM_HOTKEY:
				case MENUHELPER_ITEM_COLOR:
				case MENUHELPER_ITEM_HEADER:
				case MENUHELPER_ITEM_TEXT:
				case MENUHELPER_ITEM_TOPIC:
					return true;
				case MENUHELPER_ITEM_STEPPER:
				case MENUHELPER_ITEM_DROPDOWN:
					if (!HasOptions(a_desc)) {
						REX::WARN("rejected item \"{}\": it needs at least one option", a_desc.label);
						return false;
					}
					return true;
				case MENUHELPER_ITEM_SLIDER:
					if (!std::isfinite(a_desc.min) || !std::isfinite(a_desc.max) || !std::isfinite(a_desc.step) ||
						a_desc.max <= a_desc.min || a_desc.step <= 0.0) {
						REX::WARN("rejected slider \"{}\": max must be greater than min and step greater than 0", a_desc.label);
						return false;
					}
					return true;
				default:
					REX::WARN("rejected item \"{}\": unknown type {}", a_desc.label, a_desc.type);
					return false;
			}
		}

		bool IsWhole(double a_value)
		{
			return std::abs(a_value - std::round(a_value)) < 1e-6;
		}
	}

	double Item::Clamp(double a_value) const
	{
		if (!std::isfinite(a_value)) {
			a_value = 0.0;
		}
		switch (type) {
			case MENUHELPER_ITEM_CHECKBOX:
				return a_value != 0.0 ? 1.0 : 0.0;
			case MENUHELPER_ITEM_STEPPER:
			case MENUHELPER_ITEM_DROPDOWN:
				return std::clamp(std::round(a_value), 0.0, static_cast<double>(options.size() - 1));
			case MENUHELPER_ITEM_SLIDER:
				{
					const auto steps = std::round((std::clamp(a_value, min, max) - min) / step);
					return std::min(min + steps * step, max);
				}
			case MENUHELPER_ITEM_HOTKEY:
			case MENUHELPER_ITEM_COLOR:
				return std::clamp(std::round(a_value), 0.0, static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
			default:
				return 0.0;
		}
	}

	bool Item::HasValue() const
	{
		return IsValueType(type);
	}

	std::uint32_t Item::Decimals() const
	{
		if (decimals > 0) {
			return decimals;
		}
		double scale = 1.0;
		for (std::uint32_t digits = 0; digits < kMaxAutoDecimals; ++digits, scale *= 10.0) {
			if (IsWhole(step * scale) && IsWhole(min * scale)) {
				return digits;
			}
		}
		return kMaxAutoDecimals;
	}

	Layout Menu::GetLayout() const
	{
		if (kind == MENUHELPER_MENU_PAGES) {
			return Layout::kPages;
		}
		const bool options = std::ranges::any_of(items, [](const Item& a_item) {
			if (!a_item.Visible() || a_item.type == MENUHELPER_ITEM_HEADER || a_item.type == MENUHELPER_ITEM_TEXT) {
				return false;
			}
			return a_item.type != MENUHELPER_ITEM_BUTTON || !a_item.description.empty();
		});
		return options ? Layout::kOptions : Layout::kList;
	}

	bool Menu::HasDefaults() const
	{
		return !internal && std::ranges::any_of(items, [](const Item& a_item) { return a_item.Visible() && a_item.HasValue(); });
	}

	Registry& Registry::Get()
	{
		static Registry singleton;
		return singleton;
	}

	std::optional<Item> Registry::MakeItem(const MenuHelper_ItemDesc& a_desc)
	{
		if (!IsValidItem(a_desc)) {
			return std::nullopt;
		}
		Item item;
		item.type = a_desc.type;
		item.flags = a_desc.flags;
		item.label = a_desc.label;
		item.description = a_desc.description ? a_desc.description : "";
		item.min = a_desc.min;
		item.max = a_desc.max;
		item.step = a_desc.step;
		if (a_desc.type == MENUHELPER_ITEM_SLIDER) {
			item.format = a_desc.format ? a_desc.format : "";
			item.decimals = a_desc.decimals;
		}
		if (a_desc.type == MENUHELPER_ITEM_STEPPER || a_desc.type == MENUHELPER_ITEM_DROPDOWN) {
			item.options.assign(a_desc.options, a_desc.options + a_desc.optionCount);
		}
		item.value = a_desc.value;
		item.defaultValue = a_desc.value;
		item.saveKey = a_desc.saveKey ? a_desc.saveKey : "";
		item.submenu = a_desc.type == MENUHELPER_ITEM_BUTTON ? a_desc.submenu : MENUHELPER_INVALID_ID;
		item.onChange = a_desc.onChange;
		item.onSelect = a_desc.onSelect;
		item.onHotkey = a_desc.type == MENUHELPER_ITEM_HOTKEY ? a_desc.onHotkey : nullptr;
		item.user = a_desc.user;
		if (a_desc.type == MENUHELPER_ITEM_TOPIC && a_desc.image) {
			item.image = a_desc.image;
			item.imageWidth = a_desc.imageWidth;
			item.imageHeight = a_desc.imageHeight;
		}
		return item;
	}

	MenuHelper_EntryId Registry::AddEntry(const MenuHelper_EntryDesc& a_desc)
	{
		if (!a_desc.label || (a_desc.menus & MENUHELPER_MENU_ALL) == 0 || !IsValidModName(a_desc.modName)) {
			return MENUHELPER_INVALID_ID;
		}
		const std::scoped_lock l{ _lock };
		Entry entry;
		entry.id = _nextId++;
		entry.modName = a_desc.modName;
		entry.label = a_desc.label;
		Translations::AddToTemplate(entry.modName, entry.label);
		entry.menus = a_desc.menus & MENUHELPER_MENU_ALL;
		entry.insertBefore = a_desc.insertBefore;
		entry.onPress = a_desc.onPress;
		entry.user = a_desc.user;
		entry.submenu = a_desc.submenu;
		_entries.push_back(entry);
		return entry.id;
	}

	bool Registry::RemoveEntry(MenuHelper_EntryId a_id)
	{
		const std::scoped_lock l{ _lock };
		return std::erase_if(_entries, [&](const Entry& a_entry) { return a_entry.id == a_id; }) > 0;
	}

	bool Registry::SetEntryLabel(MenuHelper_EntryId a_id, std::string_view a_label)
	{
		const std::scoped_lock l{ _lock };
		auto* entry = FindEntryUnlocked(a_id);
		if (!entry) {
			return false;
		}
		entry->label = a_label;
		Translations::AddToTemplate(entry->modName, entry->label);
		return true;
	}

	bool Registry::SetEntryEnabled(MenuHelper_EntryId a_id, bool a_enabled)
	{
		const std::scoped_lock l{ _lock };
		auto* entry = FindEntryUnlocked(a_id);
		if (!entry) {
			return false;
		}
		entry->enabled = a_enabled;
		return true;
	}

	std::vector<Entry> Registry::Entries(std::uint32_t a_menu) const
	{
		const std::scoped_lock l{ _lock };
		std::vector<Entry> result;
		std::ranges::copy_if(_entries, std::back_inserter(result), [&](const Entry& a_entry) { return (a_entry.menus & a_menu) != 0; });
		return result;
	}

	std::optional<Entry> Registry::FindEntry(MenuHelper_EntryId a_id) const
	{
		const std::scoped_lock l{ _lock };
		const auto it = std::ranges::find(_entries, a_id, &Entry::id);
		return it != _entries.end() ? std::optional{ *it } : std::nullopt;
	}

	MenuHelper_MenuId Registry::CreateMenu(const MenuHelper_MenuDesc& a_desc)
	{
		if (!IsValidModName(a_desc.modName)) {
			return MENUHELPER_INVALID_ID;
		}
		if (a_desc.kind != MENUHELPER_MENU_ITEMS && a_desc.kind != MENUHELPER_MENU_PAGES) {
			REX::WARN("rejected menu of {}: unknown kind {}", a_desc.modName, a_desc.kind);
			return MENUHELPER_INVALID_ID;
		}
		const std::scoped_lock l{ _lock };
		Menu menu;
		menu.id = _nextId++;
		menu.modName = a_desc.modName;
		menu.kind = a_desc.kind;
		menu.onOpen = a_desc.onOpen;
		menu.onClose = a_desc.onClose;
		menu.onReset = a_desc.onReset;
		menu.user = a_desc.user;
		_menus.push_back(std::move(menu));
		return _menus.back().id;
	}

	MenuHelper_MenuId Registry::CreateInternalMenu(bool a_popup, MenuHelper_MenuCallback a_onClose)
	{
		const std::scoped_lock l{ _lock };
		Menu menu;
		menu.id = _nextId++;
		menu.modName = kInternalModName;
		menu.internal = true;
		menu.popup = a_popup;
		menu.onClose = a_onClose;
		_menus.push_back(std::move(menu));
		return _menus.back().id;
	}

	bool Registry::DestroyMenu(MenuHelper_MenuId a_id)
	{
		const std::scoped_lock l{ _lock };
		return std::erase_if(_menus, [&](const Menu& a_menu) { return a_menu.id == a_id; }) > 0;
	}

	std::optional<Menu> Registry::FindMenu(MenuHelper_MenuId a_id) const
	{
		const std::scoped_lock l{ _lock };
		const auto it = std::ranges::find(_menus, a_id, &Menu::id);
		return it != _menus.end() ? std::optional{ *it } : std::nullopt;
	}

	MenuHelper_ItemId Registry::AddItem(MenuHelper_MenuId a_menu, Item a_item)
	{
		const std::scoped_lock l{ _lock };
		const auto menu = std::ranges::find(_menus, a_menu, &Menu::id);
		if (menu == _menus.end()) {
			return MENUHELPER_INVALID_ID;
		}
		if ((menu->kind == MENUHELPER_MENU_PAGES) != (a_item.type == MENUHELPER_ITEM_TOPIC)) {
			REX::WARN("rejected item \"{}\" of {}: pages menus hold topics only, and topics need a pages menu", a_item.label, menu->modName);
			return MENUHELPER_INVALID_ID;
		}
		a_item.id = _nextId++;
		a_item.menu = a_menu;
		a_item.modName = menu->modName;
		const auto saved = a_item.saveKey.empty() ? std::nullopt : SavedValues::Load(a_item.modName, a_item.saveKey);
		a_item.value = a_item.Clamp(saved.value_or(a_item.value));
		a_item.defaultValue = a_item.Clamp(a_item.defaultValue);
		for (const auto& text : { a_item.label, a_item.description, a_item.format }) {
			Translations::AddToTemplate(a_item.modName, text);
		}
		for (const auto& option : a_item.options) {
			Translations::AddToTemplate(a_item.modName, option);
		}
		menu->items.push_back(std::move(a_item));
		return menu->items.back().id;
	}

	MenuHelper_MenuId Registry::RemoveItem(MenuHelper_ItemId a_id)
	{
		const std::scoped_lock l{ _lock };
		for (auto& menu : _menus) {
			if (std::erase_if(menu.items, [&](const Item& a_item) { return a_item.id == a_id; }) > 0) {
				return menu.id;
			}
		}
		return MENUHELPER_INVALID_ID;
	}

	template <class F>
	MenuHelper_MenuId Registry::UpdateItem(MenuHelper_ItemId a_id, F&& a_update)
	{
		const std::scoped_lock l{ _lock };
		auto* item = FindItemUnlocked(a_id);
		if (!item) {
			return MENUHELPER_INVALID_ID;
		}
		a_update(*item);
		return item->menu;
	}

	MenuHelper_MenuId Registry::SetItemLabel(MenuHelper_ItemId a_id, std::string_view a_label)
	{
		return UpdateItem(a_id, [&](Item& a_item) {
			a_item.label = a_label;
			Translations::AddToTemplate(a_item.modName, a_item.label);
		});
	}

	MenuHelper_MenuId Registry::SetItemDescription(MenuHelper_ItemId a_id, std::string_view a_description)
	{
		return UpdateItem(a_id, [&](Item& a_item) {
			a_item.description = a_description;
			Translations::AddToTemplate(a_item.modName, a_item.description);
		});
	}

	MenuHelper_MenuId Registry::SetItemEnabled(MenuHelper_ItemId a_id, bool a_enabled)
	{
		return UpdateItem(a_id, [&](Item& a_item) { a_item.enabled = a_enabled; });
	}

	MenuHelper_MenuId Registry::SetItemVisible(MenuHelper_ItemId a_id, bool a_visible)
	{
		return UpdateItem(a_id, [&](Item& a_item) {
			a_item.flags = a_visible ? a_item.flags & ~MENUHELPER_FLAG_HIDDEN : a_item.flags | MENUHELPER_FLAG_HIDDEN;
		});
	}

	MenuHelper_MenuId Registry::SetItemValue(MenuHelper_ItemId a_id, double a_value, bool a_save)
	{
		std::optional<Item> saved;
		const auto          menu = UpdateItem(a_id, [&](Item& a_item) {
			a_item.value = a_item.Clamp(a_value);
			if (a_save && !a_item.saveKey.empty()) {
				saved = a_item;
			}
		});
		if (saved) {
			SavedValues::Save(saved->modName, saved->saveKey, saved->value);
		}
		return menu;
	}

	std::optional<Item> Registry::FindItem(MenuHelper_ItemId a_id) const
	{
		const std::scoped_lock l{ _lock };
		for (const auto& menu : _menus) {
			const auto it = std::ranges::find(menu.items, a_id, &Item::id);
			if (it != menu.items.end()) {
				return *it;
			}
		}
		return std::nullopt;
	}

	std::vector<MenuHelper_MenuId> Registry::SetSavedItemValues(std::string_view a_modName, std::string_view a_saveKey, double a_value)
	{
		const std::scoped_lock         l{ _lock };
		std::vector<MenuHelper_MenuId> changed;
		for (auto& menu : _menus) {
			for (auto& item : menu.items) {
				if (item.modName == a_modName && item.saveKey == a_saveKey) {
					item.value = item.Clamp(a_value);
					if (!std::ranges::contains(changed, menu.id)) {
						changed.push_back(menu.id);
					}
				}
			}
		}
		return changed;
	}

	std::vector<Item> Registry::Items(std::uint32_t a_type) const
	{
		const std::scoped_lock l{ _lock };
		std::vector<Item>      result;
		for (const auto& menu : _menus) {
			std::ranges::copy_if(menu.items, std::back_inserter(result), [&](const Item& a_item) { return a_item.type == a_type; });
		}
		return result;
	}

	bool Registry::ReplaceItems(MenuHelper_MenuId a_menu, std::vector<Item> a_items)
	{
		const std::scoped_lock l{ _lock };
		const auto             menu = std::ranges::find(_menus, a_menu, &Menu::id);
		if (menu == _menus.end()) {
			return false;
		}
		for (auto& item : a_items) {
			item.id = _nextId++;
			item.menu = a_menu;
		}
		menu->items = std::move(a_items);
		return true;
	}

	MenuHelper_ChangeId Registry::AddVanillaChange(const MenuHelper_VanillaChangeDesc& a_desc)
	{
		if ((a_desc.menus & MENUHELPER_MENU_ALL) == 0 || a_desc.entry < 0) {
			return MENUHELPER_INVALID_ID;
		}
		const std::scoped_lock l{ _lock };
		VanillaChange change;
		change.id = _nextId++;
		change.menus = a_desc.menus & MENUHELPER_MENU_ALL;
		change.entry = a_desc.entry;
		change.hidden = a_desc.hidden != 0;
		change.move = a_desc.move;
		_changes.push_back(change);
		return change.id;
	}

	bool Registry::RemoveVanillaChange(MenuHelper_ChangeId a_id)
	{
		const std::scoped_lock l{ _lock };
		return std::erase_if(_changes, [&](const VanillaChange& a_change) { return a_change.id == a_id; }) > 0;
	}

	std::vector<VanillaChange> Registry::VanillaChanges(std::uint32_t a_menu) const
	{
		const std::scoped_lock     l{ _lock };
		std::vector<VanillaChange> result;
		std::ranges::copy_if(_changes, std::back_inserter(result), [&](const VanillaChange& a_change) { return (a_change.menus & a_menu) != 0; });
		return result;
	}

	Entry* Registry::FindEntryUnlocked(MenuHelper_EntryId a_id)
	{
		const auto it = std::ranges::find(_entries, a_id, &Entry::id);
		return it != _entries.end() ? &*it : nullptr;
	}

	Item* Registry::FindItemUnlocked(MenuHelper_ItemId a_id)
	{
		for (auto& menu : _menus) {
			const auto it = std::ranges::find(menu.items, a_id, &Item::id);
			if (it != menu.items.end()) {
				return &*it;
			}
		}
		return nullptr;
	}
}
