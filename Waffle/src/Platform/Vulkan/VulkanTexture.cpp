#include "wfpch.h"
#include "VulkanTexture.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#include "Waffle/Core/VFS.h"
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

		std::filesystem::path resolvedPath = ResolveTexturePath(path);

		stbi_set_flip_vertically_on_load(1);
		int width = 0, height = 0, channels = 0;
		stbi_uc* data = nullptr;

		Buffer fileBuffer = VFS::ReadFile(resolvedPath);
		if (!fileBuffer && resolvedPath != path)
			fileBuffer = VFS::ReadFile(path);

		if (fileBuffer)
		{
			data = stbi_load_from_memory(fileBuffer.Data, static_cast<int>(fileBuffer.Size), &width, &height, &channels, STBI_rgb_alpha);
		}

		if (!data)
		{
			WF_CORE_WARN("Texture file not found: '{0}' (Resolved: '{1}'). Using fallback white texture.", path, resolvedPath.string());

			m_Width = 1;
			m_Height = 1;
			m_Channels = 4;

			std::vector<uint8_t> fallbackData(4, 255);
			CreateTextureFromData(fallbackData.data(), 1, 1, 4);
			CreateSampler(filter);
			CreateImGuiDescriptorSet();
			return;
		}

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
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		// Drop our registrations BEFORE destroying the handles - otherwise the
		// context texture registry hands a dead view/sampler to descriptors.
		ctx->UnregisterTexture(m_ImageView, m_Sampler);

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
		m_SlotDescriptorSets.clear();

		if (m_Sampler != VK_NULL_HANDLE)
			vkDestroySampler(dev, m_Sampler, nullptr);
		if (m_ImageView != VK_NULL_HANDLE)
			vkDestroyImageView(dev, m_ImageView, nullptr);

		if (m_Image != VK_NULL_HANDLE)
			vmaDestroyImage(ctx->GetVmaAllocator(), m_Image, m_Allocation);

		m_Image = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}

	// =========================================================================
	// SetData
	// =========================================================================
	void VulkanTexture2D::SetData(void* data, uint32_t size)
	{
		WF_PROFILE_FUNCTION();
		WF_CORE_ASSERT(size == m_Width * m_Height * m_Channels, "Data must be entire texture!");

		auto* ctx = VulkanContext::Get();

		// In-flight frames may still be sampling this image in
		// SHADER_READ_ONLY_OPTIMAL; wait before re-transitioning the layout
		// (the single-time submits below have no dependency on those frames).
		VkDevice dev = ctx->GetDevice();
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		VmaAllocator allocator = ctx->GetVmaAllocator();

		VkBuffer      stagingBuffer;
		VmaAllocation stagingAllocation;
		VulkanUtils::CreateBuffer(allocator,
			size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			stagingBuffer, stagingAllocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		VmaAllocationInfo stagingAllocInfo{};
		vmaGetAllocationInfo(allocator, stagingAllocation, &stagingAllocInfo);
		memcpy(stagingAllocInfo.pMappedData, data, size);

		// Transition → transfer dst, copy, transition → shader read
		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		ctx->CopyBufferToImage(stagingBuffer, m_Image, m_Width, m_Height);

		cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
	}

	// =========================================================================
	// Bind
	// =========================================================================
	void VulkanTexture2D::Bind(uint32_t slot) const
	{
		WF_PROFILE_FUNCTION();
		VulkanContext::Get()->RegisterTexture(slot, m_ImageView, m_Sampler);
	}

	void VulkanTexture2D::SetFilter(TextureFilter filter)
	{
		// Idempotence guard: this path stalls the whole device
		// (vkDeviceWaitIdle + sampler rebuild) and is invoked per frame by
		// some UI code re-applying the same filter.
		if (filter == m_CurrentFilter && m_Sampler != VK_NULL_HANDLE)
			return;
		m_CurrentFilter = filter;

		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		// The sampler may be referenced by in-flight descriptor sets and the
		// ImGui set - wait, then rebuild every reference against the new one.
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		VkImageView oldView = m_ImageView;
		VkSampler oldSampler = m_Sampler;

		if (m_ImGuiDescriptorSet)
		{
			if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
				ImGui_ImplVulkan_RemoveTexture(m_ImGuiDescriptorSet);
			m_ImGuiDescriptorSet = VK_NULL_HANDLE;
		}

		for (auto& [slot, ds] : m_SlotDescriptorSets)
		{
			if (ds != VK_NULL_HANDLE)
				ctx->SafeFreeDescriptorSet(ds);
		}
		m_SlotDescriptorSets.clear();

		if (m_Sampler)
			vkDestroySampler(dev, m_Sampler, nullptr);
		m_Sampler = VK_NULL_HANDLE;

		CreateSampler(filter);
		CreateImGuiDescriptorSet();

		// Re-register with the context under the new sampler.
		ctx->UnregisterTexture(oldView, oldSampler);
	}

	bool VulkanTexture2D::operator==(const Texture& other) const
	{
		// Pointer form: a reference dynamic_cast throws std::bad_cast when
		// `other` is a different Texture implementation.
		const VulkanTexture2D* otherVK = dynamic_cast<const VulkanTexture2D*>(&other);
		return otherVK && m_Image == otherVK->m_Image;
	}

	// =========================================================================
	// Private helpers
	// =========================================================================

	void VulkanTexture2D::CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint32_t channels)
	{
		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();
		VmaAllocator allocator = ctx->GetVmaAllocator();

		m_Format = (channels == 4) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM;
		VkDeviceSize imageSize = width * height * channels;

		// Upload via staging buffer using VMA
		VkBuffer      stagingBuffer;
		VmaAllocation stagingAllocation;
		VulkanUtils::CreateBuffer(allocator,
			imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			stagingBuffer, stagingAllocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		VmaAllocationInfo stagingAllocInfo{};
		vmaGetAllocationInfo(allocator, stagingAllocation, &stagingAllocInfo);
		memcpy(stagingAllocInfo.pMappedData, data, static_cast<size_t>(imageSize));

		// Create image using VMA
		VulkanUtils::CreateImage(allocator,
			width, height,
			m_Format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_Image, m_Allocation);

		// Transition → transfer dst
		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		ctx->CopyBufferToImage(stagingBuffer, m_Image, width, height);

		// Transition → shader read
		cmd = ctx->BeginSingleTimeCommands();
		VulkanUtils::TransitionImageLayout(cmd, m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
		ctx->EndSingleTimeCommands(cmd);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

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

		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = vkFilter,
			.minFilter = vkFilter,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = props.limits.maxSamplerAnisotropy,
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS,
			.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE
		};

		VkResult res = vkCreateSampler(dev, &samplerInfo, nullptr, &m_Sampler);
		WF_CORE_ASSERT(res == VK_SUCCESS, "Failed to create texture sampler!");
		m_CurrentFilter = filter;
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
