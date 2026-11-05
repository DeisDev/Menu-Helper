#include "SavedValues.h"

#include <SimpleIni.h>
#undef ERROR

namespace MenuHelper::SavedValues
{
	namespace
	{
		constexpr auto        kDirectory = "Data/F4SE/Plugins/MenuHelper/Settings"sv;
		constexpr const char* kSection = "Settings";

		std::mutex g_lock;

		std::string FileName(std::string_view a_modName)
		{
			return std::format("{}/{}.ini", kDirectory, a_modName);
		}

		std::filesystem::path PathFor(std::string_view a_fileName)
		{
			return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(a_fileName.data()), a_fileName.size()));
		}

		bool Read(std::string_view a_fileName, CSimpleIniA& a_ini)
		{
			a_ini.SetUnicode();
			std::ifstream file(PathFor(a_fileName), std::ios::binary);
			if (!file) {
				return true;
			}
			const std::string data{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			if (a_ini.LoadData(data) < 0) {
				REX::ERROR("could not read {}", a_fileName);
				return false;
			}
			return true;
		}
	}

	bool IsValidKey(std::string_view a_key)
	{
		if (a_key.empty() || a_key.front() == ';' || a_key.front() == '#' || a_key.front() == '[' ||
			a_key.front() == ' ' || a_key.back() == ' ') {
			return false;
		}
		return std::ranges::none_of(a_key, [](char a_char) {
			return static_cast<unsigned char>(a_char) < 0x20 || a_char == '=';
		});
	}

	std::optional<float> Load(std::string_view a_modName, std::string_view a_key)
	{
		const std::scoped_lock l{ g_lock };
		const auto            fileName = FileName(a_modName);
		CSimpleIniA           ini;
		if (!Read(fileName, ini)) {
			return std::nullopt;
		}
		const std::string key{ a_key };
		const char*       text = ini.GetValue(kSection, key.c_str());
		if (!text) {
			return std::nullopt;
		}
		const std::string_view value{ text };
		float                  result{};
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
		if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(result)) {
			REX::WARN("{}: {} has invalid value \"{}\"; using the default", fileName, key, value);
			return std::nullopt;
		}
		return result;
	}

	void Save(std::string_view a_modName, std::string_view a_key, float a_value)
	{
		const std::scoped_lock l{ g_lock };
		const auto            fileName = FileName(a_modName);
		CSimpleIniA           ini;
		if (!Read(fileName, ini)) {
			return;
		}
		const std::string key{ a_key };
		const auto        value = std::format("{}", a_value);
		std::string       data;
		if (ini.SetValue(kSection, key.c_str(), value.c_str()) < 0 || ini.Save(data) < 0) {
			REX::ERROR("could not save {} to {}", key, fileName);
			return;
		}

		const auto      path = PathFor(fileName);
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (error || !file || !file.write(data.data(), static_cast<std::streamsize>(data.size()))) {
			REX::ERROR("could not write {}", fileName);
		}
	}
}
