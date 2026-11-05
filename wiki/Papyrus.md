# Papyrus

`MenuHelper.psc` has the same features as the plugin API. The full list of functions and events is at the top of the file.

- Papyrus runs only in a loaded game, so Papyrus entries and vanilla changes are in the pause menu only.
- Entries, menus and vanilla changes a script creates are removed when a game is loaded or started. Register from `OnInit` and again after every load.
- Events go to every registered script. Compare the ID first.
- Do not pass `None` as an array. It crashes the game.

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

## Events

| Event | Arguments |
| --- | --- |
| `MenuHelper_OnEntryPress` | `int aiEntry` |
| `MenuHelper_OnItemChange` | `int aiItem, float afValue` |
| `MenuHelper_OnItemSelect` | `int aiItem` |
| `MenuHelper_OnHotkey` | `int aiItem` |
| `MenuHelper_OnMenuOpen` | `int aiMenu` |
| `MenuHelper_OnMenuClose` | `int aiMenu` |
| `MenuHelper_OnMenuReset` | `int aiMenu` |
| `MenuHelper_OnMessageBoxButton` | `int aiBox, int aiButton` |
| `MenuHelper_OnConfirm` | `int aiConfirm, bool abResult` |
| `MenuHelper_OnSettingChange` | `string asModName, string asSaveKey, float afValue` |
| `MenuHelper_OnAction` | `string asModName, string asAction` |

Floats cannot hold every color. Read colors with `GetItemColor`.
