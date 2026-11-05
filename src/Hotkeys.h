#pragma once

#include "MenuHelper/MenuHelper.h"

namespace MenuHelper::Hotkeys
{
	struct Capture
	{
		std::uint32_t     host{ 0 };
		MenuHelper_MenuId menu{ MENUHELPER_INVALID_ID };
		MenuHelper_ItemId item{ MENUHELPER_INVALID_ID };
		std::uint32_t     flags{ 0 };
	};

	struct CaptureResult
	{
		Capture               capture;
		std::optional<double> value;  // empty when cancelled
	};

	// Registers the gameplay handler that fires hotkeys. Call once game data is ready.
	void Install();

	// UI thread. While capturing, the main and pause menus pass every button event to Feed.
	void                         BeginCapture(const Capture& a_capture);
	bool                         IsCapturing();
	std::optional<CaptureResult> Feed(const RE::ButtonEvent& a_event);
	void                         CancelCapture();
	void                         CancelCapture(MenuHelper_MenuId a_menu);

	// Translated display name, for example "Shift+F", or the "none" text for 0.
	std::string KeyName(std::uint32_t a_hotkey);
	// Same text, kept until the game exits.
	const char* KeyNameStable(std::uint32_t a_hotkey);
}
