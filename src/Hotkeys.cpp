#include "Hotkeys.h"

#include "JsonMenus.h"
#include "Registry.h"
#include "Translations.h"

#include <F4SE/InputMap.h>
#include <REX/W32/USER32.h>

namespace MenuHelper::Hotkeys
{
	namespace
	{
		namespace InputMap = F4SE::InputMap;

		constexpr const char* kModName = "MenuHelper";
		constexpr const char* kNoneText = "$MenuHelper_HotkeyNone";
		constexpr const char* kKeyCodeText = "$MenuHelper_KeyCode";
		constexpr const char* kCodePlaceholder = "{code}";

		constexpr std::array kModifierTexts{
			std::pair{ MENUHELPER_HOTKEY_CTRL, "$MenuHelper_Ctrl" },
			std::pair{ MENUHELPER_HOTKEY_SHIFT, "$MenuHelper_Shift" },
			std::pair{ MENUHELPER_HOTKEY_ALT, "$MenuHelper_Alt" },
		};

		// Keyboard button events carry Windows virtual-key codes.
		constexpr std::uint32_t kEscape = 0x1B;
		constexpr std::uint32_t kBackspace = 0x08;
		constexpr std::uint32_t kDelete = 0x2E;
		constexpr std::int32_t  kShiftKey = 0x10;
		constexpr std::int32_t  kControlKey = 0x11;
		constexpr std::int32_t  kAltKey = 0x12;
		constexpr std::array    kModifierKeys{ 0x10u, 0x11u, 0x12u, 0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u };
		// XInput B, which cancels when gamepad hotkeys are not allowed.
		constexpr std::uint32_t kGamepadCancel = 0x2000;

		struct State
		{
			std::optional<Capture> capture;
			bool                   decided{ false };
			std::optional<double>  value;
			RE::INPUT_DEVICE       device{ RE::INPUT_DEVICE::kNone };
			std::int32_t           idCode{ -1 };
		};

		std::mutex g_lock;
		State      g_state;

		bool IsModifierKey(std::uint32_t a_code)
		{
			return std::ranges::contains(kModifierKeys, a_code);
		}

		std::uint32_t HeldModifiers()
		{
			std::uint32_t bits = 0;
			for (const auto [key, bit] : { std::pair{ kShiftKey, MENUHELPER_HOTKEY_SHIFT }, std::pair{ kControlKey, MENUHELPER_HOTKEY_CTRL }, std::pair{ kAltKey, MENUHELPER_HOTKEY_ALT } }) {
				if (REX::W32::GetKeyState(key) < 0) {
					bits |= bit;
				}
			}
			return bits;
		}

		// F4SE InputMap code of a button, or 0.
		std::uint32_t KeyCode(RE::INPUT_DEVICE a_device, std::int32_t a_idCode)
		{
			const auto id = static_cast<std::uint32_t>(a_idCode);
			switch (a_device) {
				case RE::INPUT_DEVICE::kKeyboard:
					return a_idCode > 0 && id < InputMap::kMacro_NumKeyboardKeys ? id : 0;
				case RE::INPUT_DEVICE::kMouse:
					return a_idCode >= 0 && id < InputMap::kMacro_NumMouseButtons ? InputMap::kMacro_MouseButtonOffset + id : 0;
				case RE::INPUT_DEVICE::kGamepad:
					{
						const auto code = InputMap::GamepadMaskToKeycode(id);
						return code < InputMap::kMaxMacros ? code : 0;
					}
				default:
					return 0;
			}
		}

		// Decides on the first press. Returns false when the press is ignored.
		bool Decide(const Capture& a_capture, RE::INPUT_DEVICE a_device, std::int32_t a_idCode, std::optional<double>& a_value)
		{
			const auto code = KeyCode(a_device, a_idCode);
			const bool modifiers = (a_capture.flags & MENUHELPER_FLAG_HOTKEY_MODIFIERS) != 0;
			switch (a_device) {
				case RE::INPUT_DEVICE::kKeyboard:
					if (code == kEscape) {
						a_value.reset();
						return true;
					}
					if (code == kBackspace || code == kDelete) {
						a_value = 0.0;
						return true;
					}
					if (code == 0 || (modifiers && IsModifierKey(code))) {
						return false;
					}
					break;
				case RE::INPUT_DEVICE::kMouse:
					if (code == 0 || (a_capture.flags & MENUHELPER_FLAG_HOTKEY_MOUSE) == 0) {
						return false;
					}
					break;
				case RE::INPUT_DEVICE::kGamepad:
					if ((a_capture.flags & MENUHELPER_FLAG_HOTKEY_GAMEPAD) == 0) {
						if (static_cast<std::uint32_t>(a_idCode) != kGamepadCancel) {
							return false;
						}
						a_value.reset();
						return true;
					}
					if (code == 0) {
						return false;
					}
					a_value = static_cast<double>(code);
					return true;
				default:
					return false;
			}
			a_value = static_cast<double>(code | (modifiers ? HeldModifiers() : 0));
			return true;
		}

