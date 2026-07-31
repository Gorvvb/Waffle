#pragma once

#include "Waffle/Renderer/Shader.h"
#include "VulkanVertexArray.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Waffle {

	// -------------------------------------------------------------------------
	// VulkanShader
	// Loads GLSL source, compiles to Vulkan SPIR-V via shaderc (same cache
	// pattern as OpenGLShader), creates VkShaderModules, reflects SPIR-V to
	// discover push-constant ranges and descriptor-set layouts, and creates a
	// VkPipelineLayout.  Graphics pipelines are created lazily and cached by
	// the renderer API keyed on the bound vertex input description.
	// -------------------------------------------------------------------------
	class VulkanShader : public Shader
	{
	public:
		explicit VulkanShader(const std::string& filepath);
		VulkanShader(const std::string& name,
			const std::string& vertexSrc,
			const std::string& fragmentSrc);
		virtual ~VulkanShader();

		virtual void Bind()   const override;
		virtual void Unbind() const override;

		// ---- Uniform setters (implemented via push constants) ---------------
		virtual void SetInt(const std::string& name, int value) override;
		virtual void SetIntArray(const std::string& name, int* values, uint32_t count) override;
		virtual void SetFloat(const std::string& name, float value) override;
		virtual void SetFloat2(const std::string& name, const glm::vec2 value) override;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;
		virtual void SetMat3(const std::string& name, const glm::mat3& value) override;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) override;

		virtual const std::string& GetName() const override { return m_Name; }

		// ---- Vulkan-specific accessors (used by VulkanRendererAPI) ----------
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

		// Returns (or creates) a VkPipeline for the given vertex-array input layout.
		// topology: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST or LINE_LIST etc.
		VkPipeline GetOrCreatePipeline(
			const VulkanVertexArray* vertexArray,
			VkPrimitiveTopology topology,
			const std::vector<VkFormat>& colorFormats,
			VkFormat depthFormat);

		// Flush push constants into the current command buffer.
		// Called by VulkanRendererAPI just before a draw call.
		void FlushPushConstants(VkCommandBuffer cmd) const;

		// Update and bind per-shader descriptor sets before draw calls
		void BindAndFlushDescriptors(VkCommandBuffer cmd, uint32_t frameIndex);

		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const;
		uint32_t GetDescriptorSetLayoutCount() const { return (uint32_t)m_DescriptorSetLayouts.size(); }

	private:
		// ---- Compilation ---------------------------------------------------
		void CompileOrGetVulkanSPIRV(const std::unordered_map<uint32_t, std::string>& sources);
		void CreateShaderModules();
		void ReflectAndCreateLayout();
		void Reflect(uint32_t stage, const std::vector<uint32_t>& spirv);

		std::string  ReadFile(const std::string& filepath);
		std::unordered_map<uint32_t, std::string> PreProcess(const std::string& source);

		static const char* GetVulkanCacheDirectory();
		static void        EnsureCacheDirectoryExists();
		static const char* StageToVulkanCacheExtension(uint32_t glStage);

		// ---- Push-constant helpers -----------------------------------------
		void WritePushConstant(const std::string& name, const void* data, uint32_t size);

		// ---- Pipeline key --------------------------------------------------
		struct PipelineKey
		{
			std::vector<VkVertexInputBindingDescription>   Bindings;
			std::vector<VkVertexInputAttributeDescription> Attributes;
			VkPrimitiveTopology Topology;
			std::vector<VkFormat> ColorFormats;
			VkFormat DepthFormat;

			bool operator==(const PipelineKey& o) const;
		};
		struct PipelineKeyHash
		{
			size_t operator()(const PipelineKey& k) const;
		};

	private:
		std::string  m_FilePath;
		std::string  m_Name;

		// SPIR-V binaries keyed by GL stage enum (GL_VERTEX_SHADER etc.)
		std::unordered_map<uint32_t, std::vector<uint32_t>> m_SPIRV;

		// Shader modules (vert = 0x8B31, frag = 0x8B30 in GL terms)
		VkShaderModule m_VertModule = VK_NULL_HANDLE;
		VkShaderModule m_FragModule = VK_NULL_HANDLE;

		// Pipeline layout (push constants + descriptor set layouts)
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;

		struct ReflectedDescriptor
		{
			uint32_t Set = 0;
			uint32_t Binding = 0;
			VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uint32_t Count = 1;
		};
		std::vector<ReflectedDescriptor> m_ReflectedDescriptors;

		// Per-frame, per-set descriptor sets
		// m_DescriptorSets[frameIndex][setIndex]
		std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets;

		// Push constant info (reflected)
		struct PushConstantMember {
			uint32_t offset;
			uint32_t size;
		};
		std::unordered_map<std::string, PushConstantMember> m_PushConstantMembers;
		uint32_t m_TotalPushConstantSize = 0;
		mutable std::vector<uint8_t> m_PushConstantData; // staging buffer

		// Cached pipelines (lazy, keyed by vertex input + topology)
		std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> m_Pipelines;
	};

} // namespace Waffle
