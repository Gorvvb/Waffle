#include "wfpch.h"
#include "VulkanShader.h"
#include "VulkanContext.h"

#include <fstream>
#include <filesystem>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <glad/glad.h>  // for GL_VERTEX_SHADER / GL_FRAGMENT_SHADER constants
#include <glm/gtc/type_ptr.hpp>

#include "Waffle/Core/Timer.h"
#include "Waffle/Core/VFS.h"

namespace Waffle {

	// -------------------------------------------------------------------------
	// Stage helpers
	// -------------------------------------------------------------------------
	static uint32_t ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")                      return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel") return GL_FRAGMENT_SHADER;
		WF_CORE_ASSERT(false, "Unknown shader type!");
		return 0;
	}

	static shaderc_shader_kind GLStageToShadercKind(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
		case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
		}
		WF_CORE_ASSERT(false); return (shaderc_shader_kind)0;
	}

	static const char* GLStageToString(uint32_t stage)
	{
		switch (stage)
		{
		case GL_VERTEX_SHADER:   return "vertex";
		case GL_FRAGMENT_SHADER: return "fragment";
		}
		return "unknown";
	}

	// -------------------------------------------------------------------------
	// Cache
	// -------------------------------------------------------------------------
	const char* VulkanShader::GetVulkanCacheDirectory()
	{
		return "assets/cache/shader/vulkan";
	}

	void VulkanShader::EnsureCacheDirectoryExists()
	{
		if (VFS::IsMounted())
			return;

		if (!std::filesystem::exists(GetVulkanCacheDirectory()))
			std::filesystem::create_directories(GetVulkanCacheDirectory());
	}

	const char* VulkanShader::StageToVulkanCacheExtension(uint32_t glStage)
	{
		switch (glStage)
		{
		case GL_VERTEX_SHADER:   return ".cached_vulkan.vert";
		case GL_FRAGMENT_SHADER: return ".cached_vulkan.frag";
		}
		return ".cached_vulkan";
	}

	// -------------------------------------------------------------------------
	// Constructors
	// -------------------------------------------------------------------------
	VulkanShader::VulkanShader(const std::string& filepath)
		: m_FilePath(filepath)
	{
		WF_PROFILE_FUNCTION();
		EnsureCacheDirectoryExists();

		auto source  = ReadFile(filepath);
		auto sources = PreProcess(source);

		{
			Timer t;
			CompileOrGetVulkanSPIRV(sources);
			WF_CORE_WARN("VulkanShader compilation took {0} ms", t.ElapsedMillis());
		}

		CreateShaderModules();
		ReflectAndCreateLayout();

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
		auto lastDot  = filepath.rfind('.');
		auto count    = (lastDot == std::string::npos) ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	VulkanShader::VulkanShader(const std::string& name,
		const std::string& vertexSrc,
		const std::string& fragmentSrc)
		: m_Name(name)
	{
		WF_PROFILE_FUNCTION();
		EnsureCacheDirectoryExists();

		std::unordered_map<uint32_t, std::string> sources;
		sources[GL_VERTEX_SHADER]   = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;

		CompileOrGetVulkanSPIRV(sources);
		CreateShaderModules();
		ReflectAndCreateLayout();
	}

	VulkanShader::~VulkanShader()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		vkDeviceWaitIdle(dev);

		for (auto& [key, pipeline] : m_Pipelines)
			vkDestroyPipeline(dev, pipeline, nullptr);
		m_Pipelines.clear();

		vkDestroyPipelineLayout(dev, m_PipelineLayout, nullptr);
		for (auto layout : m_DescriptorSetLayouts)
			vkDestroyDescriptorSetLayout(dev, layout, nullptr);

		if (m_VertModule) vkDestroyShaderModule(dev, m_VertModule, nullptr);
		if (m_FragModule) vkDestroyShaderModule(dev, m_FragModule, nullptr);
	}

	// -------------------------------------------------------------------------
	// Bind / Unbind
	// -------------------------------------------------------------------------
	void VulkanShader::Bind() const
	{
		VulkanContext::Get()->SetBoundShader(const_cast<VulkanShader*>(this));
	}

	void VulkanShader::Unbind() const
	{
		VulkanContext::Get()->SetBoundShader(nullptr);
	}

	// -------------------------------------------------------------------------
	// SetXxx - write into the push-constant staging buffer
	// -------------------------------------------------------------------------
	void VulkanShader::WritePushConstant(const std::string& name, const void* data, uint32_t size)
	{
		auto it = m_PushConstantMembers.find(name);
		if (it == m_PushConstantMembers.end())
		{
			// Fallback: write at the running end of push constant block
			// (handles unnamed / untracked members gracefully)
			WF_CORE_WARN("VulkanShader: push constant '{0}' not found in reflection data, skipping.", name);
			return;
		}
		uint32_t offset = it->second.offset;
		WF_CORE_ASSERT(offset + size <= m_TotalPushConstantSize, "Push constant overflow!");
		memcpy(m_PushConstantData.data() + offset, data, size);
	}

	void VulkanShader::SetInt(const std::string& name, int value)
	{ WritePushConstant(name, &value, sizeof(int)); }

	void VulkanShader::SetIntArray(const std::string& name, int* values, uint32_t count)
	{ WritePushConstant(name, values, count * sizeof(int)); }

	void VulkanShader::SetFloat(const std::string& name, float value)
	{ WritePushConstant(name, &value, sizeof(float)); }

	void VulkanShader::SetFloat2(const std::string& name, const glm::vec2 value)
	{ WritePushConstant(name, glm::value_ptr(value), sizeof(glm::vec2)); }

	void VulkanShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{ WritePushConstant(name, glm::value_ptr(value), sizeof(glm::vec3)); }

	void VulkanShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{ WritePushConstant(name, glm::value_ptr(value), sizeof(glm::vec4)); }

	void VulkanShader::SetMat3(const std::string& name, const glm::mat3& value)
	{ WritePushConstant(name, glm::value_ptr(value), sizeof(glm::mat3)); }

	void VulkanShader::SetMat4(const std::string& name, const glm::mat4& value)
	{ WritePushConstant(name, glm::value_ptr(value), sizeof(glm::mat4)); }

	// -------------------------------------------------------------------------
	// FlushPushConstants
	// -------------------------------------------------------------------------
	void VulkanShader::FlushPushConstants(VkCommandBuffer cmd) const
	{
		if (m_TotalPushConstantSize == 0 || m_PushConstantData.empty()) return;
		vkCmdPushConstants(cmd, m_PipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, m_TotalPushConstantSize,
			m_PushConstantData.data());
	}

	void VulkanShader::BindAndFlushDescriptors(VkCommandBuffer cmd, uint32_t frameIndex)
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();

		if (m_DescriptorSets.empty()) return;
		uint32_t f = frameIndex % m_DescriptorSets.size();

		for (size_t setIdx = 0; setIdx < m_DescriptorSets[f].size(); setIdx++)
		{
			VkDescriptorSet ds = m_DescriptorSets[f][setIdx];
			if (ds == VK_NULL_HANDLE) continue;

			std::vector<VkWriteDescriptorSet> writes;
			std::vector<VkDescriptorBufferInfo> bufferInfos;
			std::vector<std::vector<VkDescriptorImageInfo>> imageInfoArrays;

			for (const auto& d : m_ReflectedDescriptors)
			{
				if (d.Set != (uint32_t)setIdx) continue;

				if (d.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				{
					auto ubo = ctx->GetUniformBuffer(d.Binding);
					if (ubo.Buffer != VK_NULL_HANDLE)
					{
						VkDescriptorBufferInfo bInfo{};
						bInfo.buffer = ubo.Buffer;
						bInfo.offset = 0;
						bInfo.range  = ubo.Size;
						bufferInfos.push_back(bInfo);

						VkWriteDescriptorSet w{};
						w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						w.dstSet          = ds;
						w.dstBinding      = d.Binding;
						w.dstArrayElement = 0;
						w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						w.descriptorCount = 1;
						w.pBufferInfo     = &bufferInfos.back();
						writes.push_back(w);
					}
				}
				else if (d.Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					uint32_t count = d.Count;
					std::vector<VkDescriptorImageInfo> imgInfos(count);

					for (uint32_t slot = 0; slot < count; slot++)
					{
						auto tex = ctx->GetTexture(slot);
						if (tex.ImageView == VK_NULL_HANDLE || tex.Sampler == VK_NULL_HANDLE)
							tex = ctx->GetTexture(0);

						imgInfos[slot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imgInfos[slot].imageView   = tex.ImageView;
						imgInfos[slot].sampler     = tex.Sampler;
					}

					imageInfoArrays.push_back(imgInfos);

					VkWriteDescriptorSet w{};
					w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					w.dstSet          = ds;
					w.dstBinding      = d.Binding;
					w.dstArrayElement = 0;
					w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					w.descriptorCount = count;
					w.pImageInfo      = imageInfoArrays.back().data();
					writes.push_back(w);
				}
			}

			if (!writes.empty())
			{
				vkUpdateDescriptorSets(dev, (uint32_t)writes.size(), writes.data(), 0, nullptr);
			}

			vkCmdBindDescriptorSets(cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_PipelineLayout,
				(uint32_t)setIdx, 1, &ds,
				0, nullptr);
		}
	}

	// -------------------------------------------------------------------------
	// GetOrCreatePipeline - lazy pipeline cache
	// -------------------------------------------------------------------------
	VkPipeline VulkanShader::GetOrCreatePipeline(
		const VulkanVertexArray* vertexArray,
		VkPrimitiveTopology topology,
		const std::vector<VkFormat>& colorFormats,
		VkFormat depthFormat)
	{
		auto* ctx = VulkanContext::Get();

		std::vector<VkFormat> finalColorFormats = colorFormats;
		if (finalColorFormats.empty())
		{
			finalColorFormats = { ctx->GetSwapChainFormat() };
		}
		else
		{
			for (auto& fmt : finalColorFormats)
			{
				if (fmt == VK_FORMAT_UNDEFINED)
					fmt = ctx->GetSwapChainFormat();
			}
		}
		VkFormat finalDepthFormat = (depthFormat == VK_FORMAT_UNDEFINED) ? ctx->GetDepthFormat() : depthFormat;

		PipelineKey key;
		key.Bindings     = vertexArray ? vertexArray->GetBindingDescriptions()   : std::vector<VkVertexInputBindingDescription>();
		key.Attributes   = vertexArray ? vertexArray->GetAttributeDescriptions() : std::vector<VkVertexInputAttributeDescription>();
		key.Topology     = topology;
		key.ColorFormats = finalColorFormats;
		key.DepthFormat  = finalDepthFormat;

		auto it = m_Pipelines.find(key);
		if (it != m_Pipelines.end())
		{
			return it->second;
		}

		// ---------- Create the pipeline ----------
		VkDevice dev = ctx->GetDevice();

		// Shader stages
		VkPipelineShaderStageCreateInfo vertStage{};
		vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
		vertStage.module = m_VertModule;
		vertStage.pName  = "main";

		VkPipelineShaderStageCreateInfo fragStage{};
		fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.module = m_FragModule;
		fragStage.pName  = "main";

		VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

		// Vertex input
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount   = (uint32_t)key.Bindings.size();
		vertexInputInfo.pVertexBindingDescriptions      = key.Bindings.empty() ? nullptr : key.Bindings.data();
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)key.Attributes.size();
		vertexInputInfo.pVertexAttributeDescriptions    = key.Attributes.empty() ? nullptr : key.Attributes.data();

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology               = topology;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Viewport / scissor (dynamic)
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount  = 1;

		// Rasteriser
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable        = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth               = 1.0f;
		rasterizer.cullMode                = VK_CULL_MODE_NONE;
		rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable         = VK_FALSE;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable  = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Depth / stencil
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable       = VK_TRUE;
		depthStencil.depthWriteEnable      = VK_TRUE;
		depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable     = VK_FALSE;

		// Color blend (alpha blending) for each color attachment
		std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(finalColorFormats.size());
		for (size_t i = 0; i < finalColorFormats.size(); i++)
		{
			blendAttachments[i].colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			bool isIntegerFormat = (finalColorFormats[i] == VK_FORMAT_R32_SINT ||
			                        finalColorFormats[i] == VK_FORMAT_R32_UINT ||
			                        finalColorFormats[i] == VK_FORMAT_R8_SINT ||
			                        finalColorFormats[i] == VK_FORMAT_R8_UINT ||
			                        finalColorFormats[i] == VK_FORMAT_R16_SINT ||
			                        finalColorFormats[i] == VK_FORMAT_R16_UINT);

			blendAttachments[i].blendEnable         = isIntegerFormat ? VK_FALSE : VK_TRUE;
			blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachments[i].colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blendAttachments[i].alphaBlendOp        = VK_BLEND_OP_ADD;
		}

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable     = VK_FALSE;
		colorBlending.attachmentCount   = (uint32_t)blendAttachments.size();
		colorBlending.pAttachments      = blendAttachments.data();

		// Dynamic states (viewport, scissor, line width)
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates    = dynamicStates.data();

		// Dynamic rendering (Vulkan 1.3)
		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount    = (uint32_t)finalColorFormats.size();
		renderingInfo.pColorAttachmentFormats = finalColorFormats.data();
		renderingInfo.depthAttachmentFormat   = finalDepthFormat;

		// Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext               = &renderingInfo;
		pipelineInfo.stageCount          = 2;
		pipelineInfo.pStages             = stages;
		pipelineInfo.pVertexInputState   = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState      = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState   = &multisampling;
		pipelineInfo.pDepthStencilState  = &depthStencil;
		pipelineInfo.pColorBlendState    = &colorBlending;
		pipelineInfo.pDynamicState       = &dynamicState;
		pipelineInfo.layout              = m_PipelineLayout;
		pipelineInfo.renderPass          = VK_NULL_HANDLE; // dynamic rendering

		VkPipeline pipeline = VK_NULL_HANDLE;
		VkResult pipeRes = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
		WF_CORE_ASSERT(pipeRes == VK_SUCCESS, "Failed to create Vulkan graphics pipeline!");

		m_Pipelines[key] = pipeline;
		return pipeline;
	}

	// -------------------------------------------------------------------------
	// Descriptor set layout accessor
	// -------------------------------------------------------------------------
	VkDescriptorSetLayout VulkanShader::GetDescriptorSetLayout(uint32_t set) const
	{
		if (set < m_DescriptorSetLayouts.size())
			return m_DescriptorSetLayouts[set];
		return VK_NULL_HANDLE;
	}

	// =========================================================================
	// Private implementation
	// =========================================================================

	std::string VulkanShader::ReadFile(const std::string& filepath)
	{
		std::string result = VFS::ReadFileAsString(filepath);
		if (result.empty())
		{
			WF_CORE_ERROR("Could not open shader file '{0}'", filepath);
			WF_CORE_ASSERT(false, "Failed to read shader file.");
		}
		return result;
	}

	std::unordered_map<uint32_t, std::string> VulkanShader::PreProcess(const std::string& source)
	{
		std::unordered_map<uint32_t, std::string> shaderSources;
		const char* typeToken = "#type";
		size_t typeTokenLen = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol  = source.find_first_of("\r\n", pos);
			size_t begin = pos + typeTokenLen + 1;
			std::string type = source.substr(begin, eol - begin);
			uint32_t glStage = ShaderTypeFromString(type);

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			pos = source.find(typeToken, nextLinePos);
			shaderSources[glStage] = source.substr(nextLinePos,
				pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
		}
		return shaderSources;
	}

	void VulkanShader::CompileOrGetVulkanSPIRV(const std::unordered_map<uint32_t, std::string>& sources)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		options.SetTargetSpirv(shaderc_spirv_version_1_6);
		options.SetOptimizationLevel(shaderc_optimization_level_performance);

		std::filesystem::path cacheDir = GetVulkanCacheDirectory();

		for (auto& [stage, src] : sources)
		{
			std::filesystem::path filePath  = m_FilePath;
			std::filesystem::path cachePath = cacheDir / (filePath.filename().string() + StageToVulkanCacheExtension(stage));

			bool cacheValid = false;
			if (VFS::Exists(cachePath))
			{
				if (std::filesystem::exists(cachePath) && std::filesystem::exists(filePath))
				{
					if (std::filesystem::last_write_time(cachePath) >= std::filesystem::last_write_time(filePath))
						cacheValid = true;
				}
				else
				{
					cacheValid = true;
				}
			}

			if (cacheValid)
			{
				Buffer cacheBuffer = VFS::ReadFile(cachePath);
				if (cacheBuffer && cacheBuffer.Size > 0)
				{
					auto& data = m_SPIRV[stage];
					data.resize(cacheBuffer.Size / sizeof(uint32_t));
					memcpy(data.data(), cacheBuffer.Data, cacheBuffer.Size);
					continue;
				}
			}

			// Compile
			auto module = compiler.CompileGlslToSpv(src, GLStageToShadercKind(stage),
				m_FilePath.c_str(), options);
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				WF_CORE_ERROR(module.GetErrorMessage());
				WF_CORE_ASSERT(false, "Vulkan SPIR-V compilation failed!");
			}
			m_SPIRV[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

			// Write cache
			if (!VFS::IsMounted())
			{
				EnsureCacheDirectoryExists();
				std::ofstream out(cachePath, std::ios::binary);
				if (out.is_open())
				{
					auto& data = m_SPIRV[stage];
					out.write((char*)data.data(), data.size() * sizeof(uint32_t));
				}
			}
		}
	}

	void VulkanShader::CreateShaderModules()
	{
		VkDevice dev = VulkanContext::Get()->GetDevice();

		auto createModule = [&](uint32_t stage) -> VkShaderModule
		{
			auto it = m_SPIRV.find(stage);
			if (it == m_SPIRV.end()) return VK_NULL_HANDLE;

			VkShaderModuleCreateInfo info{};
			info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			info.codeSize = it->second.size() * sizeof(uint32_t);
			info.pCode    = it->second.data();

			VkShaderModule mod = VK_NULL_HANDLE;
			VkResult modRes = vkCreateShaderModule(dev, &info, nullptr, &mod);
			WF_CORE_ASSERT(modRes == VK_SUCCESS, "Failed to create shader module!");
			return mod;
		};

		m_VertModule = createModule(GL_VERTEX_SHADER);
		m_FragModule = createModule(GL_FRAGMENT_SHADER);
	}

	void VulkanShader::Reflect(uint32_t stage, const std::vector<uint32_t>& spirv)
	{
		try
		{
			spirv_cross::Compiler compiler(spirv);
			auto resources = compiler.get_shader_resources();

			// Push constants
			for (const auto& pc : resources.push_constant_buffers)
			{
				const auto& type = compiler.get_type(pc.base_type_id);
				uint32_t memberCount = (uint32_t)type.member_types.size();

				for (uint32_t i = 0; i < memberCount; i++)
				{
					std::string name = compiler.get_member_name(pc.base_type_id, i);
					uint32_t offset  = (uint32_t)compiler.type_struct_member_offset(type, i);
					uint32_t size    = (uint32_t)compiler.get_declared_struct_member_size(type, i);

					m_PushConstantMembers[name] = { offset, size };
					m_TotalPushConstantSize = std::max(m_TotalPushConstantSize, offset + size);
				}
			}

			WF_CORE_TRACE("VulkanShader::Reflect - stage={0} file={1}", GLStageToString(stage), m_FilePath);
			WF_CORE_TRACE("  Push constant total size: {0} bytes", m_TotalPushConstantSize);
		}
		catch (const spirv_cross::CompilerError& e)
		{
			WF_CORE_ERROR("SPIRV-Cross error: {0}", e.what());
		}
	}

	void VulkanShader::ReflectAndCreateLayout()
	{
		// Reflect each stage
		for (auto& [stage, spirv] : m_SPIRV)
			Reflect(stage, spirv);

		// Ensure push constant data buffer is sized
		if (m_TotalPushConstantSize > 0)
			m_PushConstantData.resize(m_TotalPushConstantSize, 0);

		// ---------- Descriptor set layouts (UBOs, samplers) ----------
		// Reflect from the vertex SPIR-V (could extend to merge all stages)
		// For now create one descriptor set layout per set used (max set index + 1)
		uint32_t maxSet = 0;
		struct DescriptorInfo { uint32_t set; uint32_t binding; VkDescriptorType type; VkShaderStageFlags stages; uint32_t count; };
		std::map<std::pair<uint32_t, uint32_t>, DescriptorInfo> mergedDescriptors;

		for (auto& [stage, spirv] : m_SPIRV)
		{
			VkShaderStageFlags vkStage = (stage == GL_VERTEX_SHADER)
				? VK_SHADER_STAGE_VERTEX_BIT
				: VK_SHADER_STAGE_FRAGMENT_BIT;

			spirv_cross::Compiler comp(spirv);
			auto resources = comp.get_shader_resources();

			for (auto& res : resources.uniform_buffers)
			{
				uint32_t set     = comp.get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t binding = comp.get_decoration(res.id, spv::DecorationBinding);
				const auto& type = comp.get_type(res.type_id);
				uint32_t count   = type.array.empty() ? 1 : type.array[0];
				VkShaderStageFlags uboStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

				auto key = std::make_pair(set, binding);
				if (mergedDescriptors.find(key) != mergedDescriptors.end())
				{
					mergedDescriptors[key].stages |= uboStages;
					mergedDescriptors[key].count = std::max(mergedDescriptors[key].count, count);
				}
				else
				{
					mergedDescriptors[key] = { set, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboStages, count };
				}
				maxSet = std::max(maxSet, set);
			}
			for (auto& res : resources.sampled_images)
			{
				uint32_t set     = comp.get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t binding = comp.get_decoration(res.id, spv::DecorationBinding);
				const auto& type = comp.get_type(res.type_id);
				uint32_t count   = type.array.empty() ? 1 : type.array[0];

				auto key = std::make_pair(set, binding);
				if (mergedDescriptors.find(key) != mergedDescriptors.end())
				{
					mergedDescriptors[key].stages |= vkStage;
					mergedDescriptors[key].count = std::max(mergedDescriptors[key].count, count);
				}
				else
				{
					mergedDescriptors[key] = { set, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vkStage, count };
				}
				maxSet = std::max(maxSet, set);
			}
		}

		VkDevice dev = VulkanContext::Get()->GetDevice();
		m_DescriptorSetLayouts.resize(maxSet + 1, VK_NULL_HANDLE);

		for (uint32_t setIdx = 0; setIdx <= maxSet; setIdx++)
		{
			std::vector<VkDescriptorSetLayoutBinding> bindings;
			for (auto& [key, d] : mergedDescriptors)
			{
				if (d.set != setIdx) continue;
				VkDescriptorSetLayoutBinding b{};
				b.binding            = d.binding;
				b.descriptorType     = d.type;
				b.descriptorCount    = d.count;
				b.stageFlags         = d.stages;
				bindings.push_back(b);
			}

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = (uint32_t)bindings.size();
			layoutInfo.pBindings    = bindings.empty() ? nullptr : bindings.data();

			VkResult dslRes = vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &m_DescriptorSetLayouts[setIdx]);
			WF_CORE_ASSERT(dslRes == VK_SUCCESS, "Failed to create descriptor set layout!");
		}

		// Store reflected descriptors for updating sets before draw calls
		m_ReflectedDescriptors.clear();
		for (auto& [key, d] : mergedDescriptors)
		{
			m_ReflectedDescriptors.push_back({ d.set, d.binding, d.type, d.count });
		}

		// Allocate per-frame descriptor sets from shared descriptor pool
		uint32_t framesInFlight = VulkanContext::Get()->GetFramesInFlight();
		m_DescriptorSets.resize(framesInFlight);
		for (uint32_t f = 0; f < framesInFlight; f++)
		{
			m_DescriptorSets[f].resize(m_DescriptorSetLayouts.size(), VK_NULL_HANDLE);
			for (size_t s = 0; s < m_DescriptorSetLayouts.size(); s++)
			{
				if (m_DescriptorSetLayouts[s] == VK_NULL_HANDLE) continue;

				VkDescriptorSetAllocateInfo allocInfo{};
				allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorPool     = VulkanContext::Get()->GetDescriptorPool();
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts        = &m_DescriptorSetLayouts[s];

				VkResult allocRes = vkAllocateDescriptorSets(dev, &allocInfo, &m_DescriptorSets[f][s]);
				WF_CORE_ASSERT(allocRes == VK_SUCCESS, "Failed to allocate descriptor set for shader!");
			}
		}

		// ---------- Push constant range ----------
		VkPushConstantRange pcRange{};
		pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pcRange.offset     = 0;
		pcRange.size       = m_TotalPushConstantSize > 0 ? m_TotalPushConstantSize : 4; // min 4 bytes

		// ---------- Pipeline layout ----------
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount         = (uint32_t)m_DescriptorSetLayouts.size();
		pipelineLayoutInfo.pSetLayouts            = m_DescriptorSetLayouts.empty() ? nullptr : m_DescriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges    = &pcRange;

		VkResult plRes = vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);
		WF_CORE_ASSERT(plRes == VK_SUCCESS, "Failed to create pipeline layout!");
	}

	// -------------------------------------------------------------------------
	// Pipeline key hash / comparison
	// -------------------------------------------------------------------------
	static bool EqualBindings(const std::vector<VkVertexInputBindingDescription>& a,
		const std::vector<VkVertexInputBindingDescription>& b)
	{
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); i++)
		{
			if (a[i].binding != b[i].binding || a[i].stride != b[i].stride || a[i].inputRate != b[i].inputRate)
				return false;
		}
		return true;
	}

	static bool EqualAttributes(const std::vector<VkVertexInputAttributeDescription>& a,
		const std::vector<VkVertexInputAttributeDescription>& b)
	{
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); i++)
		{
			if (a[i].location != b[i].location || a[i].binding != b[i].binding ||
				a[i].format != b[i].format || a[i].offset != b[i].offset)
				return false;
		}
		return true;
	}

	bool VulkanShader::PipelineKey::operator==(const PipelineKey& o) const
	{
		return Topology == o.Topology
			&& ColorFormats == o.ColorFormats
			&& DepthFormat == o.DepthFormat
			&& EqualBindings(Bindings, o.Bindings)
			&& EqualAttributes(Attributes, o.Attributes);
	}

	size_t VulkanShader::PipelineKeyHash::operator()(const PipelineKey& k) const
	{
		size_t seed = std::hash<int>{}((int)k.Topology)
			^ (std::hash<int>{}((int)k.DepthFormat) << 2);

		for (auto f : k.ColorFormats)
			seed ^= std::hash<int>{}((int)f) << 1;

		for (auto& b : k.Bindings)
			seed ^= std::hash<uint32_t>{}(b.stride) ^ (std::hash<uint32_t>{}(b.binding) << 4);
		for (auto& a : k.Attributes)
			seed ^= std::hash<uint32_t>{}(a.location) ^ (std::hash<uint32_t>{}(a.format) << 4)
				   ^ (std::hash<uint32_t>{}(a.offset) << 8);
		return seed;
	}

}
