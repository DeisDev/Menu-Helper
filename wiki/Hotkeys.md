# Hotkeys

A hotkey item stores a key. While no menu is open, Menu Helper calls the item's `onHotkey` when the player presses that key. It does not block the game's own use of the key.

## Key codes

| Input | Code |
| --- | --- |
| Keyboard | Windows virtual-key code, for example `'F'` = 70 |
| Mouse buttons | 256 to 263 |
| Gamepad buttons | 266 to 281 |
| None | 0 |

Add `MENUHELPER_HOTKEY_SHIFT`, `MENUHELPER_HOTKEY_CTRL` or `MENUHELPER_HOTKEY_ALT` for combinations. A hotkey with modifiers fires only while they are held.

## Setting a key

The player presses the hotkey row, then the key.

- Esc cancels.
- Backspace or Delete removes the hotkey.

Item flags:

| Flag | Allows |
| --- | --- |
| `MENUHELPER_FLAG_HOTKEY_MODIFIERS` | Shift, Ctrl and Alt combinations |
| `MENUHELPER_FLAG_HOTKEY_MOUSE` | Mouse buttons |
| `MENUHELPER_FLAG_HOTKEY_GAMEPAD` | Gamepad buttons |

`GetKeyName` returns the name shown in menus, for example `Ctrl+K`.
