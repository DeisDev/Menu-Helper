# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Other F4SE plugins can add entries to the main menu and pause menu.
- Submenus with buttons, checkboxes, steppers and sliders that use the game's own Settings panels.
- Papyrus scripts can add pause menu entries and submenus and receive their events.
- Example plugin that shows every feature.
- Menu text can be translated with JSON files in `Data/Interface/MenuHelper/Translations`, without changing the mod.
- Checkboxes, steppers and sliders can save their values to an INI file.
- Mods can hide or move vanilla main and pause menu entries.
- When the main or pause menu would need to scroll, mod entries move into one MODS entry. `sModsFolder` in `MenuHelper.ini` changes this.
- `bExportTemplates` in `MenuHelper.ini` writes each mod's text to a file translators can fill in.
- German translation for the example plugin.
