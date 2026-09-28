#pragma once

#include "Waffle/Renderer/GraphicsContext.h"
#include "Waffle/Core/Base.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <functional>

namespace Waffle {

	class VulkanShader;
	class VulkanVertexArray;

	// Owns all core Vulkan infrastructure. Uses Vulkan 1.3 dynamic rendering (no VkRenderPass).
	class VulkanContext : public GraphicsContext
	{
	public:
		explicit VulkanContext(GLFWwindow* windowHandle);
		virtual ~VulkanContext();

		virtual void Init() override;
		virtual void SwapBuffers() override;

		static VulkanContext* Get() { return s_Instance; }

		VkInstance        GetInstance()            const { return m_Instance; }
		VkPhysicalDevice  GetPhysicalDevice()      const { return m_PhysicalDevice; }
		VkDevice          GetDevice()              const { return m_Device; }
		VkQueue           GetGraphicsQueue()       const { return m_GraphicsQueue; }
		uint32_t          GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
		VkDescriptorPool  GetDescriptorPool()      const { return m_DescriptorPool; }
		VkSurfaceKHR      GetSurface()             const { return m_Surface; }
		VmaAllocator      GetVmaAllocator()        const { return m_VmaAllocator; }
		VkSemaphore       GetTimelineSemaphore()   const { return m_TimelineSemaphore; }

		VkFormat          GetSwapChainImageFormat() const { return m_SwapChainImageFormat; }
		VkFormat          GetSwapChainFormat()      const { return m_SwapChainImageFormat; }
		VkFormat          GetDepthFormat()          const { return m_DepthFormat; }
		VkExtent2D        GetSwapChainExtent()      const { return m_SwapChainExtent; }
		uint32_t          GetSwapChainImageCount()  const { return (uint32_t)m_SwapChainImages.size(); }
		uint32_t          GetFramesInFlight()       const { return m_FramesInFlight; }
		void              SetFramesInFlight(uint32_t count) { m_FramesInFlight = count; } // Call before Init()

		void SafeFreeDescriptorSet(VkDescriptorSet set);

		void CreateSwapchainRenderFinishedSemaphores();
		void DestroySwapchainRenderFinishedSemaphores();

		VkCommandBuffer   GetCurrentCommandBuffer() const;
		uint32_t          GetCurrentFrameIndex()    const { return m_CurrentFrameIndex; }
		uint32_t          GetCurrentImageIndex()    const { return m_CurrentImageIndex; }

		// Waits on timeline semaphore until all other frame slots have finished, safe to overwrite mapped memory.
		void WaitForFrameUploads(uint32_t targetFrameIndex);

		// Begin/end the default swap chain render target via VK_KHR_dynamic_rendering.
		void BeginSwapChainRendering(VkClearColorValue clearColor, VkClearDepthStencilValue clearDepth);
		void EndSwapChainRendering();
		bool IsRenderingActive()             const { return m_IsRenderingActive; }
		void SetRenderingActive(bool active) { m_IsRenderingActive = active; }

		void NotifyRenderingBegan() { m_IsRenderingActive = true; }
		void NotifyRenderingEnded() { m_IsRenderingActive = false; }

		struct UniformBufferBindInfo {
			VkBuffer     Buffer = VK_NULL_HANDLE;
			VkDeviceSize Size = 0;
			uint64_t     DynamicOffset = 0; // Applied as dynamic offset when binding descriptor sets.
		};
		struct TextureBindInfo {
			VkImageView ImageView = VK_NULL_HANDLE;
			VkSampler   Sampler = VK_NULL_HANDLE;
		};

		void RegisterUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size, uint64_t dynamicOffset = 0) {
			m_BoundUniformBuffers[binding] = { buffer, size, dynamicOffset };
		}
		void RegisterTexture(uint32_t slot, VkImageView imageView, VkSampler sampler) {
			m_BoundTextures[slot] = { imageView, sampler };
		}

		// Unregister on destruction to prevent use-after-free at draw time.
		void UnregisterUniformBuffer(uint32_t binding) {
			m_BoundUniformBuffers.erase(binding);
		}
		void UnregisterTexture(VkImageView imageView, VkSampler sampler) {
			for (auto it = m_BoundTextures.begin(); it != m_BoundTextures.end(); )
			{
				if (it->second.ImageView == imageView && it->second.Sampler == sampler)
					it = m_BoundTextures.erase(it);
				else
					++it;
			}
		}

		UniformBufferBindInfo GetUniformBuffer(uint32_t binding) const {
			auto it = m_BoundUniformBuffers.find(binding);
			if (it != m_BoundUniformBuffers.end()) return it->second;
			return {};
		}
		TextureBindInfo GetTexture(uint32_t slot) const {
			auto it = m_BoundTextures.find(slot);
			if (it != m_BoundTextures.end()) return it->second;
			it = m_BoundTextures.find(0);
			if (it != m_BoundTextures.end()) return it->second;
			return {};
		}

		void BindDescriptorSet(uint32_t set, VkDescriptorSet descriptorSet);
		const std::vector<VkDescriptorSet>& GetBoundDescriptorSets() const { return m_BoundDescriptorSets; }
		void ClearBoundDescriptorSets() { m_BoundDescriptorSets.clear(); m_BoundDescriptorSets.resize(4, VK_NULL_HANDLE); }

