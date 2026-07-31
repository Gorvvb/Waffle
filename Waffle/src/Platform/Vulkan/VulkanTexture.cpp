#include "wfpch.h"
#include "VulkanTexture.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#include "stb_image.h"

// ImGui Vulkan backend (for AddTexture)
#include "backends/imgui_impl_vulkan.h"

namespace Waffle {

	// =========================================================================
	// Constructors
	// =========================================================================

	VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height, TextureFilter filter)
		: m_Width(width), m_Height(height), m_Channels(4)
	{
		WF_PROFILE_FUNCTION();

		std::vector<uint8_t> data(width * height * 4, 255);
		CreateTextureFromData(data.data(), width, height, 4);
		CreateSampler(filter);
		CreateImGuiDescriptorSet();
	}

	VulkanTexture2D::VulkanTexture2D(const std::string& path, TextureFilter filter)
		: m_Path(path)
	{
		WF_PROFILE_FUNCTION();

		stbi_set_flip_vertically_on_load(1);
		int width, height, channels;
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		WF_CORE_ASSERT(data, "Failed to load texture!");

		m_Width    = width;
		m_Height   = height;
		m_Channels = 4; // always RGBA from stbi_rgb_alpha

		CreateTextureFromData(data, width, height, 4);
		stbi_image_free(data);

		CreateSampler(filter);
		CreateImGuiDescriptorSet();
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		vkDeviceWaitIdle(dev);

		// Free ImGui descriptor set if allocated
		if (m_ImGuiDescriptorSet)
		{
			if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
				ImGui_ImplVulkan_RemoveTexture(m_ImGuiDescriptorSet);
			m_ImGuiDescriptorSet = VK_NULL_HANDLE;
		}

		// Free slot descriptor sets (they were allocated from the shared pool)
		for (auto& [slot, ds] : m_SlotDescriptorSets)
		{
			if (ds != VK_NULL_HANDLE)
				ctx->SafeFreeDescriptorSet(ds);
		}

		vkDestroySampler(dev, m_Sampler, nullptr);
		vkDestroyImageView(dev, m_ImageView, nullptr);
		vkDestroyImage(dev, m_Image, nullptr);
		vkFreeMemory(dev, m_Memory, nullptr);
	}

	// =========================================================================
	// SetData
	// =========================================================================
	void VulkanTexture2D::SetData(void* data, uint32_t size)
	{
		WF_PROFILE_FUNCTION();
		WF_CORE_ASSERT(size == m_Width * m_Height * m_Channels, "Data must be entire texture!");

		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingMemory;
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory);

		void* mapped;
		vkMapMemory(dev, stagingMemory, 0, size, 0, &mapped);
		memcpy(mapped, data, size);
		vkUnmapMemory(dev, stagingMemory);

		// Transition → transfer dst, copy, transition → shader read
		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		ctx->CopyBufferToImage(stagingBuffer, m_Image, m_Width, m_Height);

		cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		vkDestroyBuffer(dev, stagingBuffer, nullptr);
		vkFreeMemory(dev, stagingMemory, nullptr);
	}

	// =========================================================================
	// Bind
	// =========================================================================
	void VulkanTexture2D::Bind(uint32_t slot) const
	{
		WF_PROFILE_FUNCTION();

		// Ensure we have a descriptor set for this slot
		auto it = m_SlotDescriptorSets.find(slot);
		if (it == m_SlotDescriptorSets.end())
		{
			auto* ctx = VulkanContext::Get();
			VkDevice dev = ctx->GetDevice();

			// Create a combined-image-sampler descriptor set for this slot.
			// We use a simple 1-binding layout each time.
			VkDescriptorSetLayoutBinding binding{};
			binding.binding            = slot;
			binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.descriptorCount    = 1;
			binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings    = &binding;

			VkDescriptorSetLayout layout;
			vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &layout);

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool     = ctx->GetDescriptorPool();
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts        = &layout;

			VkDescriptorSet ds = VK_NULL_HANDLE;
			VkResult res = vkAllocateDescriptorSets(dev, &allocInfo, &ds);
			vkDestroyDescriptorSetLayout(dev, layout, nullptr);

			if (res != VK_SUCCESS || ds == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vkAllocateDescriptorSets in VulkanTexture2D::Bind failed with error code: {0}", (int)res);
				return;
			}

			// Write image info
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView   = m_ImageView;
			imageInfo.sampler     = m_Sampler;

			VkWriteDescriptorSet write{};
			write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet          = ds;
			write.dstBinding      = slot;
			write.dstArrayElement = 0;
			write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.pImageInfo      = &imageInfo;

			vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
			m_SlotDescriptorSets[slot] = ds;
			it = m_SlotDescriptorSets.find(slot);
		}

		// Register with the context so RendererAPI can bind it before the next draw
		VulkanContext::Get()->BindDescriptorSet(1 + slot, it->second);
		VulkanContext::Get()->RegisterTexture(slot, m_ImageView, m_Sampler);
	}

	bool VulkanTexture2D::operator==(const Texture& other) const
	{
		return m_Image == dynamic_cast<const VulkanTexture2D&>(other).m_Image;
	}

	// =========================================================================
	// Private helpers
	// =========================================================================

	void VulkanTexture2D::CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint32_t channels)
	{
		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		m_Format = (channels == 4) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8_SRGB;
		VkDeviceSize imageSize = width * height * channels;

		// Upload via staging buffer
		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingMemory;
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory);

		void* mapped;
		vkMapMemory(dev, stagingMemory, 0, imageSize, 0, &mapped);
		memcpy(mapped, data, (size_t)imageSize);
		vkUnmapMemory(dev, stagingMemory);

		// Create image
		VulkanUtils::CreateImage(dev, ctx->GetPhysicalDevice(),
			width, height,
			m_Format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_Memory);

		// Transition → transfer dst
		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		ctx->CopyBufferToImage(stagingBuffer, m_Image, width, height);

		// Transition → shader read
		cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		vkDestroyBuffer(dev, stagingBuffer, nullptr);
		vkFreeMemory(dev, stagingMemory, nullptr);

		// Create image view
		m_ImageView = VulkanUtils::CreateImageView(dev, m_Image, m_Format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void VulkanTexture2D::CreateSampler(TextureFilter filter)
	{
		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(ctx->GetPhysicalDevice(), &props);

		VkFilter vkFilter = (filter == TextureFilter::Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter               = vkFilter;
		samplerInfo.minFilter               = vkFilter;
		samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable        = VK_TRUE;
		samplerInfo.maxAnisotropy           = props.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable           = VK_FALSE;
		samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		VkResult res = vkCreateSampler(dev, &samplerInfo, nullptr, &m_Sampler);
		WF_CORE_ASSERT(res == VK_SUCCESS, "Failed to create texture sampler!");
	}

	uint64_t VulkanTexture2D::GetRendererID() const
	{
		if (!m_ImGuiDescriptorSet)
		{
			if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
			{
				const_cast<VulkanTexture2D*>(this)->m_ImGuiDescriptorSet =
					ImGui_ImplVulkan_AddTexture(m_Sampler, m_ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}
		return (uint64_t)m_ImGuiDescriptorSet;
	}

	void VulkanTexture2D::CreateImGuiDescriptorSet()
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
		{
			m_ImGuiDescriptorSet = ImGui_ImplVulkan_AddTexture(
				m_Sampler, m_ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

} // namespace Waffle
