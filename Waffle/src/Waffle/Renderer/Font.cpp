#include "wfpch.h"
#include "Font.h"

#include "Waffle/Core/Log.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Renderer/Texture.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

// ImGui ships a patched stb_truetype. Pull it in with static linkage so this
// translation unit gets its own private copy and cannot clash with the one
// compiled into imgui_draw.cpp.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

namespace Waffle {

	// ASCII 32..126, the range baked into the atlas.
	static constexpr int s_FirstChar = 32;
	static constexpr int s_CharCount = 96;

#if defined(WF_PLATFORM_WINDOWS)
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
	#include <windows.h>
#endif

	// Absolute path of the running executable. The default font must be
	// found next to the editor/runtime binary no matter what the current
	// working directory is (project folders usually have no fonts).
	static std::filesystem::path GetExecutableDirectory()
	{
#if defined(WF_PLATFORM_WINDOWS)
	char buffer[MAX_PATH] = {};
	DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
	if (len > 0 && len < MAX_PATH)
		return std::filesystem::path(std::string(buffer, len)).parent_path();
#endif
	return std::filesystem::current_path();
	}

	static bool ReadFontBytes(const std::filesystem::path& path, std::vector<uint8_t>& out)
	{
		// Mounted .wpack archives must serve fonts for exported games; fall
		// back to the loose filesystem when nothing is mounted.
		if (VFS::IsMounted() && VFS::Exists(path))
		{
			Buffer buf = VFS::ReadFile(path);
			if (buf.Data && buf.Size > 0)
			{
				out.assign(buf.Data, buf.Data + buf.Size);
				buf.Release();
				return true;
			}
		}

		std::error_code ec;
		if (!std::filesystem::exists(path, ec) || ec)
			return false;

		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream.good())
			return false;

		std::streamoff size = stream.tellg();
		if (size <= 0)
			return false;

