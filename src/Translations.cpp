#include "Translations.h"

#include "Config.h"

#include <glaze/glaze.hpp>

namespace MenuHelper::Translations
{
	namespace
	{
		constexpr auto kDirectory = "Data/Interface/MenuHelper/Translations"sv;
		constexpr auto kFallbackLanguage = "en"sv;
		constexpr auto kTemplateSuffix = "template"sv;
		constexpr auto kInvalidNameChars = "\\/:*?\"<>|"sv;
		constexpr auto kUtf8Bom = "\xEF\xBB\xBF"sv;

		using StringMap = std::map<std::string, std::string, std::less<>>;

		char ToLower(char a_char)
		{
			return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char)));
		}

		struct CaseInsensitiveLess
		{
			using is_transparent = void;

			bool operator()(std::string_view a_lhs, std::string_view a_rhs) const
			{
				return std::ranges::lexicographical_compare(a_lhs, a_rhs, {}, ToLower, ToLower);
			}
		};

		struct Table
		{
			Table() = default;
			Table(const Table&) = delete;
			Table(Table&&) = default;
			Table& operator=(const Table&) = delete;
			Table& operator=(Table&&) = default;

			StringMap exact;
			// Views into exact. Papyrus hands over strings in the casing of whichever script used them first.
			std::map<std::string_view, std::string_view, CaseInsensitiveLess> folded;

			std::optional<std::string_view> Find(std::string_view a_text) const
			{
				if (const auto it = exact.find(a_text); it != exact.end()) {
					return it->second;
				}
				if (const auto it = folded.find(a_text); it != folded.end()) {
					return it->second;
				}
				return std::nullopt;
			}
		};

		struct ModTables
		{
			Table language;
			Table fallback;
		};

		std::mutex                                       g_lock;
		bool                                             g_ready{ false };
		std::optional<std::string>                       g_language;
		std::map<std::string, ModTables, std::less<>>    g_mods;
		std::map<std::string, StringMap, std::less<>>    g_templates;

		std::filesystem::path Utf8Path(std::string_view a_text)
		{
			return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(a_text.data()), a_text.size()));
		}

		std::string ReadLanguage()
		{
			const auto setting = RE::GetINISetting("sLanguage:General");
			if (!setting || setting->GetType() != RE::Setting::SETTING_TYPE::kString || setting->GetString().empty()) {
				REX::WARN("sLanguage:General not found; using {} translations", kFallbackLanguage);
				return std::string{ kFallbackLanguage };
			}
			std::string language{ setting->GetString() };
			std::ranges::transform(language, language.begin(), ToLower);
			REX::INFO("game language: {}", language);
			return language;
		}

		std::string FileName(std::string_view a_modName, std::string_view a_suffix)
		{
			return std::format("{}_{}.json", a_modName, a_suffix);
		}

		std::filesystem::path PathFor(std::string_view a_fileName)
		{
			return std::filesystem::path(kDirectory) / Utf8Path(a_fileName);
		}

		std::optional<StringMap> ReadFile(std::string_view a_fileName)
		{
			std::ifstream file(PathFor(a_fileName), std::ios::binary);
			if (!file) {
				return std::nullopt;
			}
			std::string buffer{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			if (buffer.starts_with(kUtf8Bom)) {
				buffer.erase(0, kUtf8Bom.size());
			}
			StringMap result;
			if (const auto error = glz::read<glz::opts{ .comments = true }>(result, buffer)) {
				REX::ERROR("{}/{}: {}", kDirectory, a_fileName, glz::format_error(error, buffer));
				return std::nullopt;
			}
			return result;
		}

		Table LoadTable(std::string_view a_modName, std::string_view a_language)
		{
			const auto fileName = FileName(a_modName, a_language);
			auto       read = ReadFile(fileName);
			if (!read) {
				return {};
			}
			Table table;
			table.exact = std::move(*read);
			std::erase_if(table.exact, [](const auto& a_pair) { return a_pair.second.empty(); });
			for (const auto& [text, translation] : table.exact) {
				table.folded.emplace(text, translation);
			}
			REX::INFO("loaded {} translations from {}/{}", table.exact.size(), kDirectory, fileName);
			return table;
		}

		void AddToTemplateLocked(std::string_view a_modName, std::string_view a_text)
		{
			const auto fileName = FileName(a_modName, kTemplateSuffix);
			auto       it = g_templates.find(a_modName);
			if (it == g_templates.end()) {
				it = g_templates.emplace(a_modName, ReadFile(fileName).value_or(StringMap{})).first;
			}
			if (!it->second.emplace(a_text, a_text).second) {
				return;
			}

			std::string data;
			if (const auto error = glz::write<glz::opts{ .prettify = true }>(it->second, data)) {
				REX::ERROR("could not write {}/{}: {}", kDirectory, fileName, glz::format_error(error, data));
				return;
			}
			const auto      path = PathFor(fileName);
			std::error_code ec;
			std::filesystem::create_directories(path.parent_path(), ec);
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (ec || !file || !file.write(data.data(), static_cast<std::streamsize>(data.size()))) {
				REX::ERROR("could not write {}/{}", kDirectory, fileName);
			}
		}

		const ModTables* TablesFor(std::string_view a_modName)
		{
			if (!g_language) {
				if (!g_ready) {
					return nullptr;
				}
				g_language = ReadLanguage();
			}
			if (const auto it = g_mods.find(a_modName); it != g_mods.end()) {
				return &it->second;
			}
			ModTables tables;
			tables.language = LoadTable(a_modName, *g_language);
			if (*g_language != kFallbackLanguage) {
				tables.fallback = LoadTable(a_modName, kFallbackLanguage);
			}
			return &g_mods.emplace(a_modName, std::move(tables)).first->second;
		}
	}

	bool IsValidModName(std::string_view a_modName)
	{
		if (a_modName.empty() || a_modName.front() == ' ' || a_modName.back() == ' ' || a_modName.back() == '.') {
			return false;
		}
		return std::ranges::none_of(a_modName, [](char a_char) {
			return static_cast<unsigned char>(a_char) < 0x20 || kInvalidNameChars.contains(a_char);
		});
	}

	void AddToTemplate(std::string_view a_modName, std::string_view a_text)
	{
		if (a_text.empty() || !Config::Get().exportTemplates) {
			return;
		}
		const std::scoped_lock l{ g_lock };
		AddToTemplateLocked(a_modName, a_text);
	}

	void OnGameReady()
	{
		const std::scoped_lock l{ g_lock };
		g_ready = true;
	}

	std::string_view Lookup(std::string_view a_modName, std::string_view a_text)
	{
		if (a_text.empty()) {
			return a_text;
		}
		const std::scoped_lock l{ g_lock };
		if (Config::Get().exportTemplates) {
			AddToTemplateLocked(a_modName, a_text);
		}
		const auto* tables = TablesFor(a_modName);
		if (!tables) {
			return a_text;
		}
		for (const auto* table : { &tables->language, &tables->fallback }) {
			if (const auto translation = table->Find(a_text)) {
				return *translation;
			}
		}
		return a_text;
	}
}
