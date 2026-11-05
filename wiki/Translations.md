# Translations

Labels, descriptions, options and slider formats are looked up in translation files before they are shown. Anyone can translate a mod without changing it.

A translation file maps the original text to the translation:

`Data/Interface/MenuHelper/Translations/MyMod_de.json`

```json
{
    "SHOW MARKERS": "MARKIERUNGEN ANZEIGEN",
    "Shows map markers.": "Zeigt Kartenmarkierungen an."
}
```

- `MyMod` is the mod name. `de` is the game language (`sLanguage` in `Fallout4.ini`).
- Missing text falls back to `MyMod_en.json`, then to the original text.
- Files are UTF-8. `//` comments work.
- Files load once. Restart the game after editing one.
- Menu Helper's own text uses the mod name `MenuHelper` (`MenuHelper_en.json`).

## Text built at runtime

Translate a whole message with named placeholders, then fill them in:

- Plugin: `api->Translate("MyMod", "PRESSED {count}")`
- Papyrus: `MenuHelper.FillPlaceholder(MenuHelper.Translate("MyMod", "PRESSED {count}"), "count", 3)`

## Templates

1. Set `bExportTemplates = 1` in `MenuHelper.ini`.
2. Start the game and open the mod's menus.
3. Copy `<mod>_template.json` to `<mod>_<language>.json` and translate the values.