		stream.seekg(0, std::ios::beg);
		out.resize((size_t)size);
		stream.read(reinterpret_cast<char*>(out.data()), size);
		return stream.good() || stream.eof();
	}

	bool Font::Bake(const uint8_t* data, size_t size, float pixelSize)
	{
		m_Size = std::clamp(pixelSize, 6.0f, 96.0f);

		stbtt_fontinfo info;
		if (!stbtt_InitFont(&info, data, 0))
		{
			WF_CORE_ERROR("Font: not a valid TTF/OTF font");
			return false;
		}

		float scale = stbtt_ScaleForPixelHeight(&info, m_Size);
		int ascent = 0, descent = 0, lineGap = 0;
		stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
		m_Ascent = (float)ascent * scale;
		m_Descent = (float)descent * scale;

		// Bake into a coverage bitmap, growing the atlas height until all
		// glyphs fit (stbtt_BakeFontBitmap returns a negative index when the
		// bitmap is too small).
		const int atlasWidth = 1024;
		int atlasHeight = 256;
		std::vector<uint8_t> coverage;
		std::vector<stbtt_bakedchar> baked(s_CharCount);
		bool bakedAll = false;

		for (int attempt = 0; attempt < 5 && !bakedAll; attempt++)
		{
			coverage.assign((size_t)atlasWidth * atlasHeight, 0);
			int res = stbtt_BakeFontBitmap(data, 0, m_Size, coverage.data(),
				atlasWidth, atlasHeight, s_FirstChar, s_CharCount, baked.data());
			if (res > 0)
				bakedAll = true;
			else
				atlasHeight *= 2;
		}

		if (!bakedAll)
		{
			WF_CORE_ERROR("Font: could not fit glyphs into a {0}p tall atlas", atlasHeight);
			return false;
		}

		// Expand single-channel coverage into a white RGBA atlas.
		std::vector<uint32_t> rgba(coverage.size());
		for (size_t i = 0; i < coverage.size(); i++)
		{
			uint32_t a = coverage[i];
			rgba[i] = (a << 24) | 0x00FFFFFFu;
		}

		m_Atlas = Texture2D::Create(atlasWidth, atlasHeight, TextureFilter::Linear);
		if (!m_Atlas)
			return false;
		m_Atlas->SetData(rgba.data(), (uint32_t)(rgba.size() * sizeof(uint32_t)));

		m_Glyphs.clear();
		for (int i = 0; i < s_CharCount; i++)
		{
			const stbtt_bakedchar& b = baked[i];
			Glyph g;
			g.X0 = b.xoff;
			g.Y0 = b.yoff;
			g.X1 = b.xoff + (float)(b.x1 - b.x0);
			g.Y1 = b.yoff + (float)(b.y1 - b.y0);
			g.U0 = (float)b.x0 / (float)atlasWidth;
			g.V0 = (float)b.y0 / (float)atlasHeight;
			g.U1 = (float)b.x1 / (float)atlasWidth;
			g.V1 = (float)b.y1 / (float)atlasHeight;
			g.Advance = b.xadvance;
			m_Glyphs[(char)(s_FirstChar + i)] = g;
		}

		return true;
	}

	bool Font::LoadFromFile(const std::string& path, float pixelSize)
	{
		std::vector<uint8_t> data;
		if (!ReadFontBytes(path, data))
		{
			WF_CORE_ERROR("Font: cannot open '{0}'", path);
			return false;
		}
		return Bake(data.data(), data.size(), pixelSize);
	}

	static std::unordered_map<std::string, Ref<Font>>& GetFontCache()
	{
		static std::unordered_map<std::string, Ref<Font>> cache;
		return cache;
	}

	Ref<Font> Font::Resolve(const std::string& requestedPath, float pixelSize)
	{
		// Candidate paths, most specific first.
		std::vector<std::filesystem::path> candidates;
		const std::filesystem::path& assetDir = GetActiveAssetDirectory();

		if (!requestedPath.empty())
		{
			std::filesystem::path p(requestedPath);
			if (p.is_absolute())
			{
				candidates.push_back(p);
			}
			else
			{
				if (!assetDir.empty())
					candidates.push_back(assetDir / p);
				candidates.push_back(std::filesystem::current_path() / p);
				candidates.push_back(p);
			}
		}
		else
		{
			const char* defaultRel = "fonts/OpenSans/OpenSans-Regular.ttf";
			std::filesystem::path exeDir = GetExecutableDirectory();
			if (!assetDir.empty())
				candidates.push_back(assetDir / defaultRel);
			candidates.push_back(exeDir / "Assets" / defaultRel);
			candidates.push_back(exeDir / defaultRel);
			// "Assets/..." form: resolves next to a loose-copy game AND
			// through a mounted .wpack archive (VFS keys are Assets/-rooted).
			candidates.push_back(std::filesystem::path("Assets") / defaultRel);
			candidates.push_back(std::filesystem::current_path() / "Assets" / defaultRel);
			candidates.push_back(std::filesystem::current_path() / defaultRel);
		}

		int sizeKey = (int)(pixelSize * 2.0f); // stable half-pixel granularity
		for (auto& candidate : candidates)
		{
			std::string key = candidate.string() + "@" + std::to_string(sizeKey);

			auto& cache = GetFontCache();
			auto it = cache.find(key);
			if (it != cache.end())
				return it->second;

			std::vector<uint8_t> data;
			if (!ReadFontBytes(candidate, data))
				continue;

			auto font = CreateRef<Font>();
			if (!font->Bake(data.data(), data.size(), pixelSize))
				continue;

			cache[key] = font;
			return font;
		}

		static bool s_WarnedNoFont = false;
		if (!s_WarnedNoFont)
		{
			s_WarnedNoFont = true;
			WF_CORE_WARN("Font: no usable font found (tried {0} candidates) - UI text disabled",
				(int)candidates.size());
		}
		return nullptr;
	}

	void Font::ClearCache()
	{
		GetFontCache().clear();
	}

	const Font::Glyph* Font::GetGlyph(char c) const
	{
		auto it = m_Glyphs.find(c);
		if (it != m_Glyphs.end())
			return &it->second;
		// Everything outside the baked ASCII range renders as space.
		it = m_Glyphs.find(' ');
		return it != m_Glyphs.end() ? &it->second : nullptr;
	}

	float Font::GetTextWidth(const std::string& text, float scale) const
	{
		float width = 0.0f;
		for (char c : text)
		{
			if (const Glyph* g = GetGlyph(c))
				width += g->Advance * scale;
		}
		return width;
	}

}
