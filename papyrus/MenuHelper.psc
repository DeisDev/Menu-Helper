Scriptname MenuHelper Native Hidden
{Adds entries and submenus to the pause menu. Requires MenuHelper.dll.}

; Papyrus only runs in a loaded game, so Papyrus entries appear in the pause menu only.
; Everything created here is removed when a game is loaded or started. Register again
; from OnInit and after every game load (for example from OnPlayerLoadGame).
;
; Events, sent to every script that called RegisterForExternalEvent(eventName, "MyCallback"):
;   "MenuHelper_OnEntryPress"   Function MyCallback(int aiEntry)
;   "MenuHelper_OnItemChange"   Function MyCallback(int aiItem, float afValue)
;   "MenuHelper_OnMenuOpen"     Function MyCallback(int aiMenu)
;   "MenuHelper_OnMenuClose"    Function MyCallback(int aiMenu)
; Buttons send OnItemChange with 0.0 when pressed. Checkboxes send 0.0 or 1.0, steppers the
; option index, sliders the value.
;
; IDs are always greater than 0. Functions return 0 or False when they fail.

; aiInsertBefore is a vanilla entry index (10 = QUIT, 8 = SETTINGS), or -1 to add at the end.
int Function AddEntry(string asLabel, int aiSubmenu = 0, int aiInsertBefore = 10) global native
bool Function RemoveEntry(int aiEntry) global native
bool Function SetEntryLabel(int aiEntry, string asLabel) global native
bool Function SetEntryEnabled(int aiEntry, bool abEnabled) global native

int Function CreateMenu() global native
bool Function DestroyMenu(int aiMenu) global native

int Function AddButton(int aiMenu, string asLabel, string asDescription = "", int aiSubmenu = 0) global native
int Function AddCheckbox(int aiMenu, string asLabel, bool abChecked, string asDescription = "") global native
int Function AddStepper(int aiMenu, string asLabel, string[] asOptions, int aiIndex, string asDescription = "") global native
int Function AddSlider(int aiMenu, string asLabel, float afValue, float afMin, float afMax, float afStep, string asDescription = "") global native
bool Function RemoveItem(int aiItem) global native

bool Function SetItemLabel(int aiItem, string asLabel) global native
bool Function SetItemDescription(int aiItem, string asDescription) global native
bool Function SetItemEnabled(int aiItem, bool abEnabled) global native
bool Function SetItemValue(int aiItem, float afValue) global native
float Function GetItemValue(int aiItem) global native

; Opens a menu on top of the open pause menu.
bool Function OpenMenu(int aiMenu) global native
bool Function CloseMenu(int aiMenu) global native
