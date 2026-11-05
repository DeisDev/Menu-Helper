#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENUHELPER_API_VERSION 1u

typedef uint32_t MenuHelper_EntryId;
typedef uint32_t MenuHelper_MenuId;
typedef uint32_t MenuHelper_ItemId;
typedef uint32_t MenuHelper_ChangeId;
typedef uint32_t MenuHelper_BoxId;
#define MENUHELPER_INVALID_ID 0u

enum MenuHelper_Menu {
	MENUHELPER_MENU_MAIN = 1u << 0,
	MENUHELPER_MENU_PAUSE = 1u << 1,
	MENUHELPER_MENU_ALL = MENUHELPER_MENU_MAIN | MENUHELPER_MENU_PAUSE
};

/* Vanilla entry indices from MainMenu.as. Pass one as insertBefore to place an entry above it. */
enum MenuHelper_VanillaEntry {
	MENUHELPER_VANILLA_APPEND = -1,
	MENUHELPER_VANILLA_CONTINUE = 0,
	MENUHELPER_VANILLA_NEW = 1,
	MENUHELPER_VANILLA_QUICKSAVE = 2,
	MENUHELPER_VANILLA_SAVE = 3,
	MENUHELPER_VANILLA_LOAD = 4,
	MENUHELPER_VANILLA_DLC = 5,
	MENUHELPER_VANILLA_BETHESDANET = 6,
	MENUHELPER_VANILLA_HELP = 7,
	MENUHELPER_VANILLA_SETTINGS = 8,
	MENUHELPER_VANILLA_CREDITS = 9,
	MENUHELPER_VANILLA_QUIT = 10,
	MENUHELPER_VANILLA_CREATIONCLUB = 12,
	MENUHELPER_VANILLA_TRANSFERSAVE = 13,
	MENUHELPER_VANILLA_INSTALLED_CONTENT = 14
};

enum MenuHelper_MenuKind {
	MENUHELPER_MENU_ITEMS = 0, /* A list of items. */
	MENUHELPER_MENU_PAGES = 1  /* Text pages: MENUHELPER_ITEM_TOPIC items only, shown like vanilla HELP. */
};

enum MenuHelper_ItemType {
	MENUHELPER_ITEM_BUTTON = 0,   /* value unused */
	MENUHELPER_ITEM_CHECKBOX = 1, /* value 0 or 1 */
	MENUHELPER_ITEM_STEPPER = 2,  /* value is the index into options */
	MENUHELPER_ITEM_SLIDER = 3,   /* value in [min, max], snapped to step */
	MENUHELPER_ITEM_DROPDOWN = 4, /* value is the index into options, picked from a popup list */
	MENUHELPER_ITEM_HOTKEY = 5,   /* value is a hotkey (see "Hotkeys"), 0 for none */
	MENUHELPER_ITEM_COLOR = 6,    /* value is 0xAARRGGBB, edited with red, green and blue sliders */
	MENUHELPER_ITEM_HEADER = 7,   /* label only, cannot be selected */
	MENUHELPER_ITEM_TEXT = 8,     /* label and description, does nothing when pressed */
	MENUHELPER_ITEM_TOPIC = 9     /* page of a MENUHELPER_MENU_PAGES menu: label is the title, description the text */
};

enum MenuHelper_ItemFlag {
	MENUHELPER_FLAG_HIDDEN = 1u << 0,           /* The item is not shown. See SetItemVisible. */
	MENUHELPER_FLAG_HASALPHA = 1u << 1,         /* Color: also edit the alpha channel. */
	MENUHELPER_FLAG_HOTKEY_MODIFIERS = 1u << 2, /* Hotkey: allow Shift, Ctrl and Alt combinations. */
	MENUHELPER_FLAG_HOTKEY_MOUSE = 1u << 3,     /* Hotkey: allow mouse buttons. */
	MENUHELPER_FLAG_HOTKEY_GAMEPAD = 1u << 4    /* Hotkey: allow gamepad buttons. */
};

/* Hotkeys
   A hotkey value is a key code plus modifier bits. Key codes are the game's and F4SE's: 1-255 keyboard
   keys as Windows virtual-key codes (VK_ values), 256-263 mouse buttons, 266-281 gamepad buttons.
   0 means none. Hotkeys with modifier bits fire only while those modifiers are held.
   While no menu is open, MenuHelper calls the item's onHotkey when the player presses its hotkey. */
