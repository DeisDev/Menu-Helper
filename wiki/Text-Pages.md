# Text Pages

A menu created with `kind = MENUHELPER_MENU_PAGES` holds topics and looks like vanilla HELP. A menu with one topic shows only that page.

| Topic field | Use |
| --- | --- |
| `label` | Title |
| `description` | Text. `<b>`, `<i>` and `<br>` work. Text uses the HUD color. |
| `image` | Optional DDS file under `Data`, for example `Textures/MyMod/logo.dds`. Shown above the text in its own colors. |
| `imageWidth`, `imageHeight` | Image size on the 1280x720 menu. 0 keeps the file's size. |

- Only DDS files work.
- A missing image is logged and the page shows without it.
- Players scroll the text with Up and Down or the mouse wheel. Left and Right move between the topic list and the text.
