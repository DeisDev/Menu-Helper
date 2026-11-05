#pragma once

namespace MenuHelper::Papyrus
{
	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);

	// Papyrus state belongs to one save, so scripts register again after every load.
	void ClearRegistrations();
}
