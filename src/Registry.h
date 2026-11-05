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
		std::string              label;
		std::string              description;
		float                    value{ 0.0F };
		float                    min{ 0.0F };
		float                    max{ 1.0F };
		float                    step{ 0.0F };
		std::vector<std::string> options;
		MenuHelper_MenuId        submenu{ MENUHELPER_INVALID_ID };
		MenuHelper_ItemCallback  onChange{ nullptr };
		void*                    user{ nullptr };
		std::string              saveKey;
		bool                     enabled{ true };

		float Clamp(float a_value) const;
	};

	struct Menu
	{
		MenuHelper_MenuId       id{ MENUHELPER_INVALID_ID };
		std::string             modName;
		MenuHelper_MenuCallback onOpen{ nullptr };
		MenuHelper_MenuCallback onClose{ nullptr };
		void*                   user{ nullptr };
		std::vector<Item>       items;

		bool UsesOptionsLayout() const;
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

		MenuHelper_EntryId   AddEntry(const MenuHelper_EntryDesc& a_desc);
		bool                 RemoveEntry(MenuHelper_EntryId a_id);
		bool                 SetEntryLabel(MenuHelper_EntryId a_id, std::string_view a_label);
		bool                 SetEntryEnabled(MenuHelper_EntryId a_id, bool a_enabled);
		std::vector<Entry>   Entries(std::uint32_t a_menu) const;
		std::optional<Entry> FindEntry(MenuHelper_EntryId a_id) const;

		MenuHelper_MenuId   CreateMenu(const MenuHelper_MenuDesc& a_desc);
		bool                DestroyMenu(MenuHelper_MenuId a_id);
		std::optional<Menu> FindMenu(MenuHelper_MenuId a_id) const;

		MenuHelper_ItemId   AddItem(MenuHelper_MenuId a_menu, const MenuHelper_ItemDesc& a_desc);
		MenuHelper_MenuId   RemoveItem(MenuHelper_ItemId a_id);
		MenuHelper_MenuId   SetItemLabel(MenuHelper_ItemId a_id, std::string_view a_label);
		MenuHelper_MenuId   SetItemDescription(MenuHelper_ItemId a_id, std::string_view a_description);
		MenuHelper_MenuId   SetItemEnabled(MenuHelper_ItemId a_id, bool a_enabled);
		MenuHelper_MenuId   SetItemValue(MenuHelper_ItemId a_id, float a_value);
		std::optional<Item> FindItem(MenuHelper_ItemId a_id) const;
		// Gives the items new IDs. For menus MenuHelper builds itself.
		bool                ReplaceItems(MenuHelper_MenuId a_menu, std::vector<Item> a_items);

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