		std::string BaseKeyName(std::uint32_t a_code)
		{
			RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kNone };
			std::uint32_t    id{ 0 };
			if (a_code < InputMap::kMacro_NumKeyboardKeys) {
				device = RE::INPUT_DEVICE::kKeyboard;
				id = a_code;
			} else if (a_code < InputMap::kMacro_MouseWheelOffset) {
				device = RE::INPUT_DEVICE::kMouse;
				id = a_code - InputMap::kMacro_MouseButtonOffset;
			} else if (a_code >= InputMap::kMacro_GamepadOffset && a_code < InputMap::kMaxMacros) {
				device = RE::INPUT_DEVICE::kGamepad;
				id = InputMap::GamepadKeycodeToMask(a_code);
			}
			const auto manager = RE::BSInputDeviceManager::GetSingleton();
			const auto input = manager && device != RE::INPUT_DEVICE::kNone ? manager->devices[std::to_underlying(device)] : nullptr;
			if (input) {
				if (const auto it = input->deviceButtons.find(id); it != input->deviceButtons.end() && it->second && !it->second->name.empty()) {
					return std::string{ Translations::Lookup(kModName, it->second->name.c_str()) };
				}
			}
			std::string text{ Translations::Lookup(kModName, kKeyCodeText) };
			if (const auto at = text.find(kCodePlaceholder); at != std::string::npos) {
				text.replace(at, std::string_view{ kCodePlaceholder }.size(), std::to_string(a_code));
			}
			return text;
		}

		void Fire(const Item& a_item)
		{
			if (a_item.onHotkey) {
				a_item.onHotkey(a_item.id, a_item.value, a_item.user);
			}
			JsonMenus::RunAction(a_item);
		}

		// PlayerControls passes input to its handlers only while no menu takes input.
		class GameplayHandler : public RE::PlayerInputHandler
		{
		public:
			explicit GameplayHandler(RE::PlayerControlsData& a_data) :
				PlayerInputHandler(a_data)
			{}

			bool ShouldHandleEvent(const RE::InputEvent* a_event) override
			{
				return a_event && *a_event->eventType == RE::INPUT_EVENT_TYPE::kButton;
			}

			void OnButtonEvent(const RE::ButtonEvent* a_event) override
			{
				if (!a_event || !a_event->QJustPressed()) {
					return;
				}
				const auto code = KeyCode(a_event->device.get(), a_event->idCode);
				if (code == 0) {
					return;
				}
				const auto held = a_event->device.get() == RE::INPUT_DEVICE::kGamepad ? 0 : HeldModifiers();
				for (const auto& item : Registry::Get().Items(MENUHELPER_ITEM_HOTKEY)) {
					const auto value = static_cast<std::uint32_t>(item.value);
					const auto required = value & ~MENUHELPER_HOTKEY_KEYMASK;
					if (value != 0 && (value & MENUHELPER_HOTKEY_KEYMASK) == code && (held & required) == required) {
						F4SE::GetTaskInterface()->AddUITask([item]() { Fire(item); });
					}
				}
			}
		};
	}

	void Install()
	{
		static std::once_flag once;
		std::call_once(once, [] {
			const auto controls = RE::PlayerControls::GetSingleton();
			if (!controls) {
				REX::ERROR("PlayerControls not found; hotkeys will not work in game");
				return;
			}
			controls->RegisterHandler(new GameplayHandler(controls->data));
			REX::INFO("hotkey handler registered");
		});
	}

	void BeginCapture(const Capture& a_capture)
	{
		const std::scoped_lock l{ g_lock };
		g_state = State{ .capture = a_capture };
	}

	bool IsCapturing()
	{
		const std::scoped_lock l{ g_lock };
		return g_state.capture.has_value();
	}

	std::optional<CaptureResult> Feed(const RE::ButtonEvent& a_event)
	{
		const std::scoped_lock l{ g_lock };
		if (!g_state.capture) {
			return std::nullopt;
		}
		const auto device = a_event.device.get();
		const auto idCode = a_event.idCode;
		if (!g_state.decided) {
			if (a_event.QJustPressed() && Decide(*g_state.capture, device, idCode, g_state.value)) {
				g_state.decided = true;
				g_state.device = device;
				g_state.idCode = idCode;
			}
			return std::nullopt;
		}
		// Swallow the decided key until it is released, so its release does not reach the menu as Back or Defaults.
		const bool released = device == g_state.device && idCode == g_state.idCode && !a_event.QPressed();
		if (!released && !a_event.QJustPressed()) {
			return std::nullopt;
		}
		CaptureResult result{ *g_state.capture, g_state.value };
		g_state = State{};
		return result;
	}

	void CancelCapture()
	{
		const std::scoped_lock l{ g_lock };
		g_state = State{};
	}

	void CancelCapture(MenuHelper_MenuId a_menu)
	{
		const std::scoped_lock l{ g_lock };
		if (g_state.capture && g_state.capture->menu == a_menu) {
			g_state = State{};
		}
	}

	std::string KeyName(std::uint32_t a_hotkey)
	{
		if (a_hotkey == 0) {
			return std::string{ Translations::Lookup(kModName, kNoneText) };
		}
		std::string name;
		for (const auto& [bit, text] : kModifierTexts) {
			if ((a_hotkey & bit) != 0) {
				name += Translations::Lookup(kModName, text);
				name += '+';
			}
		}
		return name + BaseKeyName(a_hotkey & MENUHELPER_HOTKEY_KEYMASK);
	}

	const char* KeyNameStable(std::uint32_t a_hotkey)
	{
		static std::mutex                         lock;
		static std::set<std::string, std::less<>> names;
		const std::scoped_lock                    l{ lock };
		return names.insert(KeyName(a_hotkey)).first->c_str();
	}
}
