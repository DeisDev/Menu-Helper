package
{
	import flash.display.DisplayObject;
	import flash.display.MovieClip;
	import flash.events.Event;
	import flash.events.KeyboardEvent;
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
		private static const BUTTON_HINT_CLASS:String = "Shared.AS3.BSButtonHintData";
		private static const GLOBAL_FUNC_CLASS:String = "Shared.GlobalFunc";
		private static const CONTROL_CLASSES:Array = ["Option_Scrollbar", "Option_OptionStepper", "Option_Checkbox"];

		private static const TYPE_BUTTON:uint = 0;
		private static const TYPE_CHECKBOX:uint = 1;
		private static const TYPE_STEPPER:uint = 2;
		private static const TYPE_SLIDER:uint = 3;

		private static const MOVIE_SLIDER:uint = 0;
		private static const MOVIE_STEPPER:uint = 1;
		private static const MOVIE_CHECKBOX:uint = 2;

		private static const HIDDEN_FRAME:uint = 1;
		private static const SHOWN_FRAME:uint = 3;

		private static const MIN_LIST_ROWS:uint = 4;
		private static const MAX_LIST_ROWS:uint = 9;
		private static const DISABLED_ALPHA:Number = 0.5;

		private var menu:MovieClip;
		private var codeObj:Object;
		private var stack:Array = new Array();
		private var vanillaButtons:Object;
		private var backButton:Object;

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
			var required:Array = [LIST_PANEL_CLASS, OPTIONS_PANEL_CLASS, BUTTON_HINT_CLASS, GLOBAL_FUNC_CLASS];
			for each (var name:String in required)
			{
				if (this.FindClass(name) == null)
				{
					return "class " + name;
				}
			}
			var members:Array = ["SettingsPanel_mc", "OptionsPanel_mc", "MainPanel_mc", "ButtonHintBar_mc", "BGSCodeObj", "ShowSecondPanelBackground"];
			for each (var member:String in members)
			{
				if (!(member in this.menu))
				{
					return "Menu_mc." + member;
				}
			}
			return null;
		}

		public function OpenMenu(menuId:uint, items:Array, options:Boolean):Boolean
		{
			var opened:Object = this.CreatePanel(options, items.length);
			if (opened == null)
			{
				return false;
			}
			if (this.stack.length == 0)
			{
				this.BeginSession();
			}
			else
			{
				this.TopMenu().panel.gotoAndStop(HIDDEN_FRAME);
			}
			opened.id = menuId;
			addChild(opened.panel);
			this.stack.push(opened);
			this.ShowPanel(opened);
			this.SetItems(opened, items);
			opened.list.selectedIndex = this.FirstEnabledIndex(opened.list.entryList);
			this.UpdateGuideText(opened);
			stage.focus = opened.list;
			this.codeObj.OnMenuOpened(menuId);
			return true;
		}

		public function UpdateMenu(menuId:uint, items:Array, options:Boolean):Boolean
		{
			var index:int = this.IndexOfMenu(menuId);
			if (index < 0)
			{
				return false;
			}
			var current:Object = this.stack[index];
			if (current.options != options)
			{
				var replacement:Object = this.CreatePanel(options, items.length);
				if (replacement == null)
				{
					return false;
				}
				replacement.id = menuId;
				addChildAt(replacement.panel, getChildIndex(current.panel));
				this.DestroyPanel(current);
				this.stack[index] = replacement;
				this.ShowPanel(replacement);
				this.SetItems(replacement, items);
				replacement.list.selectedIndex = Math.min(current.list.selectedIndex, items.length - 1);
				if (index == this.stack.length - 1)
				{
					stage.focus = replacement.list;
				}
				else
				{
					replacement.panel.gotoAndStop(HIDDEN_FRAME);
				}
				current = replacement;
			}
			else
			{
				this.SetItems(current, items);
			}
			this.UpdateGuideText(current);
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
			if (this.stack.length == 0)
			{
				return false;
			}
			this.PlaySound("PlayCancelSound");
			this.CloseTop();
			return true;
		}

		public function OnMainListInjected():void
		{
			var top:Object = this.TopMenu();
			if (top != null)
			{
				stage.focus = top.list;
			}
		}

		public function ProcessUserEvent(name:String, released:Boolean):Boolean
		{
			if (this.stack.length == 0)
			{
				return false;
			}
			if (released && (name == "Cancel" || name == "Start"))
			{
				this.Back();
			}
			return true;
		}

		private function BeginSession():void
		{
			this.menu.ShowSecondPanelBackground(true);
			this.vanillaButtons = this.codeObj.GetButtonHintData();
			if (this.vanillaButtons != null)
			{
				var ButtonHintData:Class = this.FindClass(BUTTON_HINT_CLASS);
				this.backButton = new ButtonHintData("$BACK", "Esc", "PSN_B", "Xenon_B", 1, this.Back);
				var buttons:Object = this.vanillaButtons.slice(0, 0);
				buttons.push(this.backButton);
				this.menu.ButtonHintBar_mc.SetButtonHintData(buttons);
			}
		}

		private function EndSession():void
		{
			if (this.vanillaButtons != null)
			{
				this.menu.ButtonHintBar_mc.SetButtonHintData(this.vanillaButtons);
				this.vanillaButtons = null;
				this.backButton = null;
			}
			this.menu.ShowSecondPanelBackground(false);
			stage.focus = this.menu.MainPanel_mc.List_mc;
		}

		private function CloseTop():void
		{
			var closed:Object = this.stack.pop();
			this.DestroyPanel(closed);
			this.codeObj.OnMenuClosed(closed.id);
			var top:Object = this.TopMenu();
			if (top == null)
			{
				this.EndSession();
				return;
			}
			this.ShowPanel(top);
			stage.focus = top.list;
		}

		private function CreatePanel(options:Boolean, itemCount:uint):Object
		{
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
				var rows:uint = Math.max(MIN_LIST_ROWS, Math.min(itemCount, MAX_LIST_ROWS));
				list.border.height = list.border.height * rows / list.numListItems;
				list.numListItems = rows;
			}
			return {"panel": panel, "list": list, "options": options};
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
			var objects:Array = target.options ?
				[[target.list, "settingsList"], [target.panel.GuideText_tf, "settingGuideText"]] :
				[[target.panel, "settingsCategoryList"]];
			for each (var pair:Array in objects)
			{
				var handle:uint = this.codeObj.AddShaderTarget(pair[0], pair[1]);
				if (handle != 0)
				{
					target.shaderTargets.push(handle);
				}
			}
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
				if (target.options)
				{
					entry.ID = item.itemId;
					entry.movieType = this.MovieTypeFor(item.itemType);
					entry.value = item.value;
					entry.step = item.step;
					if (item.itemType == TYPE_STEPPER)
					{
						entry.options = item.options;
					}
				}
				entries.push(entry);
			}

			var list:Object = target.list;
			list.entryList = entries;
			if (target.options)
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
		}

		private function MovieTypeFor(itemType:uint):uint
		{
			switch (itemType)
			{
				case TYPE_SLIDER:
					return MOVIE_SLIDER;
				case TYPE_STEPPER:
					return MOVIE_STEPPER;
				default:
					return MOVIE_CHECKBOX;
			}
		}

		private function FirstEnabledIndex(entries:Array):int
		{
			for (var i:int = 0; i < entries.length; ++i)
			{
				if (!entries[i].disabled)
				{
					return i;
				}
			}
			return entries.length > 0 ? 0 : -1;
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
				control.visible = entry.itemType != TYPE_BUTTON;
				control.mouseEnabled = !entry.disabled;
				control.mouseChildren = !entry.disabled;
				if (entry.itemType == TYPE_SLIDER && entry.step > 0)
				{
					control.StepSize = entry.step;
				}
			}
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
			if (!target.options)
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
			var top:Object = this.TopMenu();
			if (top != null && top.options)
			{
				this.ApplyRowStates(top);
			}
		}

		private function onItemPress(event:Event):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (top == null || event.target != top.list)
			{
				return;
			}
			var entry:Object = top.list.selectedEntry;
			if (entry == null)
			{
				return;
			}
			if (entry.disabled)
			{
				this.PlaySound("PlayCancelSound");
				return;
			}
			if (top.options && entry.itemType != TYPE_BUTTON)
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
			if (entry == null || entry.itemType == TYPE_BUTTON)
			{
				return;
			}
			this.codeObj.OnValueChange(top.id, entry.itemId, clip.value);
		}

		private function onSelectionChange(event:Event):void
		{
			event.stopPropagation();
			var top:Object = this.TopMenu();
			if (top != null && event.target == top.list)
			{
				this.UpdateGuideText(top);
			}
		}

		private function onKeyDownCapture(event:KeyboardEvent):void
		{
			var top:Object = this.TopMenu();
			if (top == null || !top.options || (event.keyCode != Keyboard.LEFT && event.keyCode != Keyboard.RIGHT))
			{
				return;
			}
			var entry:Object = top.list.selectedEntry;
			if (entry != null && (entry.disabled || entry.itemType == TYPE_BUTTON))
			{
				event.stopPropagation();
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