		VkViewport GetCurrentViewport() const { return m_CurrentViewport; }
		VkRect2D   GetCurrentScissor()  const { return m_CurrentScissor; }
		void       SetViewport(VkViewport vp, VkRect2D sc) { m_CurrentViewport = vp; m_CurrentScissor = sc; }

		void              SetClearColor(const glm::vec4& color);
		VkClearColorValue GetClearColor() const { return m_ClearColor; }
		void              SetClearColor(VkClearColorValue c) { m_ClearColor = c; }

		void SetActiveRenderingFormats(const std::vector<VkFormat>& colorFormats, VkFormat depthFormat)
		{
			m_ActiveColorFormats = colorFormats;
			m_ActiveDepthFormat = depthFormat;
		}
		const std::vector<VkFormat>& GetActiveColorFormats() const { return m_ActiveColorFormats; }
		VkFormat GetActiveDepthFormat() const { return m_ActiveDepthFormat; }

		uint32_t       FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
		VkCommandBuffer BeginSingleTimeCommands() const;
		void            EndSingleTimeCommands(VkCommandBuffer cmd) const;
		void            CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
		void            CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;

	private:
		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateSwapChainImageViews();
		void CreateDepthResources();
		void CreateCommandPool();
		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CreateDescriptorPool();

		// Returns false if recreation was aborted (for example: minimised window).
		bool RecreateSwapChain();
		void CleanupSwapChain();

		bool     IsDeviceSuitable(VkPhysicalDevice device) const;
		bool     CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
		bool     CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredInstanceExtensions() const;

		struct QueueFamilyIndices {
			std::optional<uint32_t> GraphicsFamily;
			std::optional<uint32_t> PresentFamily;
			bool IsComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
		};
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR        Capabilities;
			std::vector<VkSurfaceFormatKHR> Formats;
			std::vector<VkPresentModeKHR>   PresentModes;
		};
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;
		VkSurfaceFormatKHR      ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) const;
		VkPresentModeKHR        ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available) const;
		VkExtent2D              ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) const;

	private:
		GLFWwindow* m_WindowHandle = nullptr;

		VkInstance               m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		bool                     m_EnableValidation = false;
		VkSurfaceKHR             m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice                 m_Device = VK_NULL_HANDLE;

		VkQueue  m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue  m_PresentQueue = VK_NULL_HANDLE;
		uint32_t m_GraphicsQueueFamily = 0;
		uint32_t m_PresentQueueFamily = 0;

		VkSwapchainKHR           m_SwapChain = VK_NULL_HANDLE;
		std::vector<VkImage>     m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		VkFormat                 m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D               m_SwapChainExtent = {};

		VmaAllocator m_VmaAllocator = VK_NULL_HANDLE;

		VkImage       m_DepthImage = VK_NULL_HANDLE;
		VmaAllocation m_DepthImageAllocation = VK_NULL_HANDLE;
		VkImageView   m_DepthImageView = VK_NULL_HANDLE;
		VkFormat      m_DepthFormat = VK_FORMAT_UNDEFINED;

		VkSemaphore m_TimelineSemaphore = VK_NULL_HANDLE;
		uint64_t    m_TimelineSignalValue = 0;
		uint64_t    m_FrameCounter = 0;

		struct FrameData {
			VkCommandPool   CommandPool = VK_NULL_HANDLE;
			VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
			VkSemaphore     ImageAvailableSemaphore = VK_NULL_HANDLE;
			VkFence         InFlightFence = VK_NULL_HANDLE;
			uint64_t        LastTimelineValue = 0; // Used to gate host writes to shared mapped buffers.
		};
		std::vector<FrameData> m_Frames;
		uint32_t m_CurrentFrameIndex = 0;
		uint32_t m_CurrentImageIndex = 0;
		uint32_t m_FramesInFlight = 2;
		uint64_t m_UploadsSyncedTimeline = 0;

		// One semaphore per swapchain image; present(image i) waits on it before re-signaling.
		std::vector<VkSemaphore> m_SwapchainRenderFinished;

		VkCommandPool    m_CommandPool = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		bool m_IsRenderingActive = false;
		bool m_SwapChainNeedsRecreation = false;

		// Tracks whether the swapchain was already cleared this frame to avoid wiping a presented image.
		bool m_SwapchainClearedThisFrame = false;
		bool m_SwapchainImageInColor = false;

		std::vector<VkDescriptorSet>                        m_BoundDescriptorSets;
		std::unordered_map<uint32_t, UniformBufferBindInfo> m_BoundUniformBuffers;
		std::unordered_map<uint32_t, TextureBindInfo>       m_BoundTextures;

		struct PendingDescriptorSetFree {
			VkDescriptorSet Set = VK_NULL_HANDLE;
			uint32_t        FrameIndex = 0;
		};
		std::vector<PendingDescriptorSetFree> m_PendingDescriptorSetFrees;

		VkClearColorValue m_ClearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkViewport        m_CurrentViewport = {};
		VkRect2D          m_CurrentScissor = {};

		std::vector<VkFormat> m_ActiveColorFormats;
		VkFormat              m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;

		static VulkanContext* s_Instance;
	};
}