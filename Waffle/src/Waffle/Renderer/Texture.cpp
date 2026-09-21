#include "wfpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include "stb_image.h"

#include <unordered_map>

namespace Waffle {

	static std::filesystem::path s_ActiveAssetDirectory = "";

	void SetActiveAssetDirectory(const std::filesystem::path& path)
	{
		s_ActiveAssetDirectory = path;
	}

	const std::filesystem::path& GetActiveAssetDirectory()
	{
		return s_ActiveAssetDirectory;
	}

	std::filesystem::path ResolveTexturePath(const std::string& texturePath)
	{
		if (texturePath.empty())
			return {};

		std::filesystem::path p(texturePath);

		std::error_code ec;

		if (!s_ActiveAssetDirectory.empty())
		{
			std::filesystem::path activeRelative = s_ActiveAssetDirectory / p;
			if (std::filesystem::exists(activeRelative, ec))
				return activeRelative;

			std::filesystem::path activeParentRelative = s_ActiveAssetDirectory.parent_path() / p;
			if (std::filesystem::exists(activeParentRelative, ec))
				return activeParentRelative;
		}

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

	glm::vec4 ComputeOpaqueBounds(const std::string& path, const glm::vec4& windowPx)
	{
		// Rounded pixel window, clamped to non-negative sizes.
		int wx0 = (int)glm::floor(windowPx.x);
		int wy0 = (int)glm::floor(windowPx.y);
		int wx1 = (int)glm::ceil(windowPx.z);
		int wy1 = (int)glm::ceil(windowPx.w);
		if (wx1 <= wx0 || wy1 <= wy0)
			return windowPx;

		static std::unordered_map<std::string, glm::vec4> cache;
		std::string key = path + "|" + std::to_string(wx0) + "," + std::to_string(wy0)
			+ "," + std::to_string(wx1) + "," + std::to_string(wy1);
		auto found = cache.find(key);
		if (found != cache.end())
			return found->second;

		glm::vec4 result = windowPx; // fallback: treat the whole window as content

		// Keyframe paths are asset-relative; resolve them against the active
		// project before giving up (the working directory is usually the
		// editor binary's folder, not the project's).
		std::string scanPath = path;
		std::error_code ec;
		if (!std::filesystem::exists(scanPath, ec))
		{
			std::filesystem::path resolved = ResolveTexturePath(path);
			if (!resolved.empty() && std::filesystem::exists(resolved, ec))
				scanPath = resolved.string();
		}

		if (std::filesystem::exists(scanPath, ec))
		{
			int width = 0, height = 0, channels = 0;
			// Force RGBA so the alpha test below always applies; images
			// without an alpha channel decode as fully opaque and return
			// the full window.
			// The texture loaders enable stb's GLOBAL vertical-flip flag
			// (and leave it on) - without forcing it off here the scan
			// would measure the image upside down.
			stbi_set_flip_vertically_on_load(0);
			unsigned char* pixels = stbi_load(scanPath.c_str(), &width, &height, &channels, 4);
			stbi_set_flip_vertically_on_load(1);
			if (pixels && width > 0 && height > 0)
			{
				int x0 = glm::clamp(wx0, 0, width);
				int y0 = glm::clamp(wy0, 0, height);
				int x1 = glm::clamp(wx1, 0, width);
				int y1 = glm::clamp(wy1, 0, height);

				int minX = x1, minY = y1, maxX = x0, maxY = y0;
				constexpr unsigned char alphaThreshold = 8;

				for (int y = y0; y < y1; y++)
				{
					const unsigned char* row = pixels + (size_t)y * width * 4;
					for (int x = x0; x < x1; x++)
					{
						if (row[x * 4 + 3] > alphaThreshold)
						{
							if (x < minX) minX = x;
							if (y < minY) minY = y;
							if (x + 1 > maxX) maxX = x + 1;
							if (y + 1 > maxY) maxY = y + 1;
						}
					}
				}

				if (minX < maxX && minY < maxY)
					result = glm::vec4((float)minX, (float)minY, (float)maxX, (float)maxY);

				stbi_image_free(pixels);
			}
		}

		cache[key] = result;
		return result;
	}
}