enum MenuHelper_HotkeyBits {
	MENUHELPER_HOTKEY_KEYMASK = 0xFFFFu,
	MENUHELPER_HOTKEY_SHIFT = 1u << 16,
	MENUHELPER_HOTKEY_CTRL = 1u << 17,
	MENUHELPER_HOTKEY_ALT = 1u << 18
};

/* Text
   Write labels, descriptions and options in English, or as "$KEY" keys. Before showing one,
   MenuHelper looks it up in these UTF-8 JSON files, so anyone can translate a mod later without code:
     Data/Interface/MenuHelper/Translations/<modName>_<language>.json
     Data/Interface/MenuHelper/Translations/<modName>_en.json
   Each file is one object that maps the original text to its translation. <language> is the game's
   sLanguage setting (en, de, fr, ...). Text found in neither file is shown as is, and "$KEY" text is
   then translated by the game like vanilla labels.

   modName names your mod. It is part of file names, so it may not contain \ / : * ? " < > | */

/* Callbacks run on the game's UI thread. Entry, menu and item callbacks other than onHotkey run while the main
   or pause menu is open. */
typedef void (*MenuHelper_PressCallback)(MenuHelper_EntryId entry, uint32_t menu, void* user);
typedef void (*MenuHelper_ItemCallback)(MenuHelper_ItemId item, double value, void* user);
typedef void (*MenuHelper_MenuCallback)(MenuHelper_MenuId menu, void* user);
/* button is the index into buttons. */
typedef void (*MenuHelper_MessageBoxCallback)(MenuHelper_BoxId box, uint32_t button, void* user);
/* result is 1 when the player confirmed, 0 when they cancelled or the menu closed. */
typedef void (*MenuHelper_ConfirmCallback)(MenuHelper_BoxId confirm, int result, void* user);
typedef void (*MenuHelper_SettingCallback)(const char* modName, const char* saveKey, double value, void* user);
typedef void (*MenuHelper_ActionCallback)(const char* modName, const char* action, void* user);

typedef struct MenuHelper_EntryDesc {
	uint32_t                 structSize;   /* sizeof(MenuHelper_EntryDesc) */
	const char*              modName;      /* Required. See "Text". */
	const char*              label;        /* Required. */
	uint32_t                 menus;        /* MenuHelper_Menu bits */
	int32_t                  insertBefore; /* MenuHelper_VanillaEntry, or MENUHELPER_VANILLA_APPEND */
	MenuHelper_PressCallback onPress;      /* Optional. */
	void*                    user;
	MenuHelper_MenuId        submenu;      /* Optional. Opened after onPress returns. */
} MenuHelper_EntryDesc;

typedef struct MenuHelper_MenuDesc {
	uint32_t                structSize; /* sizeof(MenuHelper_MenuDesc) */
	const char*             modName;    /* Required. Used for the text and saved values of the menu's items. */
	uint32_t                kind;       /* MenuHelper_MenuKind */
	MenuHelper_MenuCallback onOpen;     /* Optional. Called before the menu is shown; item changes made here are shown. */
	MenuHelper_MenuCallback onClose;    /* Optional. Called after the menu closes for any reason. */
	/* Optional. Menus with checkboxes, steppers, sliders, dropdowns, hotkeys or colors show a DEFAULTS
	   button. When the player confirms it, every visible one of them goes back to the value it was added
	   with, onChange runs for each that changed, and then onReset runs. */
	MenuHelper_MenuCallback onReset;
	void*                   user;
} MenuHelper_MenuDesc;

