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
- Settings pages can have dropdowns, hotkeys, colors, headers and text rows.
- Sliders show their value, with an optional unit format.
- Settings pages have a DEFAULTS button that restores the original values.
- Items can be hidden and report when they are selected.
- Hotkeys set in a menu run their mod's action when pressed in game.
- Mods can show vanilla message boxes, yes/no questions and HUD notifications.
- Menus of text pages, like vanilla HELP, with optional DDS images.
- Mods can add menus with a JSON file and no code.
- Plugins and Papyrus can read and write saved values and hear about setting changes and actions.
- Plugins and JSON menus made for an older Menu Helper keep working with newer versions.
- Papyrus scripts can read the installed API version with `GetAPIVersion`.

### Fixed

- The game no longer crashes when the HUD color changes while a MenuHelper menu is open.
