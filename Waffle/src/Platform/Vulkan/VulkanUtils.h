#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <stdexcept>
#include <algorithm>

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

		// Create a VkBuffer backed by VMA allocation
		static void CreateBuffer(VmaAllocator allocator,
			VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
			VkBuffer& outBuffer, VmaAllocation& outAllocation, VmaAllocationCreateFlags flags = 0)
		{
			outBuffer = VK_NULL_HANDLE;
			outAllocation = VK_NULL_HANDLE;
			if (size == 0) size = 1;

			VkBufferCreateInfo bufferInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = size,
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};

			VmaAllocationCreateInfo allocCreateInfo
			{
				.flags = flags,
				.usage = memoryUsage
			};

			VkResult res = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &outBuffer, &outAllocation, nullptr);
			if (res != VK_SUCCESS || outBuffer == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vmaCreateBuffer failed with error code: {0}", (int)res);
				WF_CORE_ASSERT(false, "Failed to create VMA buffer!");
			}
		}

		// Create a VkImage backed by VMA allocation
		static void CreateImage(VmaAllocator allocator,
			uint32_t width, uint32_t height,
			VkFormat format, VkImageTiling tiling,
			VkImageUsageFlags usage, VmaMemoryUsage memoryUsage,
			VkImage& outImage, VmaAllocation& outAllocation, VmaAllocationCreateFlags flags = 0)
		{
			outImage = VK_NULL_HANDLE;
			outAllocation = VK_NULL_HANDLE;

			width = std::max(1u, width);
			height = std::max(1u, height);

			VkImageCreateInfo imageInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent{.width = width, .height = height, .depth = 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = tiling,
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};

			VmaAllocationCreateInfo allocCreateInfo
			{
				.flags = flags,
				.usage = memoryUsage
			};

			VkResult res = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &outImage, &outAllocation, nullptr);
			if (res != VK_SUCCESS || outImage == VK_NULL_HANDLE)
			{
				WF_CORE_ERROR("vmaCreateImage failed with error code: {0} (format={1}, {2}x{3})", (int)res, (int)format, width, height);
				WF_CORE_ASSERT(false, "Failed to create VMA image!");
			}
		}

		// Create a VkImageView using C++20 designated initializers
		static VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format,
			VkImageAspectFlags aspectFlags)
		{
			VkImageViewCreateInfo viewInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = format,
				.subresourceRange
				{
					.aspectMask = aspectFlags,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			VkImageView imageView = VK_NULL_HANDLE;
			VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
			WF_CORE_ASSERT(res == VK_SUCCESS, "Failed to create image view!");
			return imageView;
		}

		// Record a pipeline barrier to transition image layout using Synchronization 2 (vkCmdPipelineBarrier2)
		static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess,
			VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
		{
			VkImageMemoryBarrier2 barrier
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = srcStage,
				.srcAccessMask = srcAccess,
				.dstStageMask = dstStage,
				.dstAccessMask = dstAccess,
				.oldLayout = oldLayout,
				.newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image,
				.subresourceRange
				{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			VkDependencyInfo depInfo
			{
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &barrier
			};

			vkCmdPipelineBarrier2(cmd, &depInfo);
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
