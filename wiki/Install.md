# Install

Copy the `Data` folder from the release archive into the game folder, or install the archive with a mod manager.

The archive contains:

- `Data/F4SE/Plugins/MenuHelper.dll` and `MenuHelper.ini`
- `Data/Interface/MenuHelper/MenuHelper.swf` and `Translations/MenuHelper_en.json`
- `Data/Scripts/MenuHelper.pex` and `Data/Scripts/Source/User/MenuHelper.psc`

## Settings

`Data/F4SE/Plugins/MenuHelper.ini`:

| Setting | Values |
| --- | --- |
| `sModsFolder` | `auto`: move all mod entries into one MODS entry when the menu would need to scroll. `always`: always. `never`: never; the list scrolls. |
| `bExportTemplates` | `1`: write translation templates. See [Translations](https://github.com/DeisDev/Menu-Helper/wiki/Translations). |

## Saved values

Items with a save key keep their values in `Data/F4SE/Plugins/MenuHelper/Settings/<mod>.ini`.

## Compatibility

- Needs the vanilla `Interface/MainMenu.swf`. If another mod replaces it, Menu Helper logs a warning and adds nothing.
- Entries from other mods, such as MCM's MOD CONFIG, keep their place.

Log: `Documents/My Games/Fallout4/F4SE/MenuHelper.log`. Its first line shows the installed version.
