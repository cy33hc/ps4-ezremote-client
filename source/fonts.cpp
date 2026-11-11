#include <string>

#include "imgui.h"
#include "fonts.h"
#include "config.h"
#include "lang.h"
#include "util.h"
#include <orbis/SystemService.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace Fonts
{
    struct CachedFont
    {
        std::vector<unsigned char> data;
    };

    static CachedFont roboto_font;
    static CachedFont roboto_ext_font;
    static CachedFont fa_font;
    static CachedFont of_font;

    static bool ReadCachedFont(const char *path, CachedFont &cache)
    {
        if (!cache.data.empty())
            return true;

        FILE *fp = fopen(path, "rb");
        if (fp == nullptr)
        {
            return false;
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size <= 0)
        {
            fclose(fp);
            return false;
        }

        cache.data.resize((size_t) size);
        size_t read = fread(cache.data.data(), 1, cache.data.size(), fp);
        fclose(fp);
        if (read != cache.data.size())
        {
            cache.data.clear();
            return false;
        }

        return true;
    }

    // At startup the app0 alias is often readable before the pfsmnt path is
    // mounted; later it is the other way round. Try both.
    static bool ReadFontWithFallback(const char *path, CachedFont &cache)
    {
        if (ReadCachedFont(path, cache))
            return true;

        const char *prefix = "/mnt/sandbox/pfsmnt/RMTC00001-app0";
        size_t prefix_len = strlen(prefix);
        if (strncmp(path, prefix, prefix_len) != 0)
            return false;

        std::string alt = std::string("/app0") + (path + prefix_len);
        return ReadCachedFont(alt.c_str(), cache);
    }

    static ImFont *AddCachedFont(ImFontAtlas *atlas, const char *path, float size,
                                 const ImFontConfig *cfg, const ImWchar *ranges)
    {
        CachedFont *cache = nullptr;
        if (strcmp(path, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto.ttf") == 0)
            cache = &roboto_font;
        else if (strcmp(path, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf") == 0)
            cache = &roboto_ext_font;
        else if (strcmp(path, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/fa-solid-900.ttf") == 0)
            cache = &fa_font;
        else if (strcmp(path, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/OpenFontIcons.ttf") == 0)
            cache = &of_font;

        if (cache == nullptr || !ReadFontWithFallback(path, *cache))
            return nullptr;

        ImFontConfig local;
        if (cfg != nullptr)
            local = *cfg;
        // The cache outlives the atlas; never let ImGui free it.
        local.FontDataOwnedByAtlas = false;
        return atlas->AddFontFromMemoryTTF(cache->data.data(), (int) cache->data.size(), size, &local, ranges);
    }

    void Load()
    {
		ImGuiIO &io = ImGui::GetIO();

		// /app0 seems to stop accepting new opens once the app is running, so
		// keep the font bytes resident and rebuild atlases from memory.
		ReadFontWithFallback("/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto.ttf", roboto_font);
		ReadFontWithFallback("/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", roboto_ext_font);
		ReadFontWithFallback("/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/fa-solid-900.ttf", fa_font);
		ReadFontWithFallback("/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/OpenFontIcons.ttf", of_font);
		io.Fonts->Clear();
		io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;

		// European/Latin languages do not need the CJK block. Building the
		// full CJK atlas twice (startup + a language change) is very memory
		// hungry on the PS4, so keep the common case small.
		static const ImWchar latin_ranges[] = {
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0100, 0x024F, // Latin Extended
			0x0370, 0x03FF, // Greek
			0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
			0x0590, 0x05FF, // Hebrew
			0x1E00, 0x1EFF, // Latin Extended Additional
			0x1F00, 0x1FFF, // Greek Extended
			0x2000, 0x206F, // General Punctuation
			0x2100, 0x214F, // Letterlike Symbols
			0x2460, 0x24FF, // Enclosed Alphanumerics
			0x2DE0, 0x2DFF, // Cyrillic Extended-A
			0xA640, 0xA69F, // Cyrillic Extended-B
			0,
		};

		static const ImWchar arabic[] = { // Arabic
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0100, 0x024F, // Latin Extended
			0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
			0x1E00, 0x1EFF, // Latin Extended Additional
			0x2000, 0x206F, // General Punctuation
			0x2100, 0x214F, // Letterlike Symbols
			0x2460, 0x24FF, // Enclosed Alphanumerics
			0x0600, 0x06FF, // Arabic
			0x0750, 0x077F, // Arabic Supplement
			0x0870, 0x089F, // Arabic Extended-B
			0x08A0, 0x08FF, // Arabic Extended-A
			0xFB50, 0xFDFF, // Arabic Presentation Forms-A
			0xFE70, 0xFEFF, // Arabic Presentation Forms-B
			0,
		};

		static const ImWchar fa_icons[] {
			0xF07B, 0xF07B, // folder
			0xF65E, 0xF65E, // new folder
			0xF15B, 0xF15B, // file
			0xF021, 0xF021, // refresh
			0xF0CA, 0xF0CA, // select all
			0xF0C9, 0xF0C9, // unselect all
			0x2700, 0x2700, // cut
			0xF0C5, 0xF0C5, // copy
			0xF0EA, 0xF0EA, // paste
			0xF31C, 0xF31C, // edit
			0xE0AC, 0xE0AC, // rename
			0xE5A1, 0xE5A1, // delete
			0xF002, 0xF002, // search
			0xF013, 0xF013, // settings
			0xF0ED, 0xF0ED, // download
			0xF0EE, 0xF0EE, // upload
			0xF56E, 0xF56E, // extract
			0xF56F, 0xF56F, // compress
			0xF0F6, 0xF0F6, // properties
			0xF112, 0xF112, // cancel
			0xF0DA, 0xF0DA, // arrow right
			0xF017, 0xF017, // activities (clock)
			0xF019, 0xF019, // download
			0x0031, 0x0031, // 1
			0x004C, 0x004C, // L
			0x0052, 0x0052, // R
			0,
		};

		static const ImWchar of_icons[] {
			0xE0CB, 0xE0CB, // square
			0xE0DE, 0xE0DE, // triangle
			0,
		};

		std::string lang = std::string(language);
		int32_t lang_idx = 0;
		sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &lang_idx);

		lang = Util::Trim(lang, " ");
		bool use_system_lang = lang.empty() || lang.compare("Default") == 0;

		if (lang.compare("Korean") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_KOREAN))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesKorean());
		}
		else if (lang.compare("Simplified Chinese") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_CHINESE_S))
		{
			ImFontConfig config;
			config.OversampleH = 1;
			config.OversampleV = 1;
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
		}
		else if (lang.compare("Traditional Chinese") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_CHINESE_T))
		{
			ImFontConfig config;
			config.OversampleH = 1;
			config.OversampleV = 1;
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
		}
		else if (lang.compare("Japanese") == 0 || lang.compare("Ryukyuan") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_JAPANESE))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
		}
		else if (lang.compare("Thai") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_THAI))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesThai());
		}
		else if (lang.compare("Vietnamese") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_VIETNAMESE))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesVietnamese());
		}
		else if (lang.compare("Greek") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_GREEK))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesGreek());
		}
		else if (lang.compare("Arabic") == 0 || (use_system_lang && lang_idx == ORBIS_SYSTEM_PARAM_LANG_ARABIC))
		{
			AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto_ext.ttf", 26.0f, NULL, arabic);
		}
		else
		{
			if (AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/Roboto.ttf", 26.0f, NULL, latin_ranges) == nullptr)
				io.Fonts->AddFontDefault();
		}
		ImFontConfig config;
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
		AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/fa-solid-900.ttf", 20.0f, &config, fa_icons);
		AddCachedFont(io.Fonts, "/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/fonts/OpenFontIcons.ttf", 20.0f, &config, of_icons);
		io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
		io.Fonts->Build();
    }
}
