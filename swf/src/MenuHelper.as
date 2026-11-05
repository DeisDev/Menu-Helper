package
{
	import flash.display.DisplayObject;
	import flash.display.Loader;
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.events.Event;
	import flash.events.IOErrorEvent;
	import flash.events.KeyboardEvent;
	import flash.events.MouseEvent;
	import flash.geom.Point;
	import flash.geom.Rectangle;
	import flash.net.URLRequest;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;
	import flash.ui.Keyboard;
	import flash.utils.getQualifiedClassName;

	public class MenuHelper extends MovieClip
	{
		private static const ITEM_PRESS:String = "BSScrollingList::itemPress";
		private static const LIST_PRESS:String = "BSScrollingList::listPress";
		private static const SELECTION_CHANGE:String = "BSScrollingList::selectionChange";
		private static const LIST_ITEMS_CREATED:String = "BSScrollingList::listItemsCreated";
		private static const OPTION_VALUE_CHANGE:String = "SettingsOptionItem::value_change";

		private static const LIST_PANEL_CLASS:String = "MainMenu_fla.SettingsListPanel_25";
		private static const OPTIONS_PANEL_CLASS:String = "MainMenu_fla.OptionsListPanel_40";
		private static const CONFIRM_PANEL_CLASS:String = "ConfirmPanel";
		private static const HELP_PANEL_CLASS:String = "MainMenu_fla.HelpPanel_mc_34";
		private static const HELP_ENTRY_CLASS:String = "MainMenuHelpListEntry";
		private static const BUTTON_HINT_CLASS:String = "Shared.AS3.BSButtonHintData";
		private static const GLOBAL_FUNC_CLASS:String = "Shared.GlobalFunc";
		private static const TEXT_FIELD_EX_CLASS:String = "scaleform.gfx.TextFieldEx";
		private static const CONTROL_CLASSES:Array = ["Option_Scrollbar", "Option_OptionStepper", "Option_Checkbox"];
		private static const STEPPER_ARROWS:Array = ["LeftArrow_mc", "RightArrow_mc", "LeftCatcher_mc", "RightCatcher_mc"];

		private static const TARGET_LIST:String = "settingsCategoryList";
		private static const TARGET_OPTIONS:String = "settingsList";
		private static const TARGET_GUIDE:String = "settingGuideText";
		private static const TARGET_CONFIRM:String = "confirmText";
		private static const TARGET_HELP_LIST:String = "helpListBackground";
		private static const TARGET_COLOR:String = "color";
		private static const TARGET_POPUP_BACKING:String = "popupBacking";

		private static const TYPE_BUTTON:uint = 0;
		private static const TYPE_CHECKBOX:uint = 1;
		private static const TYPE_STEPPER:uint = 2;
		private static const TYPE_SLIDER:uint = 3;
		private static const TYPE_DROPDOWN:uint = 4;
		private static const TYPE_HOTKEY:uint = 5;
		private static const TYPE_COLOR:uint = 6;
		private static const TYPE_HEADER:uint = 7;
		private static const TYPE_TEXT:uint = 8;
		private static const TYPE_TOPIC:uint = 9;

		private static const LAYOUT_LIST:uint = 0;
		private static const LAYOUT_OPTIONS:uint = 1;
		private static const LAYOUT_PAGES:uint = 2;

		// MainMenu.as background index for BGSCodeObj.SetBackgroundVisible.
		private static const HELP_TOPIC_BACKGROUND:uint = 2;

		private static const MOVIE_SLIDER:uint = 0;
		private static const MOVIE_STEPPER:uint = 1;
		private static const MOVIE_CHECKBOX:uint = 2;

		private static const HIDDEN_FRAME:uint = 1;
		private static const SHOWN_FRAME:uint = 3;

		private static const MIN_LIST_ROWS:uint = 4;
		private static const MAX_LIST_ROWS:uint = 9;
		private static const DISABLED_ALPHA:Number = 0.5;
		private static const STAGE_CENTER_X:Number = 640;
		private static const STAGE_CENTER_Y:Number = 360;
		private static const VALUE_FIELD_NAME:String = "MenuHelperValue_tf";
		private static const VALUE_FIELD_WIDTH:Number = 70;
		private static const VALUE_FIELD_GAP:Number = 6;
		private static const SWATCH_SIZE:Number = 96;
		private static const INPUT_RESUME_FRAMES:int = 2;
		private static const ROW_HEIGHT_SLACK:Number = 1;
		private static const POPUP_PADDING:Number = 8;
		private static const POPUP_OUTLINE:Number = 2;
		private static const IMAGE_URL_PREFIX:String = "img://";
		private static const IMAGE_GAP:Number = 8;
		private static const SELECTED_TEXT_COLOR:uint = 0x000000;
		private static const TEXT_COLOR:uint = 0xFFFFFF;

		private var menu:MovieClip;
		private var codeObj:Object;
		private var stack:Array = new Array();
		private var vanillaButtons:Object;
		private var hintButtons:Object;
		private var confirm:Object;
		private var capture:Object;
		// Frames to wait before the list takes input again, so keyboard events of a key that closed a prompt do not reach it.
		private var inputResumeFrames:int;
		private var adjustingSelection:Boolean;
		private var settingItems:Boolean;
		private var labelWidth:Number = NaN;
		// Pages reuse the vanilla HELP text panel, which has no class of its own.
		private var topicShown:Boolean;
		private var shownTopic:Object;
		private var topicTarget:uint;
		private var image:Object;
		private var helpTextY:Number = NaN;
		private var helpTextHeight:Number = NaN;

		public function MenuHelper()
		{
			super();
			addEventListener(Event.ADDED_TO_STAGE, this.onAddedToStage);
		}

		private function onAddedToStage(event:Event):void
		{
			removeEventListener(Event.ADDED_TO_STAGE, this.onAddedToStage);
			this.menu = parent.parent as MovieClip;
			this.codeObj = this.menu.parent["MenuHelperCodeObj"];

			addEventListener(ITEM_PRESS, this.onItemPress);
			addEventListener(LIST_PRESS, this.stopEvent);
			addEventListener(SELECTION_CHANGE, this.onSelectionChange);
			addEventListener(LIST_ITEMS_CREATED, this.stopEvent);
			addEventListener(OPTION_VALUE_CHANGE, this.onValueChange);
			addEventListener(KeyboardEvent.KEY_DOWN, this.onKeyDownCapture, true);
			addEventListener(KeyboardEvent.KEY_DOWN, this.stopEvent);
			addEventListener(KeyboardEvent.KEY_UP, this.stopEvent);
			this.menu.addEventListener(MouseEvent.ROLL_OVER, this.onTopicRoll, true);
			this.menu.addEventListener(MouseEvent.ROLL_OUT, this.onTopicRoll, true);

			var missing:String = this.FindMissingVanillaParts();
			if (missing != null)
			{
				this.codeObj.OnTemplateError(missing);
				return;
			}
			this.menu.parent["MenuHelperUI"] = this;
			addEventListener(Event.ENTER_FRAME, this.onEnterFrame);
			this.codeObj.InjectMainEntries();
			this.codeObj.OnReady();
		}

		private function FindMissingVanillaParts():String
		{
			var required:Array = [LIST_PANEL_CLASS, OPTIONS_PANEL_CLASS, CONFIRM_PANEL_CLASS, HELP_PANEL_CLASS, HELP_ENTRY_CLASS, BUTTON_HINT_CLASS, GLOBAL_FUNC_CLASS, TEXT_FIELD_EX_CLASS];
			for each (var name:String in required)
			{
				if (this.FindClass(name) == null)
				{
					return "class " + name;
				}
			}
			var members:Array = ["SettingsPanel_mc", "OptionsPanel_mc", "ConfirmPanel_mc", "HelpPanel_mc", "BackgroundAndBracketsHelpTopic_mc", "MainPanel_mc", "ButtonHintBar_mc", "BGSCodeObj", "ShowSecondPanelBackground"];
			for each (var member:String in members)
			{
				if (!(member in this.menu))
				{
					return "Menu_mc." + member;
				}
			}
			for each (var part:String in ["HelpText", "HelpTitleText", "ScrollUp", "ScrollDown"])
			{
				if (!(part in this.menu.BackgroundAndBracketsHelpTopic_mc))
				{
					return "Menu_mc.BackgroundAndBracketsHelpTopic_mc." + part;
				}
			}
			if (!("SetBackgroundVisible" in this.menu.BGSCodeObj))
			{
				return "Menu_mc.BGSCodeObj.SetBackgroundVisible";
			}
			return null;
		}

		public function OpenMenu(menuId:uint, items:Array, layout:uint, popup:Boolean, selectedIndex:int, hasDefaults:Boolean, swatchColor:int):Boolean
		{
			var opened:Object = this.CreatePanel(layout, items.length, popup);
			if (opened == null)
			{
				return false;
			}
			if (this.stack.length == 0)
			{
				this.BeginSession();
			}
			opened.id = menuId;
			opened.hasDefaults = hasDefaults;
			opened.swatchColor = swatchColor;
			this.AddPanel(opened, numChildren);
			this.stack.push(opened);
			this.SetItems(opened, items);
			this.UpdatePanels();
			this.SelectIndex(opened, selectedIndex >= 0 && selectedIndex < items.length ? selectedIndex : this.FirstEnabledIndex(opened));
			this.ShowTopic(opened);
			this.UpdateGuideText(opened);
			this.UpdateHints();
			this.FocusTop();
			this.codeObj.OnMenuOpened(menuId);
			return true;
		}

		public function UpdateMenu(menuId:uint, items:Array, layout:uint, hasDefaults:Boolean):Boolean
		{
			var index:int = this.IndexOfMenu(menuId);
			if (index < 0)
			{
				return false;
			}
			var current:Object = this.stack[index];
			current.hasDefaults = hasDefaults;
			if (current.layout != layout)
			{
				var replacement:Object = this.CreatePanel(layout, items.length, current.popup);
				if (replacement == null)
				{
					return false;
				}
				replacement.id = menuId;
				replacement.hasDefaults = hasDefaults;
				replacement.swatchColor = current.swatchColor;
				this.AddPanel(replacement, getChildIndex(current.panel));
				this.DestroyPanel(current);
				this.stack[index] = replacement;
				this.SetItems(replacement, items);
				this.UpdatePanels();
				this.SelectIndex(replacement, Math.min(current.list.selectedIndex, items.length - 1));
				current = replacement;
				if (index == this.stack.length - 1)
				{
					this.FocusTop();
				}
			}
			else
			{
				this.SetItems(current, items);
				this.SelectIndex(current, current.list.selectedIndex);
			}
			this.shownTopic = null;
			this.ShowTopic(current);
			this.UpdateGuideText(current);
			this.UpdateHints();
			return true;
		}

		public function CloseMenu(menuId:uint):Boolean
		{
			var index:int = this.IndexOfMenu(menuId);
			if (index < 0)
			{
				return false;
			}
			while (this.stack.length > index)
			{
				this.CloseTop();
			}
			return true;
		}

		public function CloseAll():void
		{
			while (this.stack.length > 0)
			{
				this.CloseTop();
			}
		}

		public function Back():Boolean
		{
			if (this.stack.length == 0 || this.capture != null)
			{
				return false;
			}
			if (this.confirm != null)
			{
				this.EndConfirm(false);
				return true;
			}
			this.PlaySound("PlayCancelSound");
			this.CloseTop();
			return true;
		}

		public function OnMainListInjected():void
		{
			this.FocusTop();
		}

		// Native button events. Actions run on release, and confirm answers only for keys pressed while it shows.
		public function ProcessUserEvent(name:String, released:Boolean, pressed:Boolean):Boolean
		{
			if (this.stack.length == 0)
			{
				return false;
			}
			if (this.capture != null)
			{
				return true;
			}
			if (this.confirm != null)
			{
				if (pressed)
				{
					this.confirm.pressed = name;
				}
				else if (released && name == this.confirm.pressed)
				{
					if (name == "Accept")
					{
						this.EndConfirm(true);
					}
					else if (name == "Cancel" || name == "Start")
					{
						this.EndConfirm(false);
					}
				}
				return true;
			}
			var top:Object = this.TopMenu();
			if (pressed && top.layout == LAYOUT_PAGES)
			{
				this.ScrollTopic(name);
			}
			if (!released)
			{
				return true;
			}
			if (name == "Cancel" || name == "Start")
			{
				this.Back();
			}
			else if ((name == "ResetToDefault" || name == "YButton") && top.hasDefaults && !top.popup)
			{
				this.PlaySound("PlayOKSound");
				this.codeObj.OnResetRequest(top.id);
			}
			return true;
		}

		public function ShowConfirm(confirmId:uint, text:String):Boolean
		{
			var PanelClass:Class = this.FindClass(CONFIRM_PANEL_CLASS);
			if (this.stack.length == 0 || this.confirm != null || this.capture != null || PanelClass == null)
			{
				return false;
			}
			var vanilla:MovieClip = this.menu.ConfirmPanel_mc;
			var panel:MovieClip = new PanelClass() as MovieClip;
			panel.x = vanilla.x;
			panel.y = vanilla.y;
			panel.scaleX = vanilla.scaleX;
			panel.scaleY = vanilla.scaleY;
			var field:TextField = panel.Text_mc.TextPulse_mc.textField;
			this.FindClass(TEXT_FIELD_EX_CLASS).setTextAutoSize(field, "shrink");
			addChild(panel);
			panel.gotoAndStop(SHOWN_FRAME);
			this.FindClass(GLOBAL_FUNC_CLASS).SetText(field, text, false);
			this.confirm = {"id": confirmId, "panel": panel, "target": this.codeObj.AddShaderTarget(panel, TARGET_CONFIRM, 0), "pressed": null};
			this.UpdateInput();
			this.UpdateHints();
			return true;
		}

		public function BeginCapture(menuId:uint, prompt:String):Boolean
		{
			var top:Object = this.TopMenu();
			if (top == null || top.id != menuId || top.layout != LAYOUT_OPTIONS || this.confirm != null || this.capture != null)
			{
				return false;
			}
			this.capture = {"menuId": menuId};
			this.FindClass(GLOBAL_FUNC_CLASS).SetText(top.panel.GuideText_tf, prompt, false);
			this.UpdateInput();
			this.UpdateHints();
			return true;
		}

		public function EndCapture():void
		{
			if (this.capture == null)
			{
				return;
			}
			this.capture = null;
			this.inputResumeFrames = INPUT_RESUME_FRAMES;
			var top:Object = this.TopMenu();
			if (top != null)
			{
				this.UpdateGuideText(top);
			}
			this.UpdateHints();
		}

		public function SetSwatchColor(menuId:uint, color:uint):void
		{
			var index:int = this.IndexOfMenu(menuId);
			if (index >= 0 && this.stack[index].swatchTarget)
			{
				this.codeObj.SetShaderTargetColor(this.stack[index].swatchTarget, color);
			}
		}

		private function BeginSession():void
		{
			this.vanillaButtons = this.codeObj.GetButtonHintData();
			if (this.vanillaButtons != null)
			{
				var ButtonHintData:Class = this.FindClass(BUTTON_HINT_CLASS);
				this.hintButtons = {
					"confirm": new ButtonHintData("$CONFIRM", "Enter", "PSN_A", "Xenon_A", 1, null),
					"defaults": new ButtonHintData("$DEFAULTS", "T", "PSN_Y", "Xenon_Y", 1, null),
					"cancel": new ButtonHintData("$CANCEL", "Esc", "PSN_B", "Xenon_B", 1, null),
					"back": new ButtonHintData("$BACK", "Esc", "PSN_B", "Xenon_B", 1, this.Back)
				};
			}
		}

		private function EndSession():void
		{
			if (this.vanillaButtons != null)
			{
				this.menu.ButtonHintBar_mc.SetButtonHintData(this.vanillaButtons);
				this.vanillaButtons = null;
				this.hintButtons = null;
			}
			this.ApplyBackgrounds();
			stage.focus = this.menu.MainPanel_mc.List_mc;
		}

		// Item menus use the vanilla second panel. Pages use the vanilla HELP text panel and hide the main list while they show a topic list.
		private function ApplyBackgrounds():void
		{
			var top:Object = this.TopMenu();
			var pages:Boolean = top != null && top.layout == LAYOUT_PAGES;
			this.menu.ShowSecondPanelBackground(top != null && !pages);
			this.ShowTopicPanel(pages);
			this.menu.MainPanel_mc.visible = !pages || top.single;
		}

		private function FocusTop():void
		{
			var top:Object = this.TopMenu();
			if (top != null)
			{
				stage.focus = top.layout == LAYOUT_PAGES && top.single ? this.menu.BackgroundAndBracketsHelpTopic_mc.HelpText : top.list;
			}
		}

		private function UpdateHints():void
		{
			var top:Object = this.TopMenu();
			if (this.vanillaButtons == null || top == null)
			{
				return;
			}
			var shown:Array;
			if (this.confirm != null)
			{
				shown = [this.hintButtons.confirm, this.hintButtons.cancel];
			}
			else if (this.capture != null)
			{
				shown = [this.hintButtons.cancel];
			}
			else if (top.hasDefaults && !top.popup)
			{
				shown = [this.hintButtons.defaults, this.hintButtons.back];
			}
			else
			{
				shown = [this.hintButtons.back];
			}
			var buttons:Object = this.vanillaButtons.slice(0, 0);
			for each (var button:Object in shown)
			{
				buttons.push(button);
			}
			this.menu.ButtonHintBar_mc.SetButtonHintData(buttons);
		}

		private function CloseTop():void
		{
			if (this.confirm != null)
			{
				this.EndConfirm(false);
			}
			this.capture = null;
			var closed:Object = this.stack.pop();
			this.DestroyPanel(closed);
			this.codeObj.OnMenuClosed(closed.id);
			var top:Object = this.TopMenu();
			if (top == null)
			{
				this.EndSession();
				return;
			}
			this.UpdatePanels();
			this.shownTopic = null;
			this.ShowTopic(top);
			this.UpdateHints();
			this.FocusTop();
		}

		private function EndConfirm(result:Boolean):void
		{
			var ended:Object = this.confirm;
			if (ended == null)
			{
				return;
			}
			this.confirm = null;
			if (ended.target)
			{
				this.codeObj.RemoveShaderTarget(ended.target);
			}
			removeChild(ended.panel);
			this.inputResumeFrames = INPUT_RESUME_FRAMES;
			this.UpdateInput();
			this.UpdateHints();
			this.PlaySound(result ? "PlayOKSound" : "PlayCancelSound");
			this.codeObj.OnConfirmResult(ended.id, result);
		}

		private function CreatePanel(layout:uint, itemCount:uint, popup:Boolean):Object
		{
			if (layout == LAYOUT_PAGES)
			{
				return this.CreatePagesPanel(itemCount);
			}
			var options:Boolean = layout == LAYOUT_OPTIONS;
			var PanelClass:Class = this.FindClass(options ? OPTIONS_PANEL_CLASS : LIST_PANEL_CLASS);
			if (PanelClass == null)
			{
				return null;
			}
			var vanillaPanel:MovieClip = options ? this.menu.OptionsPanel_mc : this.menu.SettingsPanel_mc;
			var panel:MovieClip = new PanelClass() as MovieClip;
			panel.x = vanillaPanel.x;
			panel.y = vanillaPanel.y;
			panel.scaleX = vanillaPanel.scaleX;
			panel.scaleY = vanillaPanel.scaleY;
			var list:Object = options ? panel.Fader_mc.List_mc : panel.SettingsList_mc;
			if (options)
			{
				panel.GuideText_tf.text = " ";
			}
			else
			{
				list.numListItems = Math.max(MIN_LIST_ROWS, Math.min(itemCount, MAX_LIST_ROWS));
			}
			return {"panel": panel, "list": list, "layout": layout, "popup": popup, "lastIndex": -1, "reportedId": 0};
		}

		// One topic is shown alone; the topic list stays hidden.
		private function CreatePagesPanel(itemCount:uint):Object
		{
			var PanelClass:Class = this.FindClass(HELP_PANEL_CLASS);
			var vanillaPanel:MovieClip = this.menu.HelpPanel_mc;
			var panel:MovieClip = new PanelClass() as MovieClip;
			panel.x = vanillaPanel.x;
			panel.y = vanillaPanel.y;
			panel.scaleX = vanillaPanel.scaleX;
			panel.scaleY = vanillaPanel.scaleY;
			return {"panel": panel, "list": panel.HelpList_mc, "layout": LAYOUT_PAGES, "popup": false, "single": itemCount == 1, "lastIndex": -1, "reportedId": 0};
		}

		// List rows exist only once the panel is on the stage. The symbol's border is taller than its default row count.
		private function AddPanel(target:Object, index:int):void
		{
			addChildAt(target.panel, index);
			if (target.layout != LAYOUT_LIST)
			{
				return;
			}
			var list:Object = target.list;
			var row:DisplayObject = list.GetClipByIndex(0);
			if (row == null)
			{
				return;
			}
			list.border.height = row.height * list.numListItems + ROW_HEIGHT_SLACK;
			if (target.popup)
			{
				var rows:Rectangle = row.getBounds(target.panel);
				rows.height = list.border.height;
				rows.inflate(POPUP_PADDING, POPUP_PADDING);
				target.panel.x = STAGE_CENTER_X - (rows.x + rows.width / 2) * target.panel.scaleX;
				target.panel.y = STAGE_CENTER_Y - (rows.y + rows.height / 2) * target.panel.scaleY;
				target.popupBounds = rows;
			}
		}

		// Popups are drawn over the page below, so they get an opaque backing and an outline like the vanilla message box.
		private function AddPopupFrame(target:Object):void
		{
			var bounds:Rectangle = target.popupBounds;
			var backing:Shape = new Shape();
			backing.graphics.beginFill(0x000000);
			backing.graphics.drawRect(bounds.x, bounds.y, bounds.width, bounds.height);
			backing.graphics.endFill();
			target.panel.addChildAt(backing, 0);
			this.AddTarget(target, backing, TARGET_POPUP_BACKING, 0);
			var outline:Shape = new Shape();
			outline.graphics.lineStyle(POPUP_OUTLINE, 0xFFFFFF);
			outline.graphics.drawRect(bounds.x, bounds.y, bounds.width, bounds.height);
			target.panel.addChild(outline);
			this.AddTarget(target, outline, TARGET_LIST, 0);
		}

		// Only the top menu and the menus under popups are shown, and only the top one takes input.
		private function UpdatePanels():void
		{
			var shown:Boolean = true;
			for (var i:int = this.stack.length - 1; i >= 0; --i)
			{
				var record:Object = this.stack[i];
				if (shown && !record.single)
				{
					this.ShowPanel(record);
				}
				else
				{
					record.panel.gotoAndStop(HIDDEN_FRAME);
				}
				shown = shown && record.popup;
			}
			this.ApplyBackgrounds();
			this.UpdateInput();
		}

		private function UpdateInput():void
		{
			var top:Object = this.TopMenu();
			for each (var record:Object in this.stack)
			{
				var enabled:Boolean = record == top && this.confirm == null && this.capture == null && this.inputResumeFrames == 0;
				record.list.disableInput = !enabled;
				record.panel.mouseChildren = enabled;
			}
		}

		// The menu only composites areas covered by native shader targets, which also apply the HUD color.
		private function ShowPanel(target:Object):void
		{
			target.panel.gotoAndStop(SHOWN_FRAME);
			if (target.shaderTargets != null)
			{
				return;
			}
			target.shaderTargets = new Array();
			var objects:Array;
			switch (target.layout)
			{
				case LAYOUT_OPTIONS:
					objects = [[target.list, TARGET_OPTIONS], [target.panel.GuideText_tf, TARGET_GUIDE]];
					break;
				case LAYOUT_PAGES:
					objects = [[target.panel, TARGET_LIST], [target.panel.background_mc, TARGET_HELP_LIST]];
					break;
				default:
					objects = [[target.panel, TARGET_LIST]];
			}
			for each (var pair:Array in objects)
			{
				this.AddTarget(target, pair[0], pair[1], 0);
			}
			if (target.popupBounds != null)
			{
				this.AddPopupFrame(target);
			}
			if (target.layout == LAYOUT_OPTIONS && target.swatchColor >= 0)
			{
				var swatch:Shape = new Shape();
				swatch.graphics.beginFill(0xFFFFFF);
				swatch.graphics.drawRect(0, 0, SWATCH_SIZE, SWATCH_SIZE);
				swatch.graphics.endFill();
				swatch.x = target.panel.GuideText_tf.x;
				swatch.y = target.panel.GuideText_tf.y;
				target.panel.addChild(swatch);
				target.swatchTarget = this.AddTarget(target, swatch, TARGET_COLOR, target.swatchColor);
			}
		}

		private function AddTarget(target:Object, object:Object, kind:String, color:uint):uint
		{
			var handle:uint = this.codeObj.AddShaderTarget(object, kind, color);
			if (handle != 0)
			{
				target.shaderTargets.push(handle);
			}
			return handle;
		}

		private function DestroyPanel(target:Object):void
		{
			for each (var handle:uint in target.shaderTargets)
			{
				this.codeObj.RemoveShaderTarget(handle);
			}
			target.shaderTargets = null;
			removeChild(target.panel);
		}

		private function SetItems(target:Object, items:Array):void
		{
			var options:Boolean = target.layout == LAYOUT_OPTIONS;
			var entries:Array = new Array();
			for each (var item:Object in items)
			{
				var entry:Object = {
					"text": item.text,
					"disabled": item.disabled,
					"itemId": item.itemId,
					"itemType": item.itemType,
					"guideText": item.description
				};
				if (target.layout == LAYOUT_PAGES)
				{
					entry.title = item.title;
					entry.html = item.description;
					entry.image = item.image;
					entry.imageWidth = item.imageWidth;
					entry.imageHeight = item.imageHeight;
				}
				if (options)
				{
					entry.ID = item.itemId;
					entry.movieType = this.MovieTypeFor(item.itemType);
					entry.value = item.value;
					entry.step = item.step;
					if (item.options != undefined)
					{
						entry.options = item.options;
					}
					if (item.itemType == TYPE_SLIDER)
					{
						entry.minValue = item.minValue;
						entry.maxValue = item.maxValue;
						entry.stepValue = item.stepValue;
						entry.decimals = item.decimals;
						entry.format = item.format;
						entry.decimalSeparator = item.decimalSeparator;
					}
				}
				entries.push(entry);
			}

			this.settingItems = true;
			var list:Object = target.list;
			list.entryList = entries;
			if (options)
			{
				list.allowValueOverwrite = true;
				list.InvalidateData();
				list.allowValueOverwrite = false;
				this.ApplyRowStates(target);
			}
			else
			{
				list.InvalidateData();
			}
			this.settingItems = false;
		}

		private function MovieTypeFor(itemType:uint):uint
		{
			switch (itemType)
			{
				case TYPE_SLIDER:
					return MOVIE_SLIDER;
				case TYPE_STEPPER:
				case TYPE_DROPDOWN:
				case TYPE_HOTKEY:
				case TYPE_COLOR:
					return MOVIE_STEPPER;
				default:
					return MOVIE_CHECKBOX;
			}
		}

		// Rows whose vanilla control changes the value itself. Other value rows show their value as stepper text.
		private function HasLiveControl(itemType:uint):Boolean
		{
			return itemType == TYPE_CHECKBOX || itemType == TYPE_STEPPER || itemType == TYPE_SLIDER;
		}

		private function ShowsControl(itemType:uint):Boolean
		{
			return this.HasLiveControl(itemType) || itemType == TYPE_DROPDOWN || itemType == TYPE_HOTKEY || itemType == TYPE_COLOR;
		}

		private function IsSelectable(target:Object, entry:Object):Boolean
		{
			return entry != null && entry.itemType != TYPE_HEADER && (entry.itemType != TYPE_TEXT || target.layout == LAYOUT_OPTIONS);
		}

		private function FirstEnabledIndex(target:Object):int
		{
			var entries:Array = target.list.entryList;
			for (var i:int = 0; i < entries.length; ++i)
			{
				if (this.IsSelectable(target, entries[i]) && !entries[i].disabled)
				{
					return i;
				}
			}
			return this.NearestSelectable(target, 0, 1);
		}

		// The nearest selectable row from index in direction, else in the other direction, else -1.
		private function NearestSelectable(target:Object, index:int, direction:int):int
		{
			var entries:Array = target.list.entryList;
			for each (var step:int in [direction, -direction])
			{
				for (var i:int = index; i >= 0 && i < entries.length; i += step)
				{
					if (this.IsSelectable(target, entries[i]))
					{
						return i;
					}
				}
			}
			return -1;
		}

		private function SelectIndex(target:Object, index:int):void
		{
			if (index >= 0 && !this.IsSelectable(target, target.list.entryList[index]))
			{
				index = this.NearestSelectable(target, index, 1);
			}
			if (target.list.selectedIndex != index)
			{
				target.list.selectedIndex = index;
			}
			target.lastIndex = target.list.selectedIndex;
			this.ReportSelection(target);
		}

		private function ReportSelection(target:Object):void
		{
			var entry:Object = target.list.selectedEntry;
			var itemId:uint = entry != null ? entry.itemId : 0;
			if (this.settingItems || itemId == target.reportedId)
			{
				return;
			}
			target.reportedId = itemId;
			if (itemId != 0)
			{
				this.codeObj.OnItemSelect(target.id, itemId);
			}
		}

		private function ApplyRowStates(target:Object):void
		{
			var list:Object = target.list;
			var entries:Array = list.entryList;
			for (var i:uint = 0; i < list.numListItems; ++i)
			{
				var clip:MovieClip = list.GetClipByIndex(i) as MovieClip;
				if (clip == null)
				{
					break;
				}
				var entry:Object = clip.itemIndex < entries.length ? entries[clip.itemIndex] : null;
				if (entry == null)
				{
					continue;
				}
				clip.alpha = entry.disabled ? DISABLED_ALPHA : 1;
				var control:MovieClip = this.FindControl(clip);
				if (control == null)
				{
					continue;
				}
				control.visible = this.ShowsControl(entry.itemType);
				control.mouseEnabled = !entry.disabled;
				control.mouseChildren = !entry.disabled;
				if (!this.HasLiveControl(entry.itemType))
				{
					for each (var arrow:String in STEPPER_ARROWS)
					{
						if (arrow in control)
						{
							control[arrow].visible = false;
						}
					}
				}
				if (entry.itemType == TYPE_SLIDER && entry.step > 0)
				{
					control.StepSize = entry.step;
				}
				this.UpdateValueField(clip, control, entry);
			}
		}

		// Sliders have no text of their own, so the value is drawn left of the slider and the label is narrowed to fit.
		private function UpdateValueField(clip:MovieClip, control:MovieClip, entry:Object):void
		{
			var field:TextField = clip.getChildByName(VALUE_FIELD_NAME) as TextField;
			var label:TextField = clip.textField;
			if (isNaN(this.labelWidth))
			{
				this.labelWidth = label.width;
			}
			if (entry.itemType != TYPE_SLIDER)
			{
				if (field != null)
				{
					field.visible = false;
				}
				if (label.width != this.labelWidth)
				{
					label.width = this.labelWidth;
				}
				return;
			}
			if (field == null)
			{
				var format:TextFormat = label.defaultTextFormat;
				format.align = TextFormatAlign.RIGHT;
				field = new TextField();
				field.name = VALUE_FIELD_NAME;
				field.embedFonts = label.embedFonts;
				field.defaultTextFormat = format;
				field.selectable = false;
				field.mouseEnabled = false;
				field.width = VALUE_FIELD_WIDTH;
				field.height = label.height;
				field.x = control.x + Math.min(control.Track_mc.x, control.LeftArrow_mc.x) - VALUE_FIELD_WIDTH - VALUE_FIELD_GAP;
				field.y = label.y;
				clip.addChild(field);
			}
			var narrowed:Number = Math.min(this.labelWidth, field.x - label.x - VALUE_FIELD_GAP);
			if (label.width != narrowed)
			{
				label.width = narrowed;
			}
			field.visible = true;
			field.text = this.FormatValue(entry, control.value);
			field.textColor = clip.selected ? SELECTED_TEXT_COLOR : TEXT_COLOR;
		}

		private function FormatValue(entry:Object, normalized:Number):String
		{
			var range:Number = entry.maxValue - entry.minValue;
			var steps:Number = Math.round(normalized * range / entry.stepValue);
			var value:Number = Math.min(entry.minValue + steps * entry.stepValue, entry.maxValue);
			if (Math.abs(value) < Math.pow(10, -entry.decimals) / 2)
			{
				value = 0;
			}
			var text:String = value.toFixed(entry.decimals);
			if (entry.decimalSeparator != ".")
			{
				text = text.split(".").join(entry.decimalSeparator);
			}
			return entry.format.split("{value}").join(text);
		}

		private function FindControl(clip:MovieClip):MovieClip
		{
			for (var i:int = clip.numChildren - 1; i >= 0; --i)
			{
				var child:DisplayObject = clip.getChildAt(i);
				if (CONTROL_CLASSES.indexOf(getQualifiedClassName(child)) >= 0)
				{
					return child as MovieClip;
				}
			}
			return null;
		}

		private function UpdateGuideText(target:Object):void
		{
			if (target.layout != LAYOUT_OPTIONS || (this.capture != null && target == this.TopMenu()))
			{
				return;
			}
			var entry:Object = target.list.selectedEntry;
			var text:String = entry != null && entry.guideText ? entry.guideText : "";
			this.FindClass(GLOBAL_FUNC_CLASS).SetText(target.panel.GuideText_tf, text, false);
		}

		// Vanilla and other mods change the main list from paths MenuHelper cannot hook. Entries they add lack both markers.
		private function onEnterFrame(event:Event):void
		{
			for each (var entry:Object in this.menu.MainPanel_mc.List_mc.entryList)
			{
				if (entry != null && entry.menuHelperId == undefined && entry.menuHelperVanilla == undefined)
				{
					this.codeObj.InjectMainEntries();
					break;
				}
			}
			if (this.inputResumeFrames > 0 && --this.inputResumeFrames == 0)
			{
				this.UpdateInput();
			}
			var top:Object = this.TopMenu();
			if (top != null && top.layout == LAYOUT_OPTIONS)
			{
				this.ApplyRowStates(top);
			}
		}

		// Rows without a live control are routed like buttons, so a hidden checkbox never toggles.
		private function onItemPress(event:Event):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (top == null || event.target != top.list)
			{
				return;
			}
			var entry:Object = top.list.selectedEntry;
			if (!this.IsSelectable(top, entry) || entry.itemType == TYPE_TEXT || entry.itemType == TYPE_TOPIC)
			{
				return;
			}
			if (entry.disabled)
			{
				this.PlaySound("PlayCancelSound");
				return;
			}
			if (top.layout == LAYOUT_OPTIONS && this.HasLiveControl(entry.itemType))
			{
				top.list.onListItemPressed();
				return;
			}
			this.PlaySound("PlayOKSound");
			this.codeObj.OnItemPress(top.id, entry.itemId);
		}

		private function onValueChange(event:Event):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (top == null)
			{
				return;
			}
			var clip:Object = event.target;
			var entry:Object = top.list.entryList[clip.itemIndex];
			if (entry == null || !this.HasLiveControl(entry.itemType))
			{
				return;
			}
			this.codeObj.OnValueChange(top.id, entry.itemId, clip.value);
		}

		// Moving onto a header continues in the same direction. Setting selectedIndex here dispatches this event again.
		private function onSelectionChange(event:Event):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (top == null || event.target != top.list || this.adjustingSelection)
			{
				return;
			}
			var list:Object = top.list;
			var index:int = list.selectedIndex;
			if (index >= 0 && !this.IsSelectable(top, list.entryList[index]))
			{
				this.adjustingSelection = true;
				list.selectedIndex = this.NearestSelectable(top, index, index < top.lastIndex ? -1 : 1);
				this.adjustingSelection = false;
			}
			top.lastIndex = list.selectedIndex;
			this.ShowTopic(top);
			this.UpdateGuideText(top);
			this.ReportSelection(top);
		}

		private function onKeyDownCapture(event:KeyboardEvent):void
		{
			var top:Object = this.TopMenu();
			if (top != null && top.layout == LAYOUT_PAGES && (event.keyCode == Keyboard.LEFT || event.keyCode == Keyboard.RIGHT))
			{
				event.stopPropagation();
				if (!top.single && this.confirm == null)
				{
					stage.focus = event.keyCode == Keyboard.RIGHT ? this.menu.BackgroundAndBracketsHelpTopic_mc.HelpText : top.list;
				}
				return;
			}
			if (top == null || top.layout != LAYOUT_OPTIONS || (event.keyCode != Keyboard.LEFT && event.keyCode != Keyboard.RIGHT))
			{
				return;
			}
			var entry:Object = top.list.selectedEntry;
			if (entry != null && (entry.disabled || !this.HasLiveControl(entry.itemType)))
			{
				event.stopPropagation();
			}
		}

		private function ShowTopicPanel(show:Boolean):void
		{
			if (this.topicShown == show)
			{
				return;
			}
			this.topicShown = show;
			this.shownTopic = null;
			var topic:MovieClip = this.menu.BackgroundAndBracketsHelpTopic_mc;
			this.menu.BGSCodeObj.SetBackgroundVisible(HELP_TOPIC_BACKGROUND, show);
			topic.visible = show;
			topic.HelpText.visible = show;
			topic.HelpTitleText.visible = show;
			// Vanilla registers the HELP panel's shader targets only when HELP opens, so the panel gets its own while pages show.
			// Keys typed while the text has focus would reach MainMenu's own handlers.
			if (show)
			{
				topic.HelpText.addEventListener(Event.SCROLL, this.onTopicScroll);
				topic.addEventListener(KeyboardEvent.KEY_DOWN, this.onTopicKeyDown);
				topic.addEventListener(KeyboardEvent.KEY_UP, this.stopEvent);
				this.topicTarget = this.codeObj.AddShaderTarget(topic, TARGET_LIST, 0);
			}
			else
			{
				topic.HelpText.removeEventListener(Event.SCROLL, this.onTopicScroll);
				topic.removeEventListener(KeyboardEvent.KEY_DOWN, this.onTopicKeyDown);
				topic.removeEventListener(KeyboardEvent.KEY_UP, this.stopEvent);
				this.ClearImage();
				if (this.topicTarget)
				{
					this.codeObj.RemoveShaderTarget(this.topicTarget);
					this.topicTarget = 0;
				}
			}
			this.UpdateScrollIndicators();
		}

		private function ShowTopic(target:Object):void
		{
			if (target == null || target.layout != LAYOUT_PAGES || target != this.TopMenu())
			{
				return;
			}
			var entry:Object = target.list.selectedEntry;
			if (entry == null || entry == this.shownTopic)
			{
				return;
			}
			this.shownTopic = entry;
			var topic:MovieClip = this.menu.BackgroundAndBracketsHelpTopic_mc;
			topic.HelpTitleText.text = entry.title;
			this.FindClass(GLOBAL_FUNC_CLASS).SetText(topic.HelpText, entry.html, true);
			topic.HelpText.scrollV = 1;
			this.LoadImage(entry);
			this.UpdateScrollIndicators();
		}

		private function ScrollTopic(name:String):void
		{
			var text:TextField = this.menu.BackgroundAndBracketsHelpTopic_mc.HelpText;
			if (stage.focus != text)
			{
				return;
			}
			if (name == "Up" || name == "Forward")
			{
				--text.scrollV;
			}
			else if (name == "Down" || name == "Back")
			{
				++text.scrollV;
			}
		}

		private function UpdateScrollIndicators():void
		{
			var topic:MovieClip = this.menu.BackgroundAndBracketsHelpTopic_mc;
			var text:TextField = topic.HelpText;
			topic.ScrollUp.visible = this.topicShown && text.scrollV > 1;
			topic.ScrollDown.visible = this.topicShown && text.bottomScrollV < text.numLines;
		}

		private function onTopicKeyDown(event:KeyboardEvent):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (event.keyCode == Keyboard.LEFT && top != null && top.layout == LAYOUT_PAGES && !top.single && this.confirm == null)
			{
				stage.focus = top.list;
			}
		}

		private function onTopicScroll(event:Event):void
		{
			this.UpdateScrollIndicators();
		}

		// Vanilla's handlers move focus to its HELP list, also when the panel hides after our page closes. Only vanilla HELP shows that list.
		private function onTopicRoll(event:MouseEvent):void
		{
			var topic:MovieClip = this.menu.BackgroundAndBracketsHelpTopic_mc;
			if (event.target != topic || this.menu.HelpPanel_mc.HelpList_mc.visible)
			{
				return;
			}
			event.stopImmediatePropagation();
			var top:Object = this.TopMenu();
			if (this.topicShown && top != null && this.confirm == null)
			{
				stage.focus = event.type == MouseEvent.ROLL_OVER || top.single ? topic.HelpText : top.list;
			}
		}

		// Images are DDS files under Data, shown through the game's image loader like vanilla DLC banners.
		private function LoadImage(entry:Object):void
		{
			this.ClearImage();
			if (!entry.image)
			{
				return;
			}
			// The game's image loader only serves textures registered natively, so the texture is loaded first.
			var name:String = this.codeObj.PrepareImage(entry.image);
			if (!name)
			{
				this.codeObj.Log("could not load page image " + entry.image);
				return;
			}
			var loader:Loader = new Loader();
			this.image = {"loader": loader, "entry": entry, "target": 0};
			loader.contentLoaderInfo.addEventListener(Event.COMPLETE, this.onImageLoaded);
			loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, this.onImageError);
			loader.load(new URLRequest(IMAGE_URL_PREFIX + name));
		}

		private function onImageLoaded(event:Event):void
		{
			if (this.image == null || event.target != this.image.loader.contentLoaderInfo)
			{
				return;
			}
			var loader:Loader = this.image.loader;
			var entry:Object = this.image.entry;
			var topic:MovieClip = this.menu.BackgroundAndBracketsHelpTopic_mc;
			var text:TextField = topic.HelpText;
			if (isNaN(this.helpTextY))
			{
				this.helpTextY = text.y;
				this.helpTextHeight = text.height;
			}
			if (entry.imageWidth > 0)
			{
				loader.width = entry.imageWidth;
			}
			if (entry.imageHeight > 0)
			{
				loader.height = entry.imageHeight;
			}
			// Drawn in this movie, outside the HUD-colored text panel.
			var position:Point = this.globalToLocal(topic.localToGlobal(new Point(text.x, this.helpTextY)));
			loader.x = position.x;
			loader.y = position.y;
			loader.mouseEnabled = false;
			loader.mouseChildren = false;
			addChild(loader);
			var shift:Number = loader.height / topic.scaleY + IMAGE_GAP;
			text.y = this.helpTextY + shift;
			text.height = Math.max(0, this.helpTextHeight - shift);
			// A fixed white color shows the image in its own colors instead of the HUD color.
			this.image.target = this.codeObj.AddShaderTarget(loader, TARGET_COLOR, 0xFFFFFF);
			this.UpdateScrollIndicators();
		}

		private function onImageError(event:IOErrorEvent):void
		{
			if (this.image != null && event.target == this.image.loader.contentLoaderInfo)
			{
				this.codeObj.Log("could not load page image " + this.image.entry.image);
				this.image = null;
			}
		}

		private function ClearImage():void
		{
			if (this.image != null)
			{
				if (this.image.target)
				{
					this.codeObj.RemoveShaderTarget(this.image.target);
				}
				var loader:Loader = this.image.loader;
				if (loader.parent != null)
				{
					loader.parent.removeChild(loader);
				}
				this.image = null;
			}
			if (!isNaN(this.helpTextY))
			{
				var text:TextField = this.menu.BackgroundAndBracketsHelpTopic_mc.HelpText;
				text.y = this.helpTextY;
				text.height = this.helpTextHeight;
			}
		}

		private function stopEvent(event:Event):void
		{
			event.stopPropagation();
		}

		private function PlaySound(name:String):void
		{
			this.menu.BGSCodeObj[name]();
		}

		private function TopMenu():Object
		{
			return this.stack.length > 0 ? this.stack[this.stack.length - 1] : null;
		}

		private function IndexOfMenu(menuId:uint):int
		{
			for (var i:int = 0; i < this.stack.length; ++i)
			{
				if (this.stack[i].id == menuId)
				{
					return i;
				}
			}
			return -1;
		}

		private function FindClass(name:String):Class
		{
			var domain:Object = this.menu.loaderInfo.applicationDomain;
			return domain.hasDefinition(name) ? domain.getDefinition(name) as Class : null;
		}
	}
}
