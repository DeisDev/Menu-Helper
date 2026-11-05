#include "Config.h"
#include "Hotkeys.h"
#include "Injector.h"
#include "JsonMenus.h"
#include "Papyrus.h"
#include "Translations.h"

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
			case F4SE::MessagingInterface::kGameDataReady:
				MenuHelper::Translations::OnGameReady();
				MenuHelper::Hotkeys::Install();
				MenuHelper::JsonMenus::Load();
				break;
			case F4SE::MessagingInterface::kPreLoadGame:
			case F4SE::MessagingInterface::kNewGame:
				MenuHelper::Papyrus::ClearRegistrations();
				break;
			default:
				break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	REX::INFO("MenuHelper loading");
	MenuHelper::Config::Load();
	MenuHelper::Injector::Install();
	if (!F4SE::GetPapyrusInterface()->Register(MenuHelper::Papyrus::RegisterFunctions)) {
		REX::ERROR("failed to register Papyrus functions");
	}
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
