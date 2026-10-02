#include "wfpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Waffle/Core/VFS.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"

#ifdef WF_PLATFORM_WINDOWS
#include <windows.h>
#endif

	namespace Waffle {

		// Built-in shaders load lazily, possibly after the editor changed the CWD, so fall back to the exe directory (where the build copies Assets/shaders); mounted VFS archives (exported games) keep using virtual paths.
		static std::string ResolveShaderFile(const std::string& path)
		{
			std::error_code ec;
			if (VFS::IsMounted() ? VFS::Exists(path) : (std::filesystem::exists(path, ec) && !ec))
				return path;

			std::filesystem::path exeDir;
#ifdef WF_PLATFORM_WINDOWS
			char buffer[MAX_PATH] = {};
			if (GetModuleFileNameA(NULL, buffer, MAX_PATH) > 0)
				exeDir = std::filesystem::path(buffer).parent_path();
#endif
			if (!exeDir.empty())
			{
				if (std::filesystem::exists(exeDir / path, ec) && !ec)
					return (exeDir / path).string();

				// Build layout uses "Assets" (capitalized); the code asks for "assets/..." - cover both spellings so case-sensitive filesystems work too.
				if (path.rfind("assets/", 0) == 0)
				{
					std::filesystem::path cap = exeDir / "Assets" / path.substr(7);
					if (std::filesystem::exists(cap, ec) && !ec)
						return cap.string();
				}
			}
			return path;
		}

		static std::unordered_map<std::string, std::string>& GetGlobalDefineStorage()
		{
			static std::unordered_map<std::string, std::string> s_Defines;
			return s_Defines;
		}

		void Shader::AddGlobalDefine(const std::string& name, const std::string& value)
		{
			GetGlobalDefineStorage()[name] = value;
		}

		const std::unordered_map<std::string, std::string>& Shader::GetGlobalDefines()
		{
			return GetGlobalDefineStorage();
		}

		std::string Shader::GetGlobalDefinesCacheTag()
		{
			// FNV-1a over the sorted define set - order-independent and stable across runs, so cache file names collide only for identical define sets.
			std::vector<std::string> parts;
			parts.reserve(GetGlobalDefineStorage().size());
			for (const auto& [name, value] : GetGlobalDefineStorage())
				parts.push_back(name + "=" + value);
			std::sort(parts.begin(), parts.end());

			uint64_t hash = 1469598103934665603ull;
			auto mix = [&hash](const char* s, size_t n)
			{
				for (size_t i = 0; i < n; i++)
				{
					hash ^= (uint8_t)s[i];
					hash *= 1099511628211ull;
				}
				hash ^= 0xff;
				hash *= 1099511628211ull;
			};

			if (parts.empty())
				mix("none", 4);
			for (const auto& p : parts)
				mix(p.c_str(), p.size());

			char buf[17];
			snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
			return std::string(buf);
		}

		Ref<Shader> Shader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLShader>(filepath);
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanShader>(filepath);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLShader>(name, vertexSource, fragmentSource);
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanShader>(name, vertexSource, fragmentSource);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	ShaderLibrary& ShaderLibrary::Get()
	{
		static ShaderLibrary s_Instance;
		return s_Instance;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		const std::string resolvedPath = ResolveShaderFile(filepath);

		// A resolved path is either a real file (mtime drives hot reload) or a VFS-virtual path in a mounted archive (exported games) - the latter has no timestamp, so cached entries simply stay stable.
		std::error_code ec;
		bool onDisk = std::filesystem::exists(resolvedPath, ec) && !ec;
		bool fileExists = onDisk || VFS::Exists(resolvedPath);
		std::filesystem::file_time_type lastWrite{};
		if (onDisk)
			lastWrite = std::filesystem::last_write_time(resolvedPath, ec);

		auto it = m_Shaders.find(filepath);
		if (it != m_Shaders.end())
		{
			// Cached - recompile only when the file actually changed. A failed compile is cached as a null shader with the failing file's timestamp, so it retries only on the next edit.
			if (!onDisk || ec || it->second.LastWrite == lastWrite)
				return it->second.Shader;
		}
		else if (!fileExists || ec)
		{
			WF_CORE_WARN("ShaderLibrary: shader file '{0}' not found.", filepath);
			m_Shaders[filepath] = { nullptr, lastWrite };
			return nullptr;
		}

		// Keep serving the last working version if a recompile fails, but stamp the failing file's timestamp so we don't retry every frame.
		Ref<Shader> previous = (it != m_Shaders.end()) ? it->second.Shader : nullptr;

		Ref<Shader> newShader = Shader::Create(resolvedPath);
		if (!newShader || !newShader->IsValid())
		{
			WF_CORE_ERROR("ShaderLibrary: failed to compile '{0}'{1}.",
				resolvedPath, previous ? " - keeping last working version" : "");
			m_Shaders[filepath] = { previous, lastWrite };
			return previous;
		}

		m_Shaders[filepath] = { newShader, lastWrite };
		return newShader;
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name) const
	{
		// find(), not operator[]: a miss must not insert a null Ref that would make every future Exists() check lie.
		auto it = m_Shaders.find(name);
		return it != m_Shaders.end() ? it->second.Shader : nullptr;
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		m_Shaders[shader->GetName()] = { shader, {} };
	}

	void ShaderLibrary::Clear()
	{
		m_Shaders.clear();
	}

}