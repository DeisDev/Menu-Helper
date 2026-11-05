# JSON Menus

A mod can add a menu without code. Put one file at:

`Data/MenuHelper/Config/<ModName>/menu.json`

The folder name is the mod name. Menu Helper reads these files once when the game starts.

## Example

```json
{
    "version": 1,
    "entries": [
        { "label": "MY MOD", "menus": ["main", "pause"], "insertBefore": "quit", "menu": "settings" }
    ],
    "menus": {
        "settings": {
            "kind": "items",
            "items": [
                { "type": "header", "label": "GENERAL" },
                { "type": "checkbox", "label": "SHOW MARKERS", "description": "Shows map markers.", "saveKey": "bShowMarkers", "default": true },
                { "type": "slider", "label": "RANGE", "saveKey": "fRange", "default": 50, "min": 0, "max": 100, "step": 5, "format": "{value} m" },
                { "type": "dropdown", "label": "MODE", "saveKey": "iMode", "default": 0, "options": ["A", "B"] },
                { "type": "hotkey", "label": "TOGGLE", "saveKey": "iToggleKey", "default": 0, "modifiers": true, "action": "toggle" },
                { "type": "color", "label": "TINT", "saveKey": "uTint", "default": "#FF00FF" },
                { "type": "button", "label": "ABOUT", "menu": "about" },
                { "type": "button", "label": "RESET STATS", "action": "resetStats", "papyrus": "MyMod_Script.OnMenuAction" }
            ]
        },
        "about": {
            "kind": "pages",
            "items": [
                { "type": "topic", "label": "ABOUT", "text": "<b>My Mod</b><br>Version 1.0", "image": "Textures/MyMod/logo.dds" }
            ]
        }
    }
}
```

`examples/basic/Config/MenuHelperExample/menu.json` in the repository uses every item type.

## Entries

| Field | Use |
| --- | --- |
| `label` | Required. |
| `menus` | `"main"`, `"pause"` or both. Default: both. |
| `insertBefore` | Vanilla entry name in lower case (`quit`, `settings`, `help`, `load`, ...) or its number. Default: `quit`. |
| `menu` | Menu to open. |

## Menus

| Field | Use |
| --- | --- |
| `kind` | `"items"` (default) or `"pages"`. |
| `items` | List of items. Pages menus hold `topic` items only. |

## Items

| Field | Use |
| --- | --- |
| `type` | `button`, `checkbox`, `stepper`, `slider`, `dropdown`, `hotkey`, `color`, `header`, `text`, `topic` |
| `label` | Required. |
| `description` | Text shown next to the list. |
| `text` | Topic text. |
| `saveKey` | Required for value items. |
| `default` | `true`/`false` for checkboxes, `"#RRGGBB"` or `"#RRGGBBAA"` for colors, a number otherwise. |
| `min`, `max`, `step` | Required for sliders. |
| `format`, `decimals` | Slider value text. |
| `options` | Required for steppers and dropdowns. |
| `modifiers`, `mouse`, `gamepad` | Hotkey options. |
| `alpha` | Color with alpha. |
| `menu` | Buttons: menu to open. |
| `action` | Buttons and hotkeys: reported when pressed. |
| `papyrus` | Buttons and hotkeys: global Papyrus function to call, `"Script.Function"`. |
| `image`, `imageWidth`, `imageHeight` | Topic image. See [Text Pages](https://github.com/DeisDev/Menu-Helper/wiki/Text-Pages). |

## Actions

When a button or hotkey with `action` is pressed, Menu Helper:

- sends the Papyrus event `MenuHelper_OnAction(string asModName, string asAction)`,
- calls plugins registered with `RegisterActionListener`,
- calls the `papyrus` function, if set. It must be `Function Name(string asModName, string asAction) global`. This only works in a loaded game.

## Reading values

- Papyrus: `MenuHelper.GetSavedFloat`, `GetSavedInt`, `GetSavedBool`, `GetSavedColor`, and the event `MenuHelper_OnSettingChange`.
- Plugins: `GetSavedValue` and `RegisterSettingListener`.

## Errors

A file with an unknown field or a bad value is skipped. The log names the file and the field.
