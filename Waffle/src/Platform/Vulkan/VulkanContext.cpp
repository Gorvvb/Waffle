#include "wfpch.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanContext.h"
#include "VulkanUtils.h"

#include <GLFW/glfw3.h>
#include <set>
#include <algorithm>

namespace Waffle {

	// -----------------------------------------------------------------------
	// Static instance
	// -----------------------------------------------------------------------
	VulkanContext* VulkanContext::s_Instance = nullptr;

	// -----------------------------------------------------------------------
	// Validation layers
	// -----------------------------------------------------------------------
	static const std::vector<const char*> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	// Required device extensions
	static const std::vector<const char*> s_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

#ifdef WF_DEBUG
	static constexpr bool s_EnableValidation = true;
#else
	static constexpr bool s_EnableValidation = false;
#endif

	// -----------------------------------------------------------------------
	// Debug messenger callback
	// -----------------------------------------------------------------------
	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
		VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* /*pUserData*/)
	{
		switch (severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			WF_CORE_TRACE("Vulkan: {0}", pCallbackData->pMessage); break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			WF_CORE_INFO("Vulkan: {0}", pCallbackData->pMessage); break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			WF_CORE_WARN("Vulkan: {0}", pCallbackData->pMessage); break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			WF_CORE_ERROR("Vulkan: {0}", pCallbackData->pMessage); break;
		default: break;
		}
		return VK_FALSE;
	}

	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func)
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
			func(instance, debugMessenger, pAllocator);
	}

	// -----------------------------------------------------------------------
	// Constructor / Destructor
	// -----------------------------------------------------------------------
	VulkanContext::VulkanContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		WF_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	VulkanContext::~VulkanContext()
	{
		if (m_Device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(m_Device);

		if (m_TimelineSemaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(m_Device, m_TimelineSemaphore, nullptr);

		// Depth resources
		if (m_DepthImageView != VK_NULL_HANDLE)
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
		if (m_DepthImage != VK_NULL_HANDLE)
			vmaDestroyImage(m_VmaAllocator, m_DepthImage, m_DepthImageAllocation);

		CleanupSwapChain();

		// Per-frame sync objects + command pools
		for (auto& frame : m_Frames)
		{
			if (frame.ImageAvailableSemaphore)
				vkDestroySemaphore(m_Device, frame.ImageAvailableSemaphore, nullptr);
			if (frame.RenderFinishedSemaphore)
				vkDestroySemaphore(m_Device, frame.RenderFinishedSemaphore, nullptr);
			if (frame.InFlightFence)
				vkDestroyFence(m_Device, frame.InFlightFence, nullptr);
			if (frame.CommandPool)
				vkDestroyCommandPool(m_Device, frame.CommandPool, nullptr);
		}

		if (m_DescriptorPool)
			vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
		if (m_CommandPool)
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		if (m_VmaAllocator)
			vmaDestroyAllocator(m_VmaAllocator);

		if (m_Device)
			vkDestroyDevice(m_Device, nullptr);

		if (s_EnableValidation && m_DebugMessenger)
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

		if (m_Surface)
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		if (m_Instance)
			vkDestroyInstance(m_Instance, nullptr);

		volkFinalize();

		s_Instance = nullptr;
	}

	// -----------------------------------------------------------------------
	// Init
	// -----------------------------------------------------------------------
	void VulkanContext::Init()
	{
		WF_PROFILE_FUNCTION();

		WF_CORE_ASSERT(!s_Instance, "VulkanContext already exists!");
		s_Instance = this;

		m_Frames.resize(m_FramesInFlight);

		CreateInstance();
		if (m_EnableValidation)
			SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();

		// Initialize Vulkan Memory Allocator (VMA) using imported Volk function pointers
		VmaVulkanFunctions vmaFuncInfo{};
		VmaAllocatorCreateInfo vmaAllocInfo
		{
			.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
			.physicalDevice = m_PhysicalDevice,
			.device = m_Device,
			.pVulkanFunctions = &vmaFuncInfo,
			.instance = m_Instance,
			.vulkanApiVersion = VK_API_VERSION_1_4
		};
		vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFuncInfo);
		VkResult vmaRes = vmaCreateAllocator(&vmaAllocInfo, &m_VmaAllocator);
		WF_CORE_ASSERT(vmaRes == VK_SUCCESS, "Failed to create Vulkan Memory Allocator (VMA)!");

		CreateSwapChain();
		CreateSwapChainImageViews();
		CreateDepthResources();
		CreateCommandPool();
		CreateSyncObjects();
		CreateCommandBuffers();
		CreateDescriptorPool();

		// Initialise descriptor-set slot array
		m_BoundDescriptorSets.resize(4, VK_NULL_HANDLE);

		// Default active rendering formats to swapchain & depth formats
		m_ActiveColorFormats = { m_SwapChainImageFormat };
		m_ActiveDepthFormat  = m_DepthFormat;

		// Set default viewport / scissor
		m_CurrentViewport.x        = 0.0f;
		m_CurrentViewport.y        = 0.0f;
		m_CurrentViewport.width    = (float)m_SwapChainExtent.width;
		m_CurrentViewport.height   = (float)m_SwapChainExtent.height;
		m_CurrentViewport.minDepth = 0.0f;
		m_CurrentViewport.maxDepth = 1.0f;
		m_CurrentScissor.offset    = { 0, 0 };
		m_CurrentScissor.extent    = m_SwapChainExtent;

		WF_CORE_INFO("Vulkan context initialised");
		WF_CORE_INFO("  Physical device: {0}", [this]() {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
			return std::string(props.deviceName);
		}());
	}

	// -----------------------------------------------------------------------
	// SwapBuffers — end frame, submit, present, advance frame index
	// -----------------------------------------------------------------------
	void VulkanContext::SwapBuffers()
	{
		WF_PROFILE_FUNCTION();

		// End dynamic rendering if still active (e.g. if nobody called Unbind on the default target)
		if (m_IsRenderingActive)
			EndSwapChainRendering();

		// Transition swap-chain image to present layout using Synchronization 2
		VkCommandBuffer cmd = GetCurrentCommandBuffer();

		VulkanUtils::TransitionImageLayout(cmd,
			m_SwapChainImages[m_CurrentImageIndex],
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_2_NONE);

		// End command buffer
		VkResult endCmdRes = vkEndCommandBuffer(cmd);
		WF_CORE_ASSERT(endCmdRes == VK_SUCCESS, "Failed to end command buffer!");

		// Submit using Synchronization 2 (vkQueueSubmit2) & Timeline Semaphore
		m_FrameCounter++;
		uint64_t signalValue = ++m_TimelineSignalValue;

		VkSemaphoreSubmitInfo imageAcquireWaitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = m_Frames[m_CurrentFrameIndex].ImageAvailableSemaphore,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
		};

		std::vector<VkSemaphoreSubmitInfo> signalSemaphores
		{
			{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_Frames[m_CurrentFrameIndex].RenderFinishedSemaphore,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
			},
			{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_TimelineSemaphore,
				.value = signalValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			}
		};

		VkCommandBufferSubmitInfo cmdSubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = m_Frames[m_CurrentFrameIndex].CommandBuffer
		};

		VkSubmitInfo2 submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &imageAcquireWaitInfo,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdSubmitInfo,
			.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphores.size()),
			.pSignalSemaphoreInfos = signalSemaphores.data()
		};

		vkResetFences(m_Device, 1, &m_Frames[m_CurrentFrameIndex].InFlightFence);
		VkResult submitResult = vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, m_Frames[m_CurrentFrameIndex].InFlightFence);
		if (submitResult != VK_SUCCESS)
			WF_CORE_ERROR("Vulkan: vkQueueSubmit2 failed ({0})", (int)submitResult);

		// Present
		VkPresentInfoKHR presentInfo
		{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_Frames[m_CurrentFrameIndex].RenderFinishedSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &m_SwapChain,
			.pImageIndices = &m_CurrentImageIndex
		};

		VkResult presentResult = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR
			|| m_SwapChainNeedsRecreation)
		{
			m_SwapChainNeedsRecreation = false;
			RecreateSwapChain();
		}

		// Advance frame, acquire next image
		m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_FramesInFlight;

		// Wait for the next frame's fence (from a previous submission)
		vkWaitForFences(m_Device, 1, &m_Frames[m_CurrentFrameIndex].InFlightFence, VK_TRUE, UINT64_MAX);

		// Safely free pending descriptor sets for this frame
		for (auto it = m_PendingDescriptorSetFrees.begin(); it != m_PendingDescriptorSetFrees.end(); )
		{
			if (it->FrameIndex == m_CurrentFrameIndex)
			{
				vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &it->Set);
				it = m_PendingDescriptorSetFrees.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Acquire next swap-chain image
		VkResult acquireResult = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX,
			m_Frames[m_CurrentFrameIndex].ImageAvailableSemaphore, VK_NULL_HANDLE,
			&m_CurrentImageIndex);

		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain();
			return;
		}

		// Begin the next command buffer
		vkResetCommandPool(m_Device, m_Frames[m_CurrentFrameIndex].CommandPool, 0);
		VkCommandBufferBeginInfo beginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		VkResult beginCmdRes = vkBeginCommandBuffer(m_Frames[m_CurrentFrameIndex].CommandBuffer, &beginInfo);
		WF_CORE_ASSERT(beginCmdRes == VK_SUCCESS, "Failed to begin command buffer!");
	}

	// -----------------------------------------------------------------------
	// Dynamic rendering helpers
	// -----------------------------------------------------------------------
	void VulkanContext::BeginSwapChainRendering(VkClearColorValue clearColor,
		VkClearDepthStencilValue clearDepth)
	{
		SetActiveRenderingFormats({ m_SwapChainImageFormat }, m_DepthFormat);
		m_BoundShader = nullptr;
		VkCommandBuffer cmd = GetCurrentCommandBuffer();

		// Transition swap-chain image to color attachment
		VulkanUtils::TransitionImageLayout(cmd,
			m_SwapChainImages[m_CurrentImageIndex],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

		// Transition depth image to depth-stencil attachment
		if (m_DepthImage != VK_NULL_HANDLE)
		{
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (VulkanUtils::HasStencilComponent(m_DepthFormat))
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

			VulkanUtils::TransitionImageLayout(cmd,
				m_DepthImage,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				0, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				aspectMask);
		}

		// Color attachment
		VkRenderingAttachmentInfo colorAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_SwapChainImageViews[m_CurrentImageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue{.color = clearColor}
		};

		// Depth attachment
		VkRenderingAttachmentInfo depthAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_DepthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue{.depthStencil = clearDepth}
		};

		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea
			{
				.offset{.x = 0, .y = 0},
				.extent = m_SwapChainExtent
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = VulkanUtils::HasStencilComponent(m_DepthFormat) ? &depthAttachment : nullptr
		};

		vkCmdBeginRendering(cmd, &renderingInfo);
		m_IsRenderingActive = true;
	}

	void VulkanContext::EndSwapChainRendering()
	{
		if (!m_IsRenderingActive) return;
		vkCmdEndRendering(GetCurrentCommandBuffer());
		m_IsRenderingActive = false;
	}

	// -----------------------------------------------------------------------
	// Descriptor-set slot management
	// -----------------------------------------------------------------------
	void VulkanContext::BindDescriptorSet(uint32_t set, VkDescriptorSet descriptorSet)
	{
		if (set >= m_BoundDescriptorSets.size())
			m_BoundDescriptorSets.resize(set + 1, VK_NULL_HANDLE);
		m_BoundDescriptorSets[set] = descriptorSet;
	}

	void VulkanContext::SafeFreeDescriptorSet(VkDescriptorSet set)
	{
		if (set == VK_NULL_HANDLE || m_Device == VK_NULL_HANDLE) return;
		m_PendingDescriptorSetFrees.push_back({ set, m_CurrentFrameIndex });
	}

	// -----------------------------------------------------------------------
	// Utilities
	// -----------------------------------------------------------------------
	VkCommandBuffer VulkanContext::GetCurrentCommandBuffer() const
	{
		return m_Frames[m_CurrentFrameIndex].CommandBuffer;
	}

	uint32_t VulkanContext::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const
	{
		return VulkanUtils::FindMemoryType(m_PhysicalDevice, typeFilter, props);
	}

	VkCommandBuffer VulkanContext::BeginSingleTimeCommands() const
	{
		WF_CORE_ASSERT(m_CommandPool != VK_NULL_HANDLE, "Command pool is null!");

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool        = m_CommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkResult res = vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);
		if (res != VK_SUCCESS || cmd == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkAllocateCommandBuffers failed in BeginSingleTimeCommands: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to allocate single time command buffer!");
			return VK_NULL_HANDLE;
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);
		return cmd;
	}

	void VulkanContext::EndSingleTimeCommands(VkCommandBuffer cmd) const
	{
		if (cmd == VK_NULL_HANDLE) return;

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &cmd;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
	}

	void VulkanContext::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands();
		VkBufferCopy copy{ 0, 0, size };
		vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
		EndSingleTimeCommands(cmd);
	}

	void VulkanContext::CopyBufferToImage(VkBuffer buffer, VkImage image,
		uint32_t width, uint32_t height) const
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset                    = 0;
		region.bufferRowLength                 = 0;
		region.bufferImageHeight               = 0;
		region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel       = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount     = 1;
		region.imageOffset                     = { 0, 0, 0 };
		region.imageExtent                     = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		EndSingleTimeCommands(cmd);
	}

	// =======================================================================
	// Private init functions
	// =======================================================================

	void VulkanContext::CreateInstance()
	{
		if (volkInitialize() != VK_SUCCESS)
		{
			WF_CORE_ERROR("Failed to initialize Volk!");
			WF_CORE_ASSERT(false, "Volk initialization failed!");
			return;
		}

		m_EnableValidation = s_EnableValidation && CheckValidationLayerSupport();
		if (s_EnableValidation && !m_EnableValidation)
			WF_CORE_WARN("Vulkan: validation layer 'VK_LAYER_KHRONOS_validation' requested but not available. Disabling validation.");

		VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Waffle Engine",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "Waffle",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_4
		};

		auto extensions = GetRequiredInstanceExtensions();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = VulkanDebugCallback
		};

		VkInstanceCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = m_EnableValidation ? &debugCreateInfo : nullptr,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = m_EnableValidation ? static_cast<uint32_t>(s_ValidationLayers.size()) : 0,
			.ppEnabledLayerNames = m_EnableValidation ? s_ValidationLayers.data() : nullptr,
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data()
		};

		VkResult res = vkCreateInstance(&createInfo, nullptr, &m_Instance);
		if (res != VK_SUCCESS)
		{
			WF_CORE_ERROR("vkCreateInstance failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan instance!");
			return;
		}

		volkLoadInstance(m_Instance);
	}

	void VulkanContext::SetupDebugMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = VulkanDebugCallback
		};

		WF_CORE_ASSERT(
			CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS,
			"Failed to set up Vulkan debug messenger!");
	}

	void VulkanContext::CreateSurface()
	{
		VkResult res = glfwCreateWindowSurface(m_Instance, m_WindowHandle, nullptr, &m_Surface);
		if (res != VK_SUCCESS || m_Surface == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("glfwCreateWindowSurface failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan window surface!");
		}
	}

	void VulkanContext::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
		WF_CORE_ASSERT(deviceCount > 0, "Failed to find a GPU with Vulkan support!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

		// Prefer discrete GPU
		for (auto& device : devices)
		{
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);
			if (IsDeviceSuitable(device) && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				m_PhysicalDevice = device;
				return;
			}
		}
		// Fall back to any suitable device
		for (auto& device : devices)
		{
			if (IsDeviceSuitable(device))
			{
				m_PhysicalDevice = device;
				return;
			}
		}

		WF_CORE_ASSERT(false, "Failed to find a suitable Vulkan GPU!");
	}

	void VulkanContext::CreateLogicalDevice()
	{
		auto indices = FindQueueFamilies(m_PhysicalDevice);
		m_GraphicsQueueFamily = indices.GraphicsFamily.value();
		m_PresentQueueFamily  = indices.PresentFamily.value();

		std::set<uint32_t> uniqueQueueFamilies = { m_GraphicsQueueFamily, m_PresentQueueFamily };
		float queuePriority = 1.0f;

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueInfo
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamily,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			};
			queueCreateInfos.push_back(queueInfo);
		}

		// Vulkan 1.4 feature chain using C++20 designated initializers
		VkPhysicalDeviceVulkan14Features features14
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
			.pNext = nullptr
		};
		VkPhysicalDeviceVulkan13Features features13
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &features14,
			.shaderDemoteToHelperInvocation = VK_TRUE,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE
		};
		VkPhysicalDeviceVulkan12Features features12
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &features13,
			.timelineSemaphore = VK_TRUE,
			.bufferDeviceAddress = VK_TRUE
		};
		VkPhysicalDeviceFeatures2 deviceFeatures2
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &features12,
			.features
			{
				.independentBlend = VK_TRUE,
				.fillModeNonSolid = VK_TRUE,
				.wideLines = VK_TRUE,
				.samplerAnisotropy = VK_TRUE
			}
		};

		VkDeviceCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &deviceFeatures2,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(s_DeviceExtensions.size()),
			.ppEnabledExtensionNames = s_DeviceExtensions.data()
		};

		VkResult res = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
		if (res != VK_SUCCESS || m_Device == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateDevice failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan logical device!");
		}

		volkLoadDevice(m_Device);

		vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_PresentQueueFamily,  0, &m_PresentQueue);
	}

	void VulkanContext::CreateSwapChain()
	{
		auto swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);
		auto surfaceFormat    = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
		auto presentMode      = ChooseSwapPresentMode(swapChainSupport.PresentModes);
		auto extent           = ChooseSwapExtent(swapChainSupport.Capabilities);

		uint32_t imageCount = swapChainSupport.Capabilities.minImageCount + 1;
		if (swapChainSupport.Capabilities.maxImageCount > 0 &&
			imageCount > swapChainSupport.Capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.Capabilities.maxImageCount;
		}
		// Clamp to configured frames-in-flight
		imageCount = std::max(imageCount, m_FramesInFlight);
		if (swapChainSupport.Capabilities.maxImageCount > 0)
			imageCount = std::min(imageCount, swapChainSupport.Capabilities.maxImageCount);

		// Select supported composite alpha
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		};
		for (auto flag : compositeAlphaFlags)
		{
			if (swapChainSupport.Capabilities.supportedCompositeAlpha & flag)
			{
				compositeAlpha = flag;
				break;
			}
		}

		extent.width  = std::max(1u, extent.width);
		extent.height = std::max(1u, extent.height);

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface          = m_Surface;
		createInfo.minImageCount    = imageCount;
		createInfo.imageFormat      = surfaceFormat.format;
		createInfo.imageColorSpace  = surfaceFormat.colorSpace;
		createInfo.imageExtent      = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		uint32_t queueFamilyIndices[] = { m_GraphicsQueueFamily, m_PresentQueueFamily };
		if (m_GraphicsQueueFamily != m_PresentQueueFamily)
		{
			createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices   = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform   = swapChainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = compositeAlpha;
		createInfo.presentMode    = presentMode;
		createInfo.clipped        = VK_TRUE;
		createInfo.oldSwapchain   = VK_NULL_HANDLE;

		VkResult res = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain);
		if (res != VK_SUCCESS || m_SwapChain == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateSwapchainKHR failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan swap chain!");
		}

		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent      = extent;
	}

	void VulkanContext::CreateSwapChainImageViews()
	{
		m_SwapChainImageViews.resize(m_SwapChainImages.size());
		for (size_t i = 0; i < m_SwapChainImages.size(); i++)
		{
			m_SwapChainImageViews[i] = VulkanUtils::CreateImageView(
				m_Device, m_SwapChainImages[i],
				m_SwapChainImageFormat,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	void VulkanContext::CreateDepthResources()
	{
		m_DepthFormat = VulkanUtils::FindSupportedFormat(m_PhysicalDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VulkanUtils::CreateImage(m_VmaAllocator,
			m_SwapChainExtent.width, m_SwapChainExtent.height,
			m_DepthFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_DepthImage, m_DepthImageAllocation,
			VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

		VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (VulkanUtils::HasStencilComponent(m_DepthFormat))
			aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

		m_DepthImageView = VulkanUtils::CreateImageView(m_Device, m_DepthImage,
			m_DepthFormat, aspectFlags);
	}

	void VulkanContext::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = m_GraphicsQueueFamily
		};

		VkResult res = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
		if (res != VK_SUCCESS || m_CommandPool == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateCommandPool failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan command pool!");
		}
	}

	void VulkanContext::CreateCommandBuffers()
	{
		if (m_Frames.empty())
			m_Frames.resize(m_FramesInFlight);

		for (auto& frame : m_Frames)
		{
			// Command pool
			VkCommandPoolCreateInfo poolInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_GraphicsQueueFamily
			};
			VkResult cpRes = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &frame.CommandPool);
			WF_CORE_ASSERT(cpRes == VK_SUCCESS, "Failed to create command pool!");

			// Command buffer
			VkCommandBufferAllocateInfo allocInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = frame.CommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};
			VkResult cbRes = vkAllocateCommandBuffers(m_Device, &allocInfo, &frame.CommandBuffer);
			WF_CORE_ASSERT(cbRes == VK_SUCCESS, "Failed to allocate command buffer!");
		}

		// Begin the first command buffer immediately so Init() callers can use it
		VkCommandBufferBeginInfo beginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		// Acquire first image and signal the first frame's ImageAvailableSemaphore
		VkResult acqRes = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX,
			m_Frames[0].ImageAvailableSemaphore, VK_NULL_HANDLE,
			&m_CurrentImageIndex);
		WF_CORE_ASSERT(acqRes == VK_SUCCESS || acqRes == VK_SUBOPTIMAL_KHR, "Failed to acquire initial swapchain image!");

		VkResult beginRes = vkBeginCommandBuffer(m_Frames[0].CommandBuffer, &beginInfo);
		WF_CORE_ASSERT(beginRes == VK_SUCCESS, "Failed to begin initial command buffer!");
	}

	void VulkanContext::CreateSyncObjects()
	{
		if (m_Frames.empty())
			m_Frames.resize(m_FramesInFlight);

		// Create timeline semaphore for frame synchronization
		m_TimelineSignalValue = m_FramesInFlight;
		VkSemaphoreTypeCreateInfo semaphoreTypeInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = m_FramesInFlight
		};
		VkSemaphoreCreateInfo timelineCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphoreTypeInfo
		};
		VkResult tsRes = vkCreateSemaphore(m_Device, &timelineCreateInfo, nullptr, &m_TimelineSemaphore);
		WF_CORE_ASSERT(tsRes == VK_SUCCESS, "Failed to create timeline semaphore!");

		VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkFenceCreateInfo fenceInfo
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		for (auto& frame : m_Frames)
		{
			VkResult s1 = vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &frame.ImageAvailableSemaphore);
			VkResult s2 = vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &frame.RenderFinishedSemaphore);
			VkResult f1 = vkCreateFence(m_Device, &fenceInfo, nullptr, &frame.InFlightFence);
			WF_CORE_ASSERT(s1 == VK_SUCCESS && s2 == VK_SUCCESS && f1 == VK_SUCCESS,
				"Failed to create Vulkan sync objects!");
		}
	}

	void VulkanContext::CreateDescriptorPool()
	{
		// Large general-purpose pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER,                 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          100  },
		};

		VkDescriptorPoolCreateInfo poolInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 2000,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data()
		};

		VkResult res = vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool);
		if (res != VK_SUCCESS || m_DescriptorPool == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateDescriptorPool failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to create Vulkan descriptor pool!");
		}
	}

	// -----------------------------------------------------------------------
	// Swap chain recreation
	// -----------------------------------------------------------------------
	void VulkanContext::CleanupSwapChain()
	{
		for (auto view : m_SwapChainImageViews)
			vkDestroyImageView(m_Device, view, nullptr);
		m_SwapChainImageViews.clear();

		vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
	}

	void VulkanContext::RecreateSwapChain()
	{
		// Wait for the window to have a valid size (minimised case)
		int width = 0, height = 0;
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_WindowHandle, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_Device);

		// Cleanup old depth
		if (m_DepthImageView)
		{
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
			m_DepthImageView = VK_NULL_HANDLE;
		}
		if (m_DepthImage)
		{
			vmaDestroyImage(m_VmaAllocator, m_DepthImage, m_DepthImageAllocation);
			m_DepthImage = VK_NULL_HANDLE;
			m_DepthImageAllocation = VK_NULL_HANDLE;
		}

		CleanupSwapChain();

		CreateSwapChain();
		CreateSwapChainImageViews();
		CreateDepthResources();

		// Update viewport/scissor
		m_CurrentViewport.width  = (float)m_SwapChainExtent.width;
		m_CurrentViewport.height = (float)m_SwapChainExtent.height;
		m_CurrentScissor.extent  = m_SwapChainExtent;
	}

	// -----------------------------------------------------------------------
	// Helper queries
	// -----------------------------------------------------------------------
	bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device) const
	{
		auto indices = FindQueueFamilies(device);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			auto details = QuerySwapChainSupport(device);
			swapChainAdequate = !details.Formats.empty() && !details.PresentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.IsComplete() && extensionsSupported && swapChainAdequate
			&& supportedFeatures.samplerAnisotropy;
	}

	bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
	{
		uint32_t count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
		std::vector<VkExtensionProperties> available(count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

		std::set<std::string> required(s_DeviceExtensions.begin(), s_DeviceExtensions.end());
		for (auto& ext : available)
			required.erase(ext.extensionName);
		return required.empty();
	}

	bool VulkanContext::CheckValidationLayerSupport() const
	{
		uint32_t count;
		vkEnumerateInstanceLayerProperties(&count, nullptr);
		std::vector<VkLayerProperties> layers(count);
		vkEnumerateInstanceLayerProperties(&count, layers.data());

		for (const char* layerName : s_ValidationLayers)
		{
			bool found = false;
			for (auto& props : layers)
				if (strcmp(layerName, props.layerName) == 0) { found = true; break; }
			if (!found) return false;
		}
		return true;
	}

	std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions() const
	{
		uint32_t glfwCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		WF_CORE_ASSERT(glfwExtensions && glfwCount > 0, "glfwGetRequiredInstanceExtensions failed to retrieve Vulkan surface extensions!");

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);
		for (uint32_t i = 0; i < glfwCount; i++)
			WF_CORE_INFO("Vulkan instance extension: {0}", glfwExtensions[i]);

		if (m_EnableValidation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		return extensions;
	}

	VulkanContext::QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device) const
	{
		QueueFamilyIndices indices;
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
		std::vector<VkQueueFamilyProperties> families(count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

		for (uint32_t i = 0; i < count; i++)
		{
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.GraphicsFamily = i;

			VkBool32 presentSupport = false;
			if (m_Surface != VK_NULL_HANDLE)
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
			if (presentSupport)
				indices.PresentFamily = i;

			if (indices.IsComplete()) break;
		}
		return indices;
	}

	VulkanContext::SwapChainSupportDetails VulkanContext::QuerySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details;
		if (m_Surface == VK_NULL_HANDLE) return details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
		if (formatCount)
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.Formats.data());
		}

		uint32_t modeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &modeCount, nullptr);
		if (modeCount)
		{
			details.PresentModes.resize(modeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &modeCount, details.PresentModes.data());
		}
		return details;
	}

	VkSurfaceFormatKHR VulkanContext::ChooseSwapSurfaceFormat(
		const std::vector<VkSurfaceFormatKHR>& available) const
	{
		for (auto& fmt : available)
			if ((fmt.format == VK_FORMAT_B8G8R8A8_UNORM || fmt.format == VK_FORMAT_R8G8B8A8_UNORM)
				&& fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return fmt;
		for (auto& fmt : available)
			if ((fmt.format == VK_FORMAT_B8G8R8A8_SRGB || fmt.format == VK_FORMAT_R8G8B8A8_SRGB)
				&& fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return fmt;
		return available[0];
	}

	VkPresentModeKHR VulkanContext::ChooseSwapPresentMode(
		const std::vector<VkPresentModeKHR>& available) const
	{
		for (auto& mode : available)
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be present
	}

	VkExtent2D VulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) const
	{
		if (caps.currentExtent.width != UINT32_MAX)
			return caps.currentExtent;

		int width, height;
		glfwGetFramebufferSize(m_WindowHandle, &width, &height);

		VkExtent2D extent = { (uint32_t)width, (uint32_t)height };
		extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
		extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
		return extent;
	}

} // namespace Waffle
