#pragma once

#include "Waffle/Renderer/GraphicsContext.h"
#include "Waffle/Core/Base.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <functional>

namespace Waffle {

	// -----------------------------------------------------------------------
	// Forward declarations used by other Vulkan classes
	// -----------------------------------------------------------------------
	class VulkanShader;
	class VulkanVertexArray;

	// -----------------------------------------------------------------------
	// VulkanContext
	// Owns ALL core Vulkan infrastructure: instance, device, swap chain,
	// command buffers, sync objects, and descriptor pool.
	// Uses Vulkan 1.3 dynamic rendering (no deprecated VkRenderPass for drawing).
	// -----------------------------------------------------------------------
	class VulkanContext : public GraphicsContext
	{
	public:
		explicit VulkanContext(GLFWwindow* windowHandle);
		virtual ~VulkanContext();

		virtual void Init() override;
		virtual void SwapBuffers() override;

		// ---- Singleton accessor (set during Init) ---------------------------
		static VulkanContext* Get() { return s_Instance; }

		// ---- Device handles -------------------------------------------------
		VkInstance        GetInstance()          const { return m_Instance; }
		VkPhysicalDevice  GetPhysicalDevice()    const { return m_PhysicalDevice; }
		VkDevice          GetDevice()            const { return m_Device; }
		VkQueue           GetGraphicsQueue()     const { return m_GraphicsQueue; }
		uint32_t          GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
		VkDescriptorPool  GetDescriptorPool()    const { return m_DescriptorPool; }
		VkSurfaceKHR      GetSurface()           const { return m_Surface; }

		// ---- Swap chain info ------------------------------------------------
		VkFormat          GetSwapChainImageFormat() const { return m_SwapChainImageFormat; }
		VkFormat          GetSwapChainFormat()      const { return m_SwapChainImageFormat; }
		VkFormat          GetDepthFormat()          const { return m_DepthFormat; }
		VkExtent2D        GetSwapChainExtent()      const { return m_SwapChainExtent; }
		uint32_t          GetSwapChainImageCount()  const { return (uint32_t)m_SwapChainImages.size(); }
		uint32_t          GetFramesInFlight()       const { return m_FramesInFlight; }
		void              SetFramesInFlight(uint32_t count) { m_FramesInFlight = count; } // Call before Init()

		// Safe per-frame descriptor set deletion
		void SafeFreeDescriptorSet(VkDescriptorSet set);

		// ---- Per-frame state ------------------------------------------------
		VkCommandBuffer   GetCurrentCommandBuffer() const;
		uint32_t          GetCurrentFrameIndex()    const { return m_CurrentFrameIndex; }
		uint32_t          GetCurrentImageIndex()    const { return m_CurrentImageIndex; }

		// ---- Rendering state (dynamic rendering without render passes) ------
		// BeginSwapChainRendering / EndSwapChainRendering manage the default
		// swap chain render target using VK_KHR_dynamic_rendering.
		void BeginSwapChainRendering(VkClearColorValue clearColor, VkClearDepthStencilValue clearDepth);
		void EndSwapChainRendering();
		bool IsRenderingActive()             const { return m_IsRenderingActive; }
		void SetRenderingActive(bool active)       { m_IsRenderingActive = active; }

		// External rendering (off-screen framebuffers) begin / end tracking.
		void NotifyRenderingBegan()  { m_IsRenderingActive = true; }
		void NotifyRenderingEnded()  { m_IsRenderingActive = false; }

		// ---- Bound-shader / vertex-array tracking (for pipeline lookup) -----
		void SetBoundShader(VulkanShader* shader)           { m_BoundShader = shader; }
		void SetBoundVertexArray(VulkanVertexArray* va)     { m_BoundVertexArray = va; }
		VulkanShader*      GetBoundShader()      const { return m_BoundShader; }
		VulkanVertexArray* GetBoundVertexArray() const { return m_BoundVertexArray; }

		// ---- Descriptor-set tracking (UBOs / textures) ----------------------
		struct UniformBufferBindInfo {
			VkBuffer Buffer = VK_NULL_HANDLE;
			VkDeviceSize Size = 0;
		};
		struct TextureBindInfo {
			VkImageView ImageView = VK_NULL_HANDLE;
			VkSampler Sampler = VK_NULL_HANDLE;
		};

