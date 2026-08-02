#include "wfpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Waffle {

	std::filesystem::path ResolveTexturePath(const std::string& texturePath)
	{
		if (texturePath.empty())
			return {};

		std::filesystem::path p(texturePath);

		std::error_code ec;
		if (std::filesystem::exists(p, ec))
			return p;

		std::string normPath = p.string();
		for (char& c : normPath)
		{
			if (c == '\\') c = '/';
		}

		std::string lowerNorm = normPath;
		for (char& c : lowerNorm) { c = (char)tolower(c); }

		size_t assetsPos = lowerNorm.find("assets/");
		if (assetsPos != std::string::npos)
		{
			std::string relativeFromAssets = normPath.substr(assetsPos);
			if (std::filesystem::exists(relativeFromAssets, ec))
				return relativeFromAssets;

			std::string afterAssets = normPath.substr(assetsPos + 7);
			if (std::filesystem::exists(std::filesystem::path("Assets") / afterAssets, ec))
				return std::filesystem::path("Assets") / afterAssets;
		}

		std::filesystem::path assetsRelative = std::filesystem::path("Assets") / p;
		if (std::filesystem::exists(assetsRelative, ec))
			return assetsRelative;

		if (std::filesystem::exists("Assets", ec))
		{
			std::string filename = p.filename().string();
			for (const auto& entry : std::filesystem::recursive_directory_iterator("Assets", ec))
			{
				if (entry.is_regular_file(ec) && entry.path().filename().string() == filename)
					return entry.path();
			}
		}

		if (std::filesystem::exists("Projects", ec))
		{
			std::string filename = p.filename().string();
			for (const auto& entry : std::filesystem::recursive_directory_iterator("Projects", ec))
			{
				if (entry.is_regular_file(ec) && entry.path().filename().string() == filename)
					return entry.path();
			}
		}

		return p;
	}

	std::string GetNormalizedAssetPath(const std::string& fullPath)
	{
		if (fullPath.empty())
			return "";

		std::string normPath = fullPath;
		for (char& c : normPath)
		{
			if (c == '\\') c = '/';
		}

		std::string lowerNorm = normPath;
		for (char& c : lowerNorm) { c = (char)tolower(c); }

		size_t assetsPos = lowerNorm.find("assets/");
		if (assetsPos != std::string::npos)
		{
			return normPath.substr(assetsPos);
		}

		return normPath;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, TextureFilter filter)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGlTexture2D>(width, height, filter);
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanTexture2D>(width, height, filter);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path, TextureFilter filter)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGlTexture2D>(path, filter);
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanTexture2D>(path, filter);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}