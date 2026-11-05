# Dialogs

| Function | Shows | Where |
| --- | --- | --- |
| `ShowMessageBox` | A vanilla message box with 1 to 4 buttons | Main menu or in game |
| `ShowConfirm` | A yes/no question over the open Menu Helper menu | Main or pause menu, with a Menu Helper menu open |
| `ShowNotification` | Text at the top left of the HUD, with an optional sound such as `UIMenuOK` | In game |

- The message box callback gets the index of the pressed button.
- The confirm callback runs exactly once: 1 for yes, 0 for no or when the menu closes.
- All text is translated with the mod's translation files.
