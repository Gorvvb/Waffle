#pragma once

	#include <string>
	#include <unordered_map>
	#include <glm/glm.hpp>

	#include "Waffle/Core/Ref.h"

	namespace Waffle {

		class Shader : public RefCounted
		{
		private:
			uint32_t m_RendererID;
		public:
			virtual ~Shader() = default;

			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;

			virtual void SetInt(const std::string& name, int value) = 0;
			virtual void SetIntArray(const std::string& name, int* values, uint32_t count) = 0;
			virtual void SetFloat(const std::string& name, float value) = 0;
			virtual void SetFloat2(const std::string& name, const glm::vec2 value) = 0;
			virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
			virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
			virtual void SetMat3(const std::string& name, const glm::mat3& value) = 0;
			virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;

			virtual const std::string& GetName() const = 0;

		// Global preprocessor defines applied to every shader compiled
		// afterwards (both backends). Used to feed device limits - e.g.
		// WF_MAX_TEXTURE_SLOTS - into shader source before compilation.
		// Must be set BEFORE Shader::Create for a given shader.
		static void AddGlobalDefine(const std::string& name, const std::string& value);
		static const std::unordered_map<std::string, std::string>& GetGlobalDefines();
		// Stable hash of the current define set - SPIR-V cache file names must
		// incorporate it, or a cache built with one device's limits (e.g.
		// u_Textures[32]) gets silently reused on another (16).
		static std::string GetGlobalDefinesCacheTag();

			static Ref<Shader> Create(const std::string& filepath);
			static Ref<Shader> Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource);
		};

	class ShaderLibrary
	{
	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	public:
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);
		Ref<Shader> Load(const std::string& filepath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;
	};
}