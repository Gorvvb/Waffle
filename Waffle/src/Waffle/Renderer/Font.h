#pragma once

#include "Waffle/Core/Ref.h"
#include "Waffle/Renderer/Texture.h"

#include <string>
#include <unordered_map>

namespace Waffle {

	// Baked bitmap font for runtime game text (UI). A TTF is rasterized once
	// per (path, pixel size) into an RGBA atlas texture and queried per glyph.
	// Coordinates are in the engine's UI space: origin top-left, Y down, the
	// pen sitting on the baseline (glyph boxes extend into negative Y above
	// the baseline).
	class Font : public RefCounted
	{
	public:
		struct Glyph
		{
			float X0 = 0, Y0 = 0;       // offsets from pen position (pixels)
			float X1 = 0, Y1 = 0;
			float U0 = 0, V0 = 0;       // atlas UVs
			float U1 = 0, V1 = 0;
			float Advance = 0.0f;       // horizontal pen advance
		};

		Font() = default;
		~Font() = default;

		bool LoadFromFile(const std::string& path, float pixelSize);

		// Resolves and caches a font. An empty requestedPath falls back to
		// the engine default font. Returns nullptr when nothing can be loaded.
		static Ref<Font> Resolve(const std::string& requestedPath, float pixelSize);
		static void ClearCache();

		const Glyph* GetGlyph(char c) const;

		const Ref<Texture2D>& GetAtlas() const { return m_Atlas; }
		float GetSize() const { return m_Size; }
		float GetAscent() const { return m_Ascent; }
		float GetDescent() const { return m_Descent; }
		float GetTextWidth(const std::string& text, float scale = 1.0f) const;

		bool IsLoaded() const { return m_Atlas != nullptr; }

	private:
		bool Bake(const uint8_t* data, size_t size, float pixelSize);

		std::unordered_map<char, Glyph> m_Glyphs;
		Ref<Texture2D> m_Atlas;
		float m_Size = 24.0f;
		float m_Ascent = 0.0f;
		float m_Descent = 0.0f;
	};

}
