#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper
{
	struct Entry
	{
		MenuHelper_EntryId       id{ MENUHELPER_INVALID_ID };
		std::string              modName;
		std::string              label;
		std::uint32_t            menus{ 0 };
		std::int32_t             insertBefore{ MENUHELPER_VANILLA_APPEND };
		MenuHelper_PressCallback onPress{ nullptr };
		void*                    user{ nullptr };
		MenuHelper_MenuId        submenu{ MENUHELPER_INVALID_ID };
		bool                     enabled{ true };
	};

	struct Item
	{
		MenuHelper_ItemId        id{ MENUHELPER_INVALID_ID };
		MenuHelper_MenuId        menu{ MENUHELPER_INVALID_ID };
		std::string              modName;
		std::uint32_t            type{ MENUHELPER_ITEM_BUTTON };
		std::uint32_t            flags{ 0 };
		std::string              label;
		std::string              description;
		double                   value{ 0.0 };
		double                   defaultValue{ 0.0 };
		double                   min{ 0.0 };
		double                   max{ 1.0 };
		double                   step{ 0.0 };
		std::string              format;
		std::uint32_t            decimals{ 0 };
		std::vector<std::string> options;
		MenuHelper_MenuId        submenu{ MENUHELPER_INVALID_ID };
		MenuHelper_ItemCallback  onChange{ nullptr };
		MenuHelper_ItemCallback  onSelect{ nullptr };
		MenuHelper_ItemCallback  onHotkey{ nullptr };
		void*                    user{ nullptr };
		std::string              saveKey;
		std::string              image;
		std::uint32_t            imageWidth{ 0 };
		std::uint32_t            imageHeight{ 0 };
		// JSON menus: reported to action listeners and Papyrus, and the Papyrus global function to call.
		std::string              action;
		std::string              papyrusFunction;
		bool                     enabled{ true };

		double Clamp(double a_value) const;
		bool   HasValue() const;
		bool   Visible() const { return (flags & MENUHELPER_FLAG_HIDDEN) == 0; }
		// Slider digits after the decimal point.
		std::uint32_t Decimals() const;
	};

	enum class Layout : std::uint32_t
	{
		kList = 0,
		kOptions = 1,
		kPages = 2
	};

	struct Menu
	{
		MenuHelper_MenuId       id{ MENUHELPER_INVALID_ID };
		std::string             modName;
		std::uint32_t           kind{ MENUHELPER_MENU_ITEMS };
		MenuHelper_MenuCallback onOpen{ nullptr };
		MenuHelper_MenuCallback onClose{ nullptr };
		MenuHelper_MenuCallback onReset{ nullptr };
		void*                   user{ nullptr };
		std::vector<Item>       items;
		// Menus MenuHelper builds itself: no DEFAULTS button. Popups are drawn over the menu below.
		bool                    internal{ false };
		bool                    popup{ false };

		Layout GetLayout() const;
		bool   HasDefaults() const;
	};

	struct VanillaChange
	{
		MenuHelper_ChangeId id{ MENUHELPER_INVALID_ID };
		std::uint32_t       menus{ 0 };
		std::int32_t        entry{ 0 };
		bool                hidden{ false };
		std::int32_t        move{ 0 };
	};

	class Registry
	{
	public:
		static Registry& Get();

		// Validates a descriptor and converts it. Logs why it is rejected.
		static std::optional<Item> MakeItem(const MenuHelper_ItemDesc& a_desc);

		MenuHelper_EntryId   AddEntry(const MenuHelper_EntryDesc& a_desc);
		bool                 RemoveEntry(MenuHelper_EntryId a_id);
		bool                 SetEntryLabel(MenuHelper_EntryId a_id, std::string_view a_label);
		bool                 SetEntryEnabled(MenuHelper_EntryId a_id, bool a_enabled);
		std::vector<Entry>   Entries(std::uint32_t a_menu) const;
		std::optional<Entry> FindEntry(MenuHelper_EntryId a_id) const;

		MenuHelper_MenuId   CreateMenu(const MenuHelper_MenuDesc& a_desc);
		MenuHelper_MenuId   CreateInternalMenu(bool a_popup, MenuHelper_MenuCallback a_onClose = nullptr);
		bool                DestroyMenu(MenuHelper_MenuId a_id);
		std::optional<Menu> FindMenu(MenuHelper_MenuId a_id) const;

		MenuHelper_ItemId   AddItem(MenuHelper_MenuId a_menu, Item a_item);
		MenuHelper_MenuId   RemoveItem(MenuHelper_ItemId a_id);
		MenuHelper_MenuId   SetItemLabel(MenuHelper_ItemId a_id, std::string_view a_label);
		MenuHelper_MenuId   SetItemDescription(MenuHelper_ItemId a_id, std::string_view a_description);
		MenuHelper_MenuId   SetItemEnabled(MenuHelper_ItemId a_id, bool a_enabled);
		MenuHelper_MenuId   SetItemVisible(MenuHelper_ItemId a_id, bool a_visible);
		// Saves the value when the item has a save key, unless a_save is false.
		MenuHelper_MenuId   SetItemValue(MenuHelper_ItemId a_id, double a_value, bool a_save = true);
		std::optional<Item> FindItem(MenuHelper_ItemId a_id) const;
		// Sets every item with this save key without saving. Returns the menus to refresh.
		std::vector<MenuHelper_MenuId> SetSavedItemValues(std::string_view a_modName, std::string_view a_saveKey, double a_value);
		std::vector<Item>              Items(std::uint32_t a_type) const;
		// Gives the items new IDs. For menus MenuHelper builds itself.
		bool                           ReplaceItems(MenuHelper_MenuId a_menu, std::vector<Item> a_items);

		MenuHelper_ChangeId        AddVanillaChange(const MenuHelper_VanillaChangeDesc& a_desc);
		bool                       RemoveVanillaChange(MenuHelper_ChangeId a_id);
		std::vector<VanillaChange> VanillaChanges(std::uint32_t a_menu) const;

	private:
		Entry* FindEntryUnlocked(MenuHelper_EntryId a_id);
		Item*  FindItemUnlocked(MenuHelper_ItemId a_id);

		template <class F>
		MenuHelper_MenuId UpdateItem(MenuHelper_ItemId a_id, F&& a_update);

		mutable std::mutex         _lock;
		std::vector<Entry>         _entries;
		std::vector<Menu>          _menus;
		std::vector<VanillaChange> _changes;
		std::uint32_t              _nextId{ 1 };
	};
}
