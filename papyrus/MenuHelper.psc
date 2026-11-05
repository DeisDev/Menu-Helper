Scriptname MenuHelper Native Hidden
{Adds entries and submenus to the pause menu, and shows dialogs. Requires MenuHelper.dll.}

; Papyrus only runs in a loaded game, so Papyrus entries and changes apply to the pause menu only.
; Entries, menus and vanilla changes created here are removed when a game is loaded or started.
; Register again from OnInit and after every game load (for example from OnPlayerLoadGame).
;
; asModName names your mod. Text is looked up in
; Data/Interface/MenuHelper/Translations/<asModName>_<language>.json, so anyone can translate
; your menu later. Items with asSaveKey keep their value in
; Data/F4SE/Plugins/MenuHelper/Settings/<asModName>.ini; read it back with GetItemValue.
;
; Events, sent to every script that called RegisterForExternalEvent(eventName, "MyCallback"):
;   "MenuHelper_OnEntryPress"         Function MyCallback(int aiEntry)
;   "MenuHelper_OnItemChange"         Function MyCallback(int aiItem, float afValue)
;   "MenuHelper_OnItemSelect"         Function MyCallback(int aiItem)
;   "MenuHelper_OnHotkey"             Function MyCallback(int aiItem)
;   "MenuHelper_OnMenuOpen"           Function MyCallback(int aiMenu)
;   "MenuHelper_OnMenuClose"          Function MyCallback(int aiMenu)
;   "MenuHelper_OnMenuReset"          Function MyCallback(int aiMenu)
;   "MenuHelper_OnMessageBoxButton"   Function MyCallback(int aiBox, int aiButton)
;   "MenuHelper_OnConfirm"            Function MyCallback(int aiConfirm, bool abResult)
;   "MenuHelper_OnSettingChange"      Function MyCallback(string asModName, string asSaveKey, float afValue)
;   "MenuHelper_OnAction"             Function MyCallback(string asModName, string asAction)
; Buttons send OnItemChange with 0.0 when pressed. Checkboxes send 0.0 or 1.0, steppers and
; dropdowns the option index, sliders the value. Floats cannot hold every color, so read colors
; with GetItemColor. OnHotkey is sent when the player presses a hotkey in game.
; OnSettingChange and OnAction also come from other mods, including JSON menus; compare asModName.
;
; IDs are always greater than 0. Functions return 0 or False when they fail.

; aiInsertBefore is a vanilla entry index (10 = QUIT, 8 = SETTINGS), or -1 to add at the end.
int Function AddEntry(string asModName, string asLabel, int aiSubmenu = 0, int aiInsertBefore = 10) global native
bool Function RemoveEntry(int aiEntry) global native
bool Function SetEntryLabel(int aiEntry, string asLabel) global native
bool Function SetEntryEnabled(int aiEntry, bool abEnabled) global native

; abPages creates a menu of text pages, shown like vanilla HELP. It holds topics only.
int Function CreateMenu(string asModName, bool abPages = false) global native
bool Function DestroyMenu(int aiMenu) global native

