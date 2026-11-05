#include "Config.h"

#include <SimpleIni.h>
#undef ERROR

namespace MenuHelper::Config
{
	namespace
	{
		constexpr const char* kPath = "Data/F4SE/Plugins/MenuHelper.ini";

		Options g_options;

		ModsFolder ParseModsFolder(std::string_view a_value)
		{
			std::string value{ a_value };
			std::ranges::transform(value, value.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
			if (value == "auto") {
				return ModsFolder::kAuto;
			}
			if (value == "always") {
				return ModsFolder::kAlways;
			}
			if (value == "never") {
				return ModsFolder::kNever;
			}
			REX::WARN("{}: sModsFolder must be auto, always or never, not \"{}\"; using auto", kPath, a_value);
			return ModsFolder::kAuto;
		}
	}

	void Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (!std::filesystem::exists(kPath)) {
			REX::INFO("{} not found; using defaults", kPath);
			return;
		}
		if (ini.LoadFile(kPath) < 0) {
			REX::ERROR("could not read {}; using defaults", kPath);
			return;
		}
		g_options.modsFolder = ParseModsFolder(ini.GetValue("General", "sModsFolder", "auto"));
		g_options.exportTemplates = ini.GetBoolValue("Translations", "bExportTemplates", false);
	}

	const Options& Get()
	{
		return g_options;
	}
}
