#pragma once

#include "Waffle/Core/Base.h"
#include "TextureFilter.h"

#include <filesystem>
#include <string>

#include <glm/glm.hpp>

#include "Waffle/Core/Ref.h"

namespace Waffle {

	std::filesystem::path ResolveTexturePath(const std::string& texturePath);
	std::string GetNormalizedAssetPath(const std::string& fullPath);
	void SetActiveAssetDirectory(const std::filesystem::path& path);
	const std::filesystem::path& GetActiveAssetDirectory();

	// Tight pixel bounds (x0, y0, x1, y1 - top-left origin, x1/y1 exclusive)
	// of the pixels with alpha inside the given window (same coordinate
	// system) of an image file. Falls back to the full window when the file
	// cannot be decoded; results are cached per path + window. Used to align
	// animation frames by their visible content instead of their crop rect.
	glm::vec4 ComputeOpaqueBounds(const std::string& path, const glm::vec4& windowPx);

	class Texture : public RefCounted
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Explicit bridge for drawing this texture through ImGui.
		// OpenGL: the GLuint texture name. Vulkan: an ImGui-owned
		// VkDescriptorSet. Never store or truncate this value - it is only
		// valid as an ImTextureID for the current frame/backend.
		virtual void* GetImGuiTextureId() const = 0;

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