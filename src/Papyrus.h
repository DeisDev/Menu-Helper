#pragma once

namespace MenuHelper::Papyrus
{
	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);

	// Papyrus state belongs to one save, so scripts register again after every load.
	void ClearRegistrations();

	// Events for every mod, not only for objects Papyrus created.
	void SendSettingChange(const std::string& a_modName, const std::string& a_saveKey, double a_value);
	void SendAction(const std::string& a_modName, const std::string& a_action);
	// Calls a global function (string asModName, string asAction). False when the VM rejects the call.
	bool CallGlobal(const std::string& a_script, const std::string& a_function, const std::string& a_modName, const std::string& a_action);
}
