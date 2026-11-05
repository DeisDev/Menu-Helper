#include "Injector.h"
#include "Papyrus.h"

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == F4SE::MessagingInterface::kPreLoadGame || a_msg->type == F4SE::MessagingInterface::kNewGame) {
			MenuHelper::Papyrus::ClearRegistrations();
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	REX::INFO("MenuHelper loading");
	MenuHelper::Injector::Install();
	if (!F4SE::GetPapyrusInterface()->Register(MenuHelper::Papyrus::RegisterFunctions)) {
		REX::ERROR("failed to register Papyrus functions");
	}
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
