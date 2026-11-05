# Plugin API

Copy `include/MenuHelper/MenuHelper.h` into your project. The comments in the header describe every function and field. `examples/basic/main.cpp` uses every feature.

Load the API once all plugins have loaded (F4SE `kPostLoad` or later):

```cpp
#include <Windows.h>
#include "MenuHelper/MenuHelper.h"

void OnShowMarkers(MenuHelper_ItemId, double value, void*)
{
    // value is 0 or 1
}

void Register()
{
    const MenuHelper_API* api = MenuHelper_Load();  // NULL when Menu Helper is not installed
    if (!api) {
        return;
    }

    MenuHelper_MenuDesc menuDesc{ sizeof(menuDesc) };
    menuDesc.modName = "MyMod";
    const MenuHelper_MenuId menu = api->CreateMenu(&menuDesc);

    MenuHelper_ItemDesc item{ sizeof(item) };
    item.type = MENUHELPER_ITEM_CHECKBOX;
    item.label = "SHOW MARKERS";
    item.description = "Shows map markers.";
    item.saveKey = "bShowMarkers";
    item.onChange = OnShowMarkers;
    const MenuHelper_ItemId checkbox = api->AddItem(menu, &item);
    OnShowMarkers(checkbox, api->GetItemValue(checkbox), nullptr);  // apply the saved value

    MenuHelper_EntryDesc entry{ sizeof(entry) };
    entry.modName = "MyMod";
    entry.label = "MY MOD";
    entry.menus = MENUHELPER_MENU_ALL;
    entry.insertBefore = MENUHELPER_VANILLA_QUIT;
    entry.submenu = menu;
    api->AddEntry(&entry);
}
```

## Versions

`MenuHelper_Load` returns `NULL` when Menu Helper is missing or older than your copy of the header. Newer Menu Helper versions work with plugins built against older headers.

## Functions

| Function | Use |
| --- | --- |
| `AddEntry`, `RemoveEntry`, `SetEntryLabel`, `SetEntryEnabled` | Entries in the main menu, pause menu, or both. |
| `CreateMenu`, `DestroyMenu` | Menus of items or of text pages. |
| `AddItem`, `RemoveItem` | Items in a menu. |
| `SetItemLabel`, `SetItemDescription`, `SetItemEnabled`, `SetItemVisible`, `SetItemValue`, `GetItemValue` | Read or change an item. |
| `OpenMenu`, `CloseMenu` | Open or close a menu while the main or pause menu is open. |
| `ChangeVanillaEntry`, `RemoveVanillaChange` | Hide or move a vanilla entry. |
| `ShowMessageBox`, `ShowConfirm`, `ShowNotification` | See [Dialogs](https://github.com/DeisDev/Menu-Helper/wiki/Dialogs). |
| `GetSavedValue`, `SetSavedValue` | Read or write a saved value without an item. |
| `RegisterSettingListener`, `RegisterActionListener` | Get changes and actions of a mod, for example of its JSON menu. |
| `Translate` | See [Translations](https://github.com/DeisDev/Menu-Helper/wiki/Translations). |
| `GetKeyName` | See [Hotkeys](https://github.com/DeisDev/Menu-Helper/wiki/Hotkeys). |

## Item types

| Type | Shows | Value |
| --- | --- | --- |
| Button | A row to press. Can open a submenu. | none |
| Checkbox | ON or OFF | 0 or 1 |
| Stepper | Options changed with Left and Right | option index |
| Slider | A slider and its value, for example `50 %` | `min` to `max` |
| Dropdown | The chosen option. Pressing it opens a list. | option index |
| Hotkey | The key name. Pressing it waits for a key. | key code |
| Color | `#RRGGBB`. Pressing it opens red, green and blue sliders. | `0xAARRGGBB` |
| Header | A row that cannot be selected | none |
| Text | A row that shows its description | none |
| Topic | A page of a pages menu | none |

## Rules

- Functions can be called from any thread. Callbacks run on the game's UI thread.
- Changes show in open menus right away.
- A menu with only buttons, headers and text looks like the Settings category list. Other items, or a button with a description, make it look like a Settings page.
- Menus with value items show DEFAULTS (T). It sets visible value items back to the value they were added with, then calls `onReset`.
- `onSelect` runs when an item becomes the selected row.
- A button with `submenu` opens that menu. Esc goes back one level.
- `SetItemValue` does not call `onChange`.
- Labels are plain text.

## Saved values

Give a value item a `saveKey`. Its value is saved to `Data/F4SE/Plugins/MenuHelper/Settings/<modName>.ini` when it changes, and `AddItem` starts from the saved value. Read it with `GetItemValue` after adding the item.

## Vanilla entries

`ChangeVanillaEntry` hides a vanilla entry or moves it. Changes apply in order, after mod entries are placed.

```cpp
MenuHelper_VanillaChangeDesc change{ sizeof(change) };
change.menus = MENUHELPER_MENU_MAIN;
change.entry = MENUHELPER_VANILLA_LOAD;
change.move = 2;  // two rows down
api->ChangeVanillaEntry(&change);
```

## Many entries

When the main or pause menu would have more than 9 rows, all mod entries move into one MODS entry. Players can change this with `sModsFolder`.
