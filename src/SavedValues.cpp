#include "SavedValues.h"

#include <SimpleIni.h>
#undef ERROR

namespace MenuHelper::SavedValues
{
	namespace
	{
		constexpr auto        kDirectory = "Data/F4SE/Plugins/MenuHelper/Settings"sv;
		constexpr const char* kSection = "Settings";
		// Slider drags change values many times a second; writes wait until changes stop.
		constexpr auto kFlushDelay = std::chrono::milliseconds(750);

		std::mutex                                                    g_lock;
		std::condition_variable                                       g_wake;
		std::map<std::string, std::unique_ptr<CSimpleIniA>, std::less<>> g_files;
		std::set<std::string, std::less<>>                            g_dirty;
		std::chrono::steady_clock::time_point                         g_lastSave;

		std::string FileName(std::string_view a_modName)
		{
			return std::format("{}/{}.ini", kDirectory, a_modName);
		}

		std::filesystem::path PathFor(std::string_view a_fileName)
		{
			return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(a_fileName.data()), a_fileName.size()));
		}

		CSimpleIniA* FileFor(std::string_view a_modName)
		{
			if (const auto it = g_files.find(a_modName); it != g_files.end()) {
				return it->second.get();
			}
			const auto fileName = FileName(a_modName);
			auto       ini = std::make_unique<CSimpleIniA>();
			ini->SetUnicode();
			if (std::ifstream file(PathFor(fileName), std::ios::binary); file) {
				const std::string data{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
				if (ini->LoadData(data) < 0) {
					REX::ERROR("could not read {}; its values are not used or saved", fileName);
					return nullptr;
				}
			}
			return g_files.emplace(a_modName, std::move(ini)).first->second.get();
		}

		void FlushLocked()
		{
			for (const auto& modName : g_dirty) {
				const auto fileName = FileName(modName);
				std::string data;
				if (g_files.at(modName)->Save(data) < 0) {
					REX::ERROR("could not save {}", fileName);
					continue;
				}
				const auto      path = PathFor(fileName);
				std::error_code error;
				std::filesystem::create_directories(path.parent_path(), error);
				std::ofstream file(path, std::ios::binary | std::ios::trunc);
				if (error || !file || !file.write(data.data(), static_cast<std::streamsize>(data.size()))) {
					REX::ERROR("could not write {}", fileName);
				}
			}
			g_dirty.clear();
		}

		void FlushWorker()
		{
			std::unique_lock l{ g_lock };
			for (;;) {
				g_wake.wait(l, [] { return !g_dirty.empty(); });
				while (std::chrono::steady_clock::now() < g_lastSave + kFlushDelay) {
					g_wake.wait_until(l, g_lastSave + kFlushDelay);
				}
				FlushLocked();
			}
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

	std::optional<double> Load(std::string_view a_modName, std::string_view a_key)
	{
		const std::scoped_lock l{ g_lock };
		const auto*           ini = FileFor(a_modName);
		const std::string     key{ a_key };
		const char*           text = ini ? ini->GetValue(kSection, key.c_str()) : nullptr;
		if (!text) {
			return std::nullopt;
		}
		const std::string_view value{ text };
		double                 result{};
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
		if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(result)) {
			REX::WARN("{}: {} has invalid value \"{}\"; using the default", FileName(a_modName), key, value);
			return std::nullopt;
		}
		return result;
	}

	void Save(std::string_view a_modName, std::string_view a_key, double a_value)
	{
		static std::once_flag worker;
		std::call_once(worker, [] { std::thread(FlushWorker).detach(); });

		const std::scoped_lock l{ g_lock };
		auto*                  ini = FileFor(a_modName);
		const std::string      key{ a_key };
		if (!ini || ini->SetValue(kSection, key.c_str(), std::format("{}", a_value).c_str()) < 0) {
			REX::ERROR("could not save {} to {}", key, FileName(a_modName));
			return;
		}
		g_dirty.emplace(a_modName);
		g_lastSave = std::chrono::steady_clock::now();
		g_wake.notify_all();
	}

	void Flush()
	{
		const std::scoped_lock l{ g_lock };
		FlushLocked();
	}
}