typedef struct MenuHelper_ItemDesc {
	uint32_t                structSize;  /* sizeof(MenuHelper_ItemDesc) */
	uint32_t                type;        /* MenuHelper_ItemType */
	uint32_t                flags;       /* MenuHelper_ItemFlag bits */
	const char*             label;       /* Required. */
	/* Optional. Shown next to the list while the item is selected. Topics: the page text, which may use <b>, <i>
	   and <br>; it is shown in the HUD color. */
	const char*             description;
	double                  value;       /* Initial value, see MenuHelper_ItemType. Also the value DEFAULTS restores. */
	double                  min;         /* Slider only. */
	double                  max;         /* Slider only. Must be greater than min. */
	double                  step;        /* Slider only. Must be greater than 0. */
	/* Slider only, optional. How the value is shown next to the slider, with {value} replaced by the
	   value. Translated like labels, so units can change per language. Default "{value}". */
	const char*             format;
	uint32_t                decimals;    /* Slider only. Digits after the decimal point. 0 shows as many as step and min need. */
	const char* const*      options;     /* Stepper and dropdown. optionCount strings. */
	uint32_t                optionCount; /* Stepper and dropdown. At least 1. */
	MenuHelper_MenuId       submenu;     /* Button only, optional. Opened on top after onChange returns. */
	/* Optional. Buttons: on press with value 0. Others: when the player changes the value. */
	MenuHelper_ItemCallback onChange;
	MenuHelper_ItemCallback onSelect;    /* Optional. When the item becomes the selected row. */
	MenuHelper_ItemCallback onHotkey;    /* Hotkey only, optional. When the player presses the hotkey in game. */
	void*                   user;
	/* Optional, only for value items. Saves the value under this key in
	   Data/F4SE/Plugins/MenuHelper/Settings/<modName>.ini whenever it changes. AddItem then starts from
	   the saved value instead of value; read it back with GetItemValue. */
	const char*             saveKey;
	/* Topic only, optional. A DDS file under Data, for example "Textures/MyMod/logo.dds", shown above the
	   text in its own colors. imageWidth and imageHeight are its size on the 1280x720 menu; 0 keeps the file's size. */
	const char*             image;
	uint32_t                imageWidth;
	uint32_t                imageHeight;
} MenuHelper_ItemDesc;

/* Changes a vanilla entry of the main menu, the pause menu, or both.
   Changes apply in the order they were made, after mod entries are placed. */
typedef struct MenuHelper_VanillaChangeDesc {
	uint32_t structSize; /* sizeof(MenuHelper_VanillaChangeDesc) */
	uint32_t menus;      /* MenuHelper_Menu bits */
	int32_t  entry;      /* MenuHelper_VanillaEntry, not MENUHELPER_VANILLA_APPEND */
	int32_t  hidden;     /* Nonzero hides the entry. */
	int32_t  move;       /* Rows to move the entry by. Positive moves it down. */
} MenuHelper_VanillaChangeDesc;

/* A vanilla message box, at the main menu or in game. The game shows one box at a time. */
typedef struct MenuHelper_MessageBoxDesc {
	uint32_t                      structSize;  /* sizeof(MenuHelper_MessageBoxDesc) */
	const char*                   modName;     /* Required. Used to translate the text. */
	const char*                   body;        /* Required. */
	const char* const*            buttons;     /* Required. buttonCount labels. */
	uint32_t                      buttonCount; /* 1 to 4 */
	MenuHelper_MessageBoxCallback onButton;    /* Optional. Called once with the button the player picked. */
	void*                         user;
} MenuHelper_MessageBoxDesc;

/* A yes/no question shown over the open MenuHelper menu, like vanilla's "Reset settings to default values?". */
typedef struct MenuHelper_ConfirmDesc {
	uint32_t                  structSize; /* sizeof(MenuHelper_ConfirmDesc) */
	const char*               modName;    /* Required. Used to translate the text. */
	const char*               text;       /* Required. */
	MenuHelper_ConfirmCallback onResult;  /* Optional. Called exactly once. */
	void*                     user;
} MenuHelper_ConfirmDesc;

