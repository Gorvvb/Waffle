#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

namespace Waffle {

	class VulkanUtils
	{
	public:
		// Find a device memory type satisfying both the type filter and property flags.
		static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
			{
				if ((typeFilter & (1 << i)) &&
					(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}

			WF_CORE_ASSERT(false, "Failed to find suitable memory type!");
			return 0;
		}

		// Find the first format in 'candidates' that supports the given tiling / features.
		static VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice,
			const std::vector<VkFormat>& candidates,
			VkImageTiling tiling,
			VkFormatFeatureFlags features)
		{
			for (VkFormat format : candidates)
			{
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

				if (tiling == VK_IMAGE_TILING_LINEAR  && (props.linearTilingFeatures  & features) == features) return format;
				if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
			}

			WF_CORE_ASSERT(false, "Failed to find supported format!");
			return VK_FORMAT_UNDEFINED;
		}

		// Create a VkBuffer with backing device memory.
		static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
			VkBuffer& outBuffer, VkDeviceMemory& outMemory)
		{
			outBuffer = VK_NULL_HANDLE;
			outMemory = VK_NULL_HANDLE;
			if (size == 0) size = 1;

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size        = size;
			bufferInfo.usage       = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkResult res = vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer);
			if (res != VK_SUCCESS || outBuffer == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vkCreateBuffer failed with error code: {0}", (int)res);
				WF_CORE_ASSERT(false, "Failed to create buffer!");
				return;
			}

			VkMemoryRequirements memReqs;
			vkGetBufferMemoryRequirements(device, outBuffer, &memReqs);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize  = memReqs.size;
			allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

			res = vkAllocateMemory(device, &allocInfo, nullptr, &outMemory);
			if (res != VK_SUCCESS || outMemory == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vkAllocateMemory for buffer failed with error code: {0}", (int)res);
				WF_CORE_ASSERT(false, "Failed to allocate buffer memory!");
				return;
			}

			vkBindBufferMemory(device, outBuffer, outMemory, 0);
		}

		// Create a VkImage with backing device memory.
		static void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice,
			uint32_t width, uint32_t height,
			VkFormat format, VkImageTiling tiling,
			VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			VkImage& outImage, VkDeviceMemory& outMemory)
		{
			outImage = VK_NULL_HANDLE;
			outMemory = VK_NULL_HANDLE;

			width  = std::max(1u, width);
			height = std::max(1u, height);

			VkImageCreateInfo imageInfo{};
			imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType     = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width  = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth  = 1;
			imageInfo.mipLevels     = 1;
			imageInfo.arrayLayers   = 1;
			imageInfo.format        = format;
			imageInfo.tiling        = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage         = usage;
			imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

			VkResult res = vkCreateImage(device, &imageInfo, nullptr, &outImage);
			if (res != VK_SUCCESS || outImage == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vkCreateImage failed with error code: {0} (format={1}, {2}x{3})", (int)res, (int)format, width, height);
				WF_CORE_ASSERT(false, "Failed to create image!");
				return;
			}

			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, outImage, &memReqs);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize  = memReqs.size;
			allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

			res = vkAllocateMemory(device, &allocInfo, nullptr, &outMemory);
			if (res != VK_SUCCESS || outMemory == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vkAllocateMemory for image failed with error code: {0}", (int)res);
				WF_CORE_ASSERT(false, "Failed to allocate image memory!");
				return;
			}

			vkBindImageMemory(device, outImage, outMemory, 0);
		}

		// Create a VkImageView.
		static VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format,
			VkImageAspectFlags aspectFlags)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image                           = image;
			viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format                          = format;
			viewInfo.subresourceRange.aspectMask     = aspectFlags;
			viewInfo.subresourceRange.baseMipLevel   = 0;
			viewInfo.subresourceRange.levelCount     = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount     = 1;

			VkImageView imageView = VK_NULL_HANDLE;
			VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
			WF_CORE_ASSERT(res == VK_SUCCESS, "Failed to create image view!");
			return imageView;
		}

		// Record a pipeline barrier to transition image layout.
		static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			VkAccessFlags srcAccess, VkAccessFlags dstAccess,
			VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout                       = oldLayout;
			barrier.newLayout                       = newLayout;
			barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
			barrier.image                           = image;
			barrier.subresourceRange.aspectMask     = aspectMask;
			barrier.subresourceRange.baseMipLevel   = 0;
			barrier.subresourceRange.levelCount     = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount     = 1;
			barrier.srcAccessMask                   = srcAccess;
			barrier.dstAccessMask                   = dstAccess;

			vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		// Check if a format has a depth component.
		static bool HasDepthComponent(VkFormat format)
		{
			return format == VK_FORMAT_D16_UNORM
				|| format == VK_FORMAT_D32_SFLOAT
				|| format == VK_FORMAT_D24_UNORM_S8_UINT
				|| format == VK_FORMAT_D32_SFLOAT_S8_UINT;
		}

		// Check if a format has a stencil component.
		static bool HasStencilComponent(VkFormat format)
		{
			return format == VK_FORMAT_D24_UNORM_S8_UINT
				|| format == VK_FORMAT_D32_SFLOAT_S8_UINT;
		}

		// VK_CHECK macro helper
		static void CheckResult(VkResult result, const char* msg)
		{
			if (result != VK_SUCCESS)
			{
				WF_CORE_ERROR("Vulkan error ({0}): {1}", (int)result, msg);
				WF_CORE_ASSERT(false, msg);
			}
		}
	};

} // namespace Waffle
