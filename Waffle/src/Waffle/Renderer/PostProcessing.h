#pragma once

#include "Waffle/Core/Ref.h"
#include <glm/glm.hpp>

namespace Waffle {

	class Framebuffer;

	struct PostProcessingSettings
	{
		bool EnablePostProcessing = false;

		// Bloom
		bool EnableBloom = false;
		float BloomThreshold = 0.8f;
		float BloomIntensity = 1.0f;
		glm::vec3 BloomColor = glm::vec3(1.0f, 1.0f, 1.0f);

		// Vignette
		bool EnableVignette = false;
		float VignetteIntensity = 0.4f;
		float VignetteSmoothness = 0.6f;
		glm::vec3 VignetteColor = glm::vec3(0.0f, 0.0f, 0.0f);

		// Tonemapping & Color Grading
		bool EnableTonemapping = false;
		float Exposure = 1.0f;
		float Saturation = 1.0f;
		float Contrast = 1.0f;
		glm::vec3 ColorGradingTint = glm::vec3(1.0f, 1.0f, 1.0f);
	};

	// Fullscreen post chain (bloom + composite), recorded through the RHI
	// CommandBuffer so it runs identically on OpenGL and Vulkan. Operates on
	// framebuffers, never on raw texture IDs - backend handles must not
	// cross this boundary (that was the original GL/Vulkan ID corruption bug).
	class PostProcessing
	{
	public:
		static void Init();
		static void Shutdown();

		static PostProcessingSettings& GetSettings() { return s_Settings; }

		// Runs the post chain over `src`'s color attachment and returns the
		// framebuffer holding the result (for display inside an ImGui image).
		static Ref<Framebuffer> Process(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height);

		// Process + present the result to the screen (swapchain / default
		// framebuffer). Runtime path.
		static void ProcessAndPresent(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height);

	private:
		static void EnsureResources();
		static void EnsureSizes(uint32_t width, uint32_t height);

		inline static PostProcessingSettings s_Settings;
	};

}
