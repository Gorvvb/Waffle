#pragma once

#include "Waffle/Core/Base.h"
#include "TextureFilter.h"

#include <filesystem>
#include <string>

#include "Waffle/Core/Ref.h"

namespace Waffle {

	std::filesystem::path ResolveTexturePath(const std::string& texturePath);
	std::string GetNormalizedAssetPath(const std::string& fullPath);
	void SetActiveAssetDirectory(const std::filesystem::path& path);
	const std::filesystem::path& GetActiveAssetDirectory();

	class Texture : public RefCounted
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint64_t GetRendererID() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;
		
		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual void SetFilter(TextureFilter filter) = 0;

		virtual std::string GetPath() const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, TextureFilter filter = TextureFilter::Linear);
		static Ref<Texture2D> Create(const std::string& path, TextureFilter filter = TextureFilter::Linear);
	};
}