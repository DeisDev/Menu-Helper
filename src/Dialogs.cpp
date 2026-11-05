#include "Dialogs.h"

#include "Submenus.h"
#include "Translations.h"

namespace MenuHelper::Dialogs
{
	namespace
	{
		constexpr std::size_t kMaxButtons = 4;
		constexpr auto        kHUDMenu = "HUDMenu"sv;

		std::atomic<MenuHelper_BoxId> g_nextId{ 1 };

		// The game owns it through MessageBoxData::callback and deletes it when the box closes.
		class BoxCallback : public RE::IMessageBoxCallback
		{
		public:
			BoxCallback(MenuHelper_BoxId a_id, std::function<void(MenuHelper_BoxId, std::uint32_t)> a_onButton) :
				_id(a_id),
				_onButton(std::move(a_onButton))
			{}

			void operator()(std::uint8_t a_button) override
			{
				if (_onButton) {
					_onButton(_id, a_button);
				}
			}

		private:
			MenuHelper_BoxId                                     _id;
			std::function<void(MenuHelper_BoxId, std::uint32_t)> _onButton;
		};
	}

	MenuHelper_BoxId NextId()
	{
		return g_nextId++;
	}

	MenuHelper_BoxId ShowMessageBox(std::string_view a_modName, std::string_view a_body, const std::vector<std::string>& a_buttons, std::function<void(MenuHelper_BoxId, std::uint32_t)> a_onButton)
	{
		if (a_buttons.empty() || a_buttons.size() > kMaxButtons) {
			REX::WARN("message box of {} rejected: it needs 1 to {} buttons", a_modName, kMaxButtons);
			return MENUHELPER_INVALID_ID;
		}
		const auto               id = NextId();
		std::string              body{ Translations::Lookup(a_modName, a_body) };
		std::vector<std::string> buttons;
		for (const auto& button : a_buttons) {
			buttons.emplace_back(Translations::Lookup(a_modName, button));
		}
		F4SE::GetTaskInterface()->AddTask([id, body = std::move(body), buttons = std::move(buttons), onButton = std::move(a_onButton)]() mutable {
			const auto manager = RE::MessageMenuManager::GetSingleton();
			if (!manager) {
				REX::ERROR("message box {} not shown: MessageMenuManager not found", id);
				return;
			}
			auto label = [&](std::size_t a_index) { return a_index < buttons.size() ? buttons[a_index].c_str() : nullptr; };
			manager->Create(nullptr, body.c_str(), new BoxCallback(id, std::move(onButton)), RE::WARNING_TYPES::kDefault, label(0), label(1), label(2), label(3));
		});
		return id;
	}

	MenuHelper_BoxId ShowConfirm(std::string_view a_modName, std::string_view a_text, std::function<void(MenuHelper_BoxId, bool)> a_onResult)
	{
		const auto id = NextId();
		auto       done = [id, onResult = std::move(a_onResult)](bool a_result) {
            if (onResult) {
                onResult(id, a_result);
            }
		};
		return Submenus::RequestConfirm(id, std::string{ Translations::Lookup(a_modName, a_text) }, std::move(done)) ? id : MENUHELPER_INVALID_ID;
	}

	bool ShowNotification(std::string_view a_modName, std::string_view a_text, std::string_view a_sound, bool a_warning)
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui || !ui->GetMenuOpen(kHUDMenu)) {
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([text = std::string{ Translations::Lookup(a_modName, a_text) }, sound = std::string{ a_sound }, a_warning]() {
			RE::SendHUDMessage::ShowHUDMessage(text.c_str(), sound.c_str(), false, a_warning);
		});
		return true;
	}
}