int Function AddButton(int aiMenu, string asLabel, string asDescription = "", int aiSubmenu = 0) global native
int Function AddCheckbox(int aiMenu, string asLabel, bool abChecked, string asDescription = "", string asSaveKey = "") global native
; asOptions must not be None: passing None crashes the game.
int Function AddStepper(int aiMenu, string asLabel, string[] asOptions, int aiIndex, string asDescription = "", string asSaveKey = "") global native
; Like AddStepper, but the player picks the option from a list. asOptions must not be None.
int Function AddDropdown(int aiMenu, string asLabel, string[] asOptions, int aiIndex, string asDescription = "", string asSaveKey = "") global native
; asFormat shows the value next to the slider, for example "{value} m". aiDecimals 0 shows as many digits as afStep needs.
int Function AddSlider(int aiMenu, string asLabel, float afValue, float afMin, float afMax, float afStep, string asDescription = "", string asSaveKey = "", string asFormat = "", int aiDecimals = 0) global native
; aiHotkey is a key code: 1-255 keyboard (Windows virtual-key codes), 256-263 mouse buttons, 266-281 gamepad buttons,
; plus 65536 for Shift, 131072 for Ctrl and 262144 for Alt. 0 is no hotkey.
int Function AddHotkey(int aiMenu, string asLabel, int aiHotkey = 0, string asDescription = "", string asSaveKey = "", bool abModifiers = false, bool abMouse = false, bool abGamepad = false) global native
; aiColor is 0xAARRGGBB. abAlpha also lets the player change the alpha.
int Function AddColor(int aiMenu, string asLabel, int aiColor, string asDescription = "", string asSaveKey = "", bool abAlpha = false) global native
; A row that cannot be selected.
int Function AddHeader(int aiMenu, string asLabel) global native
; A row that shows asDescription when selected and does nothing when pressed.
int Function AddText(int aiMenu, string asLabel, string asDescription = "") global native
; A page of a pages menu. asText may use <b>, <i> and <br>; it is shown in the HUD color.
; asImage is a DDS file under Data, for example "Textures/MyMod/logo.dds", shown above the text.
int Function AddTopic(int aiMenu, string asTitle, string asText, string asImage = "", int aiImageWidth = 0, int aiImageHeight = 0) global native
bool Function RemoveItem(int aiItem) global native

bool Function SetItemLabel(int aiItem, string asLabel) global native
bool Function SetItemDescription(int aiItem, string asDescription) global native
bool Function SetItemEnabled(int aiItem, bool abEnabled) global native
bool Function SetItemVisible(int aiItem, bool abVisible) global native
bool Function SetItemValue(int aiItem, float afValue) global native
float Function GetItemValue(int aiItem) global native
bool Function SetItemColor(int aiItem, int aiColor) global native
int Function GetItemColor(int aiItem) global native
bool Function SetItemHotkey(int aiItem, int aiHotkey) global native
int Function GetItemHotkey(int aiItem) global native
; The name of a hotkey as shown in menus, for example "Shift+F".
string Function GetKeyName(int aiHotkey) global native

; Opens a menu on top of the open pause menu.
bool Function OpenMenu(int aiMenu) global native
bool Function CloseMenu(int aiMenu) global native

; Hides a vanilla pause menu entry (aiEntry as in AddEntry) or moves it by aiMove rows; positive moves down.
int Function ChangeVanillaEntry(int aiEntry, bool abHidden, int aiMove = 0) global native
bool Function RemoveVanillaChange(int aiChange) global native

; A vanilla message box with 1 to 4 buttons. asButtons must not be None. Sends OnMessageBoxButton.
int Function ShowMessageBox(string asModName, string asBody, string[] asButtons) global native
; A yes/no question over the open MenuHelper menu. Sends OnConfirm.
int Function ShowConfirm(string asModName, string asText) global native
; Text at the top left of the HUD. asSound is for example "UIMenuOK".
bool Function ShowNotification(string asModName, string asText, string asSound = "", bool abWarning = false) global native

; Saved values of any mod, also without an item. Set also updates items with that key.
float Function GetSavedFloat(string asModName, string asSaveKey, float afDefault = 0.0) global native
int Function GetSavedInt(string asModName, string asSaveKey, int aiDefault = 0) global native
bool Function GetSavedBool(string asModName, string asSaveKey, bool abDefault = false) global native
int Function GetSavedColor(string asModName, string asSaveKey, int aiDefault = 0) global native
bool Function SetSavedFloat(string asModName, string asSaveKey, float afValue) global native
bool Function SetSavedInt(string asModName, string asSaveKey, int aiValue) global native
bool Function SetSavedBool(string asModName, string asSaveKey, bool abValue) global native
bool Function SetSavedColor(string asModName, string asSaveKey, int aiValue) global native

; Returns the translation of asText, or asText itself.
string Function Translate(string asModName, string asText) global native
; Replaces every {asName} in asText with asValue. Translate first, then fill in, for example:
; FillPlaceholder(Translate("MyMod", "PRESSED {count}"), "count", 3)
string Function FillPlaceholder(string asText, string asName, string asValue) global native
