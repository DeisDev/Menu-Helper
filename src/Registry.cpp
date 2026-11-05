#include "Registry.h"

#include "SavedValues.h"
#include "Translations.h"

namespace MenuHelper
{
	namespace
	{
		bool IsValidModName(const char* a_modName)
		{
			if (a_modName && Translations::IsValidModName(a_modName)) {
				return true;
			}
			REX::WARN("rejected mod name \"{}\": it must be a valid file name", a_modName ? a_modName : "");
			return false;
		}

		bool IsValidItem(const MenuHelper_ItemDesc& a_desc)
		{
			if (!a_desc.label) {
				return false;
			}
			if (a_desc.saveKey && (a_desc.type == MENUHELPER_ITEM_BUTTON || !SavedValues::IsValidKey(a_desc.saveKey))) {
				REX::WARN("rejected item \"{}\": buttons cannot have a save key, and save keys must be valid INI keys", a_desc.label);
				return false;
			}
			switch (a_desc.type) {
				case MENUHELPER_ITEM_BUTTON:
				case MENUHELPER_ITEM_CHECKBOX:
					return true;
				case MENUHELPER_ITEM_STEPPER:
					return a_desc.options && a_desc.optionCount > 0 &&
					       std::all_of(a_desc.options, a_desc.options + a_desc.optionCount, [](const char* a_option) { return a_option != nullptr; });
				case MENUHELPER_ITEM_SLIDER:
					return std::isfinite(a_desc.min) && std::isfinite(a_desc.max) && std::isfinite(a_desc.step) &&
					       a_desc.max > a_desc.min && a_desc.step > 0.0F;
				default:
					return false;
			}
		}
	}

	float Item::Clamp(float a_value) const
	{
		if (!std::isfinite(a_value)) {
			a_value = 0.0F;
		}
		switch (type) {
			case MENUHELPER_ITEM_CHECKBOX:
				return a_value != 0.0F ? 1.0F : 0.0F;
			case MENUHELPER_ITEM_STEPPER:
				return std::clamp(std::round(a_value), 0.0F, static_cast<float>(options.size() - 1));
			case MENUHELPER_ITEM_SLIDER:
				{
					const auto steps = std::round((std::clamp(a_value, min, max) - min) / step);
					return std::min(min + steps * step, max);
				}
			default:
				return 0.0F;
		}
	}

	bool Menu::UsesOptionsLayout() const
	{
		return std::ranges::any_of(items, [](const Item& a_item) {
			return a_item.type != MENUHELPER_ITEM_BUTTON || !a_item.description.empty();
		});
	}

	Registry& Registry::Get()
	{
		static Registry singleton;
		return singleton;
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
		const std::scoped_lock l{ _lock };
		Menu menu;
		menu.id = _nextId++;
		menu.modName = a_desc.modName;
		menu.onOpen = a_desc.onOpen;
		menu.onClose = a_desc.onClose;
		menu.user = a_desc.user;
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

	MenuHelper_ItemId Registry::AddItem(MenuHelper_MenuId a_menu, const MenuHelper_ItemDesc& a_desc)
	{
		if (!IsValidItem(a_desc)) {
			return MENUHELPER_INVALID_ID;
		}
		const std::scoped_lock l{ _lock };
		const auto menu = std::ranges::find(_menus, a_menu, &Menu::id);
		if (menu == _menus.end()) {
			return MENUHELPER_INVALID_ID;
		}
		Item item;
		item.id = _nextId++;
		item.menu = a_menu;
		item.modName = menu->modName;
		item.type = a_desc.type;
		item.label = a_desc.label;
		item.description = a_desc.description ? a_desc.description : "";
		item.min = a_desc.min;
		item.max = a_desc.max;
		item.step = a_desc.step;
		if (a_desc.type == MENUHELPER_ITEM_STEPPER) {
			item.options.assign(a_desc.options, a_desc.options + a_desc.optionCount);
		}
		item.saveKey = a_desc.saveKey ? a_desc.saveKey : "";
		const auto saved = item.saveKey.empty() ? std::nullopt : SavedValues::Load(item.modName, item.saveKey);
		item.value = item.Clamp(saved.value_or(a_desc.value));
		for (const auto& text : { item.label, item.description }) {
			Translations::AddToTemplate(item.modName, text);
		}
		for (const auto& option : item.options) {
			Translations::AddToTemplate(item.modName, option);
		}
		item.submenu = a_desc.type == MENUHELPER_ITEM_BUTTON ? a_desc.submenu : MENUHELPER_INVALID_ID;
		item.onChange = a_desc.onChange;
		item.user = a_desc.user;
		menu->items.push_back(std::move(item));
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

	MenuHelper_MenuId Registry::SetItemValue(MenuHelper_ItemId a_id, float a_value)
	{
		std::optional<Item> saved;
		const auto          menu = UpdateItem(a_id, [&](Item& a_item) {
			a_item.value = a_item.Clamp(a_value);
			if (!a_item.saveKey.empty()) {
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
