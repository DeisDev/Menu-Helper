# Menu Helper

An F4SE plugin that lets other F4SE plugins add entries and submenus to the Fallout 4 main menu and pause menu.

Submenus use the game's own Settings panels, so they look and behave like vanilla menus. No vanilla SWF files are replaced.

Requires Fallout 4 1.11.240 (Steam), F4SE 0.7.9 or later, and Address Library for F4SE.

## Install

Copy the `Data` folder from the release archive into the game folder, or install the archive with a mod manager. It contains:

- `Data/F4SE/Plugins/MenuHelper.dll` and `MenuHelper.ini`
- `Data/Interface/MenuHelper/MenuHelper.swf`
- `Data/Scripts/MenuHelper.pex` and `Data/Scripts/Source/User/MenuHelper.psc`

## Settings

`Data/F4SE/Plugins/MenuHelper.ini`:

- `sModsFolder`: `auto` (default) moves all mod entries into one MODS entry when the main or pause menu would need to scroll. `always` always does, `never` keeps them in the list, which then scrolls.
- `bExportTemplates`: `1` writes translation templates, see [Translations](#translations).

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

`examples/basic/main.cpp` shows every feature.

| Function | What it does |
|---|---|
| `AddEntry`, `RemoveEntry` | Add or remove an entry in the main menu, the pause menu, or both. |
| `SetEntryLabel`, `SetEntryEnabled` | Change an entry. |
| `CreateMenu`, `DestroyMenu` | Create or remove a submenu. |
| `AddItem`, `RemoveItem` | Add or remove a button, checkbox, stepper or slider. |
| `SetItemLabel`, `SetItemDescription`, `SetItemEnabled`, `SetItemValue`, `GetItemValue` | Read or change an item. |
| `OpenMenu`, `CloseMenu` | Open a submenu on top of the open main or pause menu, or close it. |
| `ChangeVanillaEntry`, `RemoveVanillaChange` | Hide or move a vanilla entry, or undo that. |
| `Translate` | Translate text you build yourself. |

Notes:

- API functions can be called from any thread. Callbacks run on the game's UI thread.
- Changes show up right away in open menus.
- A menu with only buttons looks like the Settings category list. A menu with a checkbox, stepper, slider or description looks like a Settings page.
- A button with `submenu` set opens that menu. Esc goes back one level.
- Labels are plain text. See [Translations](#translations).
- To open your own window (for example an ImGui overlay) from an entry or button, show it from `onPress` or `onChange`. MenuHelper does not handle your window's input.

### Saved values

Give a checkbox, stepper or slider a `saveKey` and MenuHelper saves its value to `Data/F4SE/Plugins/MenuHelper/Settings/<modName>.ini` whenever it changes. `AddItem` then starts from the saved value, so read it with `GetItemValue` after adding the item.

### Vanilla entries

`ChangeVanillaEntry` hides a vanilla entry or moves it by a number of rows. Changes apply in the order they were made, after mod entries are placed:

```cpp
MenuHelper_VanillaChangeDesc change{ sizeof(change) };
change.menus = MENUHELPER_MENU_MAIN;
change.entry = MENUHELPER_VANILLA_LOAD;
change.move = 2;  // two rows down
api->ChangeVanillaEntry(&change);
```

### Many entries

When the main or pause menu would have more rows than it shows (9), all mod entries move into one MODS entry that lists them. QUIT stays visible and nothing scrolls. Players can change this with `sModsFolder`.

## Translations

Every label, description and stepper option is looked up before it is shown, so anyone can translate a mod later without changing it. A translation is a UTF-8 JSON file that maps the original text to the translated text:

`Data/Interface/MenuHelper/Translations/MyMod_de.json`

```json
{
    "SHOW MARKERS": "MARKIERUNGEN ANZEIGEN",
    "Shows map markers.": "Zeigt Kartenmarkierungen an."
}
```

- `MyMod` is the `modName` the mod passes to MenuHelper. `de` is the game's language (`sLanguage` in `Fallout4.ini`: `en`, `de`, `fr`, `es`, `esmx`, `it`, `pl`, `ptbr`, `ru`, `ja`, `cn`).
- Text missing from the file falls back to `MyMod_en.json`, then to the original text. Mods can use `$KEY` strings with an `en` file as well.
- `//` comments are allowed. Files are loaded once, the first time they are needed; restart the game after editing one.
- For text you build at runtime, translate a whole message with named placeholders and fill them in yourself: `api->Translate("MyMod", "PRESSED {count}")`. In Papyrus: `MenuHelper.FillPlaceholder(MenuHelper.Translate("MyMod", "PRESSED {count}"), "count", 3)`.
- MenuHelper's own text (the MODS entry) uses the mod name `MenuHelper`.
- To start a translation, set `bExportTemplates = 1` in `MenuHelper.ini` and start the game. MenuHelper writes `<modName>_template.json` next to the translations with every text the mod uses; open the mod's menus to catch text it builds at runtime. Copy the file to `<modName>_<language>.json` and translate the values.

The example plugin ships `examples/basic/Translations/MenuHelperExample_de.json`.

## Use it from Papyrus

`MenuHelper.psc` adds the same features for Papyrus scripts. Papyrus only runs in a loaded game, so Papyrus entries and vanilla changes apply to the pause menu only. Everything a script creates is removed when a game is loaded or started, so register from `OnInit` and again after every load.

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
    int menu = MenuHelper.CreateMenu("MyMod")
    checkboxId = MenuHelper.AddCheckbox(menu, "SHOW MARKERS", true, "Shows map markers.", "bShowMarkers")
    MenuHelper.AddEntry("MyMod", "MY MOD", menu)
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

Entries that other mods add to the main or pause menu (for example MCM's MOD CONFIG) keep their place.

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
