#pragma once

	#include <string>
	#include <unordered_map>
	#include <filesystem>
	#include <glm/glm.hpp>

	#include "Waffle/Core/Ref.h"

	namespace Waffle {

		class Shader : public RefCounted
		{
		public:
			virtual ~Shader() = default;

			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;

			// False when construction failed (unreadable/unparseable file) - such a shader must not be used for rendering.
			virtual bool IsValid() const = 0;

			virtual void SetInt(const std::string& name, int value) = 0;
			virtual void SetIntArray(const std::string& name, int* values, uint32_t count) = 0;
			virtual void SetFloat(const std::string& name, float value) = 0;
			virtual void SetFloat2(const std::string& name, const glm::vec2 value) = 0;
			virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
			virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
			virtual void SetMat3(const std::string& name, const glm::mat3& value) = 0;
			virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;

			virtual const std::string& GetName() const = 0;

		// Global preprocessor defines applied to every shader compiled afterwards (both backends); used to feed device limits (e.g. WF_MAX_TEXTURE_SLOTS) into shader source. Must be set BEFORE Shader::Create for a given shader.
		static void AddGlobalDefine(const std::string& name, const std::string& value);
		static const std::unordered_map<std::string, std::string>& GetGlobalDefines();
		// Stable hash of the current define set - SPIR-V cache file names must incorporate it, or a cache built with one device's limits (u_Textures[32]) gets silently reused on another (16).
		static std::string GetGlobalDefinesCacheTag();

			static Ref<Shader> Create(const std::string& filepath);
			static Ref<Shader> Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource);
		};

	// Central shader registry: all engine and user shaders load through it, cached by path and recompiled when the timestamp changes (live reload); keeps the last working shader if a recompile fails. Must be Clear()ed before the graphics context is destroyed (Renderer::Shutdown).
	class ShaderLibrary
	{
	public:
		static ShaderLibrary& Get();

		// Returns the cached shader for `filepath`, compiling on first use and recompiling when the file changed. nullptr if the file is missing or failed to compile (failure remembered, not retried every frame).
		Ref<Shader> Load(const std::string& filepath);

		Ref<Shader> Get(const std::string& name) const;
		bool Exists(const std::string& name) const;

		// Registers an already-created shader under its own name.
		void Add(const Ref<Shader>& shader);

		// Releases every shader - call from Renderer::Shutdown while the graphics context is still alive.
		void Clear();

	private:
		struct Entry
		{
			Ref<Shader> Shader;
			std::filesystem::file_time_type LastWrite{};
		};
		std::unordered_map<std::string, Entry> m_Shaders;
	};
	}
