#pragma once

namespace MenuHelper::Config
{
	enum class ModsFolder
	{
		kAuto,
		kAlways,
		kNever
	};

	struct Options
	{
		ModsFolder modsFolder{ ModsFolder::kAuto };
		bool       exportTemplates{ false };
	};

	// Reads Data/F4SE/Plugins/MenuHelper.ini once at plugin load.
	void           Load();
	const Options& Get();
}
