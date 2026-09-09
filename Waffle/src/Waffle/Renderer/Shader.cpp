#include "wfpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"

	namespace Waffle {

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
			// FNV-1a over the sorted define set - order-independent and
			// stable across runs, so cache file names collide only for
			// identical define sets.
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

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		WF_CORE_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		m_Shaders[name] = shader;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name)
	{
		WF_CORE_ASSERT(Exists(name), "Shader not found!");
		// find(), not operator[]: a miss must not insert a null Ref that
		// would make every future Exists() check lie.
		auto it = m_Shaders.find(name);
		return it != m_Shaders.end() ? it->second : nullptr;
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

}