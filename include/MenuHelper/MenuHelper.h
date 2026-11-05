#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENUHELPER_API_VERSION 1u

typedef uint32_t MenuHelper_EntryId;
typedef uint32_t MenuHelper_MenuId;
typedef uint32_t MenuHelper_ItemId;
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

enum MenuHelper_ItemType {
	MENUHELPER_ITEM_BUTTON = 0,   /* value unused */
	MENUHELPER_ITEM_CHECKBOX = 1, /* value 0 or 1 */
	MENUHELPER_ITEM_STEPPER = 2,  /* value is the index into options */
	MENUHELPER_ITEM_SLIDER = 3    /* value in [min, max], snapped to step */
};

/* Callbacks run on the game's UI thread while the main or pause menu is open. */
typedef void (*MenuHelper_PressCallback)(MenuHelper_EntryId entry, uint32_t menu, void* user);
typedef void (*MenuHelper_ItemCallback)(MenuHelper_ItemId item, float value, void* user);
typedef void (*MenuHelper_MenuCallback)(MenuHelper_MenuId menu, void* user);

typedef struct MenuHelper_EntryDesc {
	uint32_t                 structSize;   /* sizeof(MenuHelper_EntryDesc) */
	const char*              label;        /* UTF-8 plain text. "$KEY" strings are localized by the game. */
	uint32_t                 menus;        /* MenuHelper_Menu bits */
	int32_t                  insertBefore; /* MenuHelper_VanillaEntry, or MENUHELPER_VANILLA_APPEND */
	MenuHelper_PressCallback onPress;      /* Optional. */
	void*                    user;
	MenuHelper_MenuId        submenu;      /* Optional. Opened after onPress returns. */
} MenuHelper_EntryDesc;

typedef struct MenuHelper_MenuDesc {
	uint32_t                structSize; /* sizeof(MenuHelper_MenuDesc) */
	MenuHelper_MenuCallback onOpen;     /* Optional. Called before the menu is shown; item changes made here are shown. */
	MenuHelper_MenuCallback onClose;    /* Optional. Called after the menu closes for any reason. */
	void*                   user;
} MenuHelper_MenuDesc;

typedef struct MenuHelper_ItemDesc {
	uint32_t                structSize;  /* sizeof(MenuHelper_ItemDesc) */
	uint32_t                type;        /* MenuHelper_ItemType */
	const char*             label;       /* UTF-8 plain text. "$KEY" strings are localized by the game. */
	const char*             description; /* Optional. Shown under the list while the item is selected. */
	float                   value;       /* Initial value, see MenuHelper_ItemType. */
	float                   min;         /* Slider only. */
	float                   max;         /* Slider only. Must be greater than min. */
	float                   step;        /* Slider only. Must be greater than 0. */
	const char* const*      options;     /* Stepper only. optionCount UTF-8 strings. */
	uint32_t                optionCount; /* Stepper only. At least 1. */
	MenuHelper_MenuId       submenu;     /* Button only, optional. Opened on top after onChange returns. */
	MenuHelper_ItemCallback onChange;    /* Optional. Buttons: on press with value 0. Others: on value change. */
	void*                   user;
} MenuHelper_ItemDesc;

/* Functions may be called from any thread. Integer results are 1 on success and 0 on failure. */
typedef struct MenuHelper_API {
	uint32_t version;

	MenuHelper_EntryId (*AddEntry)(const MenuHelper_EntryDesc* desc);
	int (*RemoveEntry)(MenuHelper_EntryId entry);
	int (*SetEntryLabel)(MenuHelper_EntryId entry, const char* label);
	int (*SetEntryEnabled)(MenuHelper_EntryId entry, int enabled);

	/* Menus with only buttons look like the vanilla Settings category list.
	   Menus with a checkbox, stepper, slider or any description look like a vanilla Settings page. */
	MenuHelper_MenuId (*CreateMenu)(const MenuHelper_MenuDesc* desc);
	int (*DestroyMenu)(MenuHelper_MenuId menu);
	MenuHelper_ItemId (*AddItem)(MenuHelper_MenuId menu, const MenuHelper_ItemDesc* desc);
	int (*RemoveItem)(MenuHelper_ItemId item);
	int (*SetItemLabel)(MenuHelper_ItemId item, const char* label);
	int (*SetItemDescription)(MenuHelper_ItemId item, const char* description);
	int (*SetItemEnabled)(MenuHelper_ItemId item, int enabled);
	int (*SetItemValue)(MenuHelper_ItemId item, float value);
	float (*GetItemValue)(MenuHelper_ItemId item);

	/* Opens a menu on top of the open main or pause menu. Fails when neither is open. */
	int (*OpenMenu)(MenuHelper_MenuId menu);
	/* Closes the menu and every menu opened on top of it. */
	int (*CloseMenu)(MenuHelper_MenuId menu);
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