/* Functions may be called from any thread. Integer results are 1 on success and 0 on failure. */
typedef struct MenuHelper_API {
	uint32_t version;

	/* When the main or pause menu would have more rows than it shows, all mod entries move into one
	   MODS entry that opens them as a list. Players can change this in MenuHelper.ini. */
	MenuHelper_EntryId (*AddEntry)(const MenuHelper_EntryDesc* desc);
	int (*RemoveEntry)(MenuHelper_EntryId entry);
	int (*SetEntryLabel)(MenuHelper_EntryId entry, const char* label);
	int (*SetEntryEnabled)(MenuHelper_EntryId entry, int enabled);

	/* Item menus with only buttons, headers and text look like the vanilla Settings category list.
	   Item menus with any other item or a button description look like a vanilla Settings page. */
	MenuHelper_MenuId (*CreateMenu)(const MenuHelper_MenuDesc* desc);
	int (*DestroyMenu)(MenuHelper_MenuId menu);
	MenuHelper_ItemId (*AddItem)(MenuHelper_MenuId menu, const MenuHelper_ItemDesc* desc);
	int (*RemoveItem)(MenuHelper_ItemId item);
	int (*SetItemLabel)(MenuHelper_ItemId item, const char* label);
	int (*SetItemDescription)(MenuHelper_ItemId item, const char* description);
	int (*SetItemEnabled)(MenuHelper_ItemId item, int enabled);
	int (*SetItemVisible)(MenuHelper_ItemId item, int visible);
	/* Also saves the value when the item has a saveKey. Does not call onChange. */
	int (*SetItemValue)(MenuHelper_ItemId item, double value);
	double (*GetItemValue)(MenuHelper_ItemId item);

	/* Opens a menu on top of the open main or pause menu. Fails when neither is open. */
	int (*OpenMenu)(MenuHelper_MenuId menu);
	/* Closes the menu and every menu opened on top of it. */
	int (*CloseMenu)(MenuHelper_MenuId menu);

	MenuHelper_ChangeId (*ChangeVanillaEntry)(const MenuHelper_VanillaChangeDesc* desc);
	int (*RemoveVanillaChange)(MenuHelper_ChangeId change);

	/* Returns the translation of text for modName (see "Text"), or text itself when there is none or the
	   game has not loaded its settings yet. A returned translation stays valid until the game exits.
	   Use it for text you build yourself: translate a whole message with named placeholders such as
	   "PRESSED {count}", then fill them in. */
	const char* (*Translate)(const char* modName, const char* text);
	/* Returns the name of a hotkey as shown in menus, for example "Shift+F". Valid until the game exits. */
	const char* (*GetKeyName)(uint32_t hotkey);

	/* Returns 0 when the box cannot be shown. */
	MenuHelper_BoxId (*ShowMessageBox)(const MenuHelper_MessageBoxDesc* desc);
	/* Fails when no MenuHelper menu is open. */
	MenuHelper_BoxId (*ShowConfirm)(const MenuHelper_ConfirmDesc* desc);
	/* Shows text at the top left of the HUD, like vanilla notifications. sound is optional, for example
	   "UIMenuOK". warning uses the warning color. Fails when the HUD is not shown. */
	int (*ShowNotification)(const char* modName, const char* text, const char* sound, int warning);

	/* Read or write a saved value directly, also for items another plugin or a JSON menu added.
	   SetSavedValue also updates items with that key. GetSavedValue fails when nothing is saved. */
	int (*GetSavedValue)(const char* modName, const char* saveKey, double* value);
	int (*SetSavedValue)(const char* modName, const char* saveKey, double value);
	/* The listener runs whenever the player changes an item of modName that has a saveKey. */
	int (*RegisterSettingListener)(const char* modName, MenuHelper_SettingCallback callback, void* user);
	/* The listener runs when the player presses a JSON menu button or hotkey of modName that has an action. */
	int (*RegisterActionListener)(const char* modName, MenuHelper_ActionCallback callback, void* user);
} MenuHelper_API;

/* Exported by MenuHelper.dll. Returns NULL when the requested version is not supported. */
typedef const MenuHelper_API* (*MenuHelper_GetAPI_t)(uint32_t requestedVersion);

#ifdef __cplusplus
}
#endif

/* Optional runtime loader, available when <Windows.h> was included before this header.
   Call after all F4SE plugins have loaded (F4SE kPostLoad or later).
   Without <Windows.h>, resolve "MenuHelper_GetAPI" from GetModuleHandleW(L"MenuHelper.dll") yourself. */
#if defined(_WINDOWS_) && !defined(MENUHELPER_NO_LOADER)
static inline const MenuHelper_API* MenuHelper_Load(void)
{
	HMODULE module = GetModuleHandleW(L"MenuHelper.dll");
	if (!module) {
		return NULL;
	}
	MenuHelper_GetAPI_t getApi = (MenuHelper_GetAPI_t)GetProcAddress(module, "MenuHelper_GetAPI");
	if (!getApi) {
		return NULL;
	}
	return getApi(MENUHELPER_API_VERSION);
}
#endif
