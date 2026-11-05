# Contributing

## Setup

Build steps are on the [Building](https://github.com/DeisDev/Menu-Helper/wiki/Building) wiki page.

To test, copy `MenuHelper.dll` and `MenuHelperExample.dll` to `Data/F4SE/Plugins`, `swf/MenuHelper.swf` to `Data/Interface/MenuHelper`, and the `res` and `examples/basic` files to the matching `Data` folders (see `xmake.lua`). The example plugin uses every feature.

## Rules

- Do not ship or require a replaced vanilla SWF.
- The C API in `include/MenuHelper/MenuHelper.h` is versioned. Descriptor structs start with `structSize`. Keep the header comments up to date; they are the API reference.
- Keep user-facing text in translation files, not in code.
- If you change `swf/src` or `papyrus/MenuHelper.psc`, rebuild and commit `swf/MenuHelper.swf` or `papyrus/MenuHelper.pex` too.
- Test changes in game, in the main menu and the pause menu.

## Documentation

- The wiki is built from `wiki/` in this repository. Edit the files there, not the wiki itself.
- Add a line to `CHANGELOG.md` under `Unreleased` for each notable change.
- Keep documentation short and plain.

## Commits and pull requests

- Use [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/), for example `fix(submenus): keep selection after refresh`.
- One topic per pull request. Describe what you tested.