		void RegisterUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size) {
			m_BoundUniformBuffers[binding] = { buffer, size };
		}
		void RegisterTexture(uint32_t slot, VkImageView imageView, VkSampler sampler) {
			m_BoundTextures[slot] = { imageView, sampler };
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

		// Called by VulkanUniformBuffer and VulkanTexture to register their sets.
		void BindDescriptorSet(uint32_t set, VkDescriptorSet descriptorSet);
		const std::vector<VkDescriptorSet>& GetBoundDescriptorSets() const { return m_BoundDescriptorSets; }
		void ClearBoundDescriptorSets() { m_BoundDescriptorSets.clear(); m_BoundDescriptorSets.resize(4, VK_NULL_HANDLE); }

		// ---- Viewport / scissor (set by RendererAPI, used by pipeline) ------
		VkViewport GetCurrentViewport() const { return m_CurrentViewport; }
		VkRect2D   GetCurrentScissor()  const { return m_CurrentScissor; }
		void       SetViewport(VkViewport vp, VkRect2D sc) { m_CurrentViewport = vp; m_CurrentScissor = sc; }

		// ---- Clear values ---------------------------------------------------
		void SetClearColor(const glm::vec4& color);
		VkClearColorValue GetClearColor() const { return m_ClearColor; }

		// Dynamic rendering attachment format tracking
		void SetActiveRenderingFormats(const std::vector<VkFormat>& colorFormats, VkFormat depthFormat)
		{
			m_ActiveColorFormats = colorFormats;
			m_ActiveDepthFormat = depthFormat;
		}
		const std::vector<VkFormat>& GetActiveColorFormats() const { return m_ActiveColorFormats; }
		VkFormat GetActiveDepthFormat() const { return m_ActiveDepthFormat; }
		void              SetClearColor(VkClearColorValue c) { m_ClearColor = c; }

		// ---- Utilities ------------------------------------------------------
		uint32_t       FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
		VkCommandBuffer BeginSingleTimeCommands() const;
		void            EndSingleTimeCommands(VkCommandBuffer cmd) const;

		// Copy a staging buffer to a device-local buffer.
		void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
		// Copy a staging buffer to a VkImage.
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;

	private:
		// ---- Init stages ----------------------------------------------------
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

		// ---- Swap-chain helpers ---------------------------------------------
		void RecreateSwapChain();
		void CleanupSwapChain();

		// ---- Device helpers -------------------------------------------------
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

		// Core objects
		VkInstance               m_Instance       = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		bool                     m_EnableValidation = false;
		VkSurfaceKHR             m_Surface        = VK_NULL_HANDLE;
		VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice                 m_Device         = VK_NULL_HANDLE;

		// Queues
		VkQueue  m_GraphicsQueue      = VK_NULL_HANDLE;
		VkQueue  m_PresentQueue       = VK_NULL_HANDLE;
		uint32_t m_GraphicsQueueFamily = 0;
		uint32_t m_PresentQueueFamily  = 0;

		// Swap chain
		VkSwapchainKHR           m_SwapChain           = VK_NULL_HANDLE;
		std::vector<VkImage>     m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		VkFormat                 m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D               m_SwapChainExtent      = {};

		// Depth attachment (for the default swap-chain target)
		VkImage        m_DepthImage       = VK_NULL_HANDLE;
		VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
		VkImageView    m_DepthImageView   = VK_NULL_HANDLE;
		VkFormat       m_DepthFormat      = VK_FORMAT_UNDEFINED;

		// Per-frame data
		struct FrameData {
			VkCommandPool   CommandPool   = VK_NULL_HANDLE;
			VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
			VkSemaphore     ImageAvailableSemaphore  = VK_NULL_HANDLE;
			VkSemaphore     RenderFinishedSemaphore  = VK_NULL_HANDLE;
			VkFence         InFlightFence            = VK_NULL_HANDLE;
		};
		std::vector<FrameData> m_Frames;
		uint32_t m_CurrentFrameIndex = 0;
		uint32_t m_CurrentImageIndex = 0;
		uint32_t m_FramesInFlight    = 2;  // configurable before Init()

		// Single-time command pool
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		// Shared descriptor pool
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		// Rendering state
		bool m_IsRenderingActive = false;
		bool m_SwapChainNeedsRecreation = false;

		// Bound pipeline state (updated by VulkanShader/VulkanVertexArray::Bind())
		VulkanShader*      m_BoundShader      = nullptr;
		VulkanVertexArray* m_BoundVertexArray = nullptr;

		// Descriptor set slots [0..3] — updated by UBOs and textures
		std::vector<VkDescriptorSet> m_BoundDescriptorSets;
		std::unordered_map<uint32_t, UniformBufferBindInfo> m_BoundUniformBuffers;
		std::unordered_map<uint32_t, TextureBindInfo>       m_BoundTextures;

		// Safe per-frame deletion queue
		struct PendingDescriptorSetFree {
			VkDescriptorSet Set = VK_NULL_HANDLE;
			uint32_t FrameIndex = 0;
		};
		std::vector<PendingDescriptorSetFree> m_PendingDescriptorSetFrees;

		// Clear / viewport state
		VkClearColorValue m_ClearColor      = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkViewport        m_CurrentViewport = {};
		VkRect2D          m_CurrentScissor  = {};

		// Active dynamic rendering formats
		std::vector<VkFormat> m_ActiveColorFormats;
		VkFormat              m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;

		static VulkanContext* s_Instance;
	};

} // namespace Waffle
