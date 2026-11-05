# Menu Helper

An F4SE plugin that lets other F4SE plugins add entries and submenus to the Fallout 4 main menu and pause menu.

Submenus use the game's own Settings panels, so they look and behave like vanilla menus. No vanilla SWF files are replaced.

Requires Fallout 4 1.11.240 (Steam), F4SE 0.7.9 or later, and Address Library for F4SE.

## Install

Copy the `Data` folder from the release archive into the game folder, or install the archive with a mod manager. It contains:

- `Data/F4SE/Plugins/MenuHelper.dll`
- `Data/Interface/MenuHelper/MenuHelper.swf`
- `Data/Scripts/MenuHelper.pex` and `Data/Scripts/Source/User/MenuHelper.psc`

## Use it from your plugin

Copy `include/MenuHelper/MenuHelper.h` into your project and load the API once all plugins have loaded (F4SE `kPostLoad` or later):

```cpp
#include <Windows.h>
#include "MenuHelper/MenuHelper.h"

void OnShowMarkers(MenuHelper_ItemId, float value, void*)
{
    // value is 0 or 1
}

void Register()
{
    const MenuHelper_API* api = MenuHelper_Load();  // NULL when MenuHelper is not installed
    if (!api) {
        return;
    }

    MenuHelper_MenuDesc menuDesc{ sizeof(menuDesc) };
    const MenuHelper_MenuId menu = api->CreateMenu(&menuDesc);

    MenuHelper_ItemDesc item{ sizeof(item) };
    item.type = MENUHELPER_ITEM_CHECKBOX;
    item.label = "SHOW MARKERS";
    item.description = "Shows map markers.";
    item.onChange = OnShowMarkers;
    api->AddItem(menu, &item);

    MenuHelper_EntryDesc entry{ sizeof(entry) };
    entry.label = "MY MOD";
    entry.menus = MENUHELPER_MENU_ALL;
    entry.insertBefore = MENUHELPER_VANILLA_QUIT;
    entry.submenu = menu;
    api->AddEntry(&entry);
}
```

`examples/basic/main.cpp` shows every feature.

| Function | What it does |
|---|---|
| `AddEntry`, `RemoveEntry` | Add or remove an entry in the main menu, the pause menu, or both. |
| `SetEntryLabel`, `SetEntryEnabled` | Change an entry. |
| `CreateMenu`, `DestroyMenu` | Create or remove a submenu. |
| `AddItem`, `RemoveItem` | Add or remove a button, checkbox, stepper or slider. |
| `SetItemLabel`, `SetItemDescription`, `SetItemEnabled`, `SetItemValue`, `GetItemValue` | Read or change an item. |
| `OpenMenu`, `CloseMenu` | Open a submenu on top of the open main or pause menu, or close it. |

Notes:

- API functions can be called from any thread. Callbacks run on the game's UI thread.
- Changes show up right away in open menus.
- A menu with only buttons looks like the Settings category list. A menu with a checkbox, stepper, slider or description looks like a Settings page.
- A button with `submenu` set opens that menu. Esc goes back one level.
- Labels are plain text. `$KEY` strings are translated by the game, like vanilla labels.

## Use it from Papyrus

`MenuHelper.psc` adds the same features for Papyrus scripts. Papyrus only runs in a loaded game, so Papyrus entries appear in the pause menu only. Everything a script creates is removed when a game is loaded or started, so register from `OnInit` and again after every load.

```papyrus
Scriptname MyModMenuQuest extends Quest

int checkboxId

Event OnInit()
    RegisterForRemoteEvent(Game.GetPlayer(), "OnPlayerLoadGame")
    Setup()
EndEvent

Event Actor.OnPlayerLoadGame(Actor akSender)
    Setup()
EndEvent

Function Setup()
    RegisterForExternalEvent("MenuHelper_OnItemChange", "OnMenuHelperItemChange")
    int menu = MenuHelper.CreateMenu()
    checkboxId = MenuHelper.AddCheckbox(menu, "SHOW MARKERS", true, "Shows map markers.")
    MenuHelper.AddEntry("MY MOD", menu)
EndFunction

Function OnMenuHelperItemChange(int aiItem, float afValue)
    If aiItem == checkboxId
        Debug.Notification("Show markers: " + afValue)
    EndIf
EndFunction
```

The full function and event list is at the top of `papyrus/MenuHelper.psc`. Events are sent to every registered script, so compare the ID first.

## Compatibility

MenuHelper needs the vanilla `Interface/MainMenu.swf`. If another mod replaces it, MenuHelper logs a warning and adds nothing.

Log file: `Documents/My Games/Fallout4/F4SE/MenuHelper.log`.

## Build

Use a Windows shell (not Git Bash) with xmake and Visual Studio's C++ tools:

```
git submodule update --init --recursive
xmake f -p windows -a x64 --toolchain=msvc -m releasedbg
xmake build
xmake package MenuHelper
```

The archive is written to `build/packages`. Set `XSE_FO4_GAME_PATH` to the game folder to install into the game after each build.

`swf/MenuHelper.swf` is built from `swf/src` and checked in. To rebuild it you need Java, the [Apache Flex SDK](https://flex.apache.org/download-binaries.html) and a `playerglobal.swc` (FFDec includes one in its `flashlib` folder):

```
xmake f --flex_sdk=<Flex SDK folder> --playerglobal=<path to playerglobal.swc>
xmake build MenuHelperSWF
```

`papyrus/MenuHelper.pex` is also checked in. To rebuild it you need the Creation Kit's compiler, F4SE's script sources and the Creation Kit's `Base.zip` extracted:

```
xmake f --papyrus_compiler=<path to PapyrusCompiler.exe> --papyrus_imports=<F4SE script sources>;<extracted Base.zip>
xmake build MenuHelperPapyrus
```

## License

GPL-3.0. See `LICENSE`.
