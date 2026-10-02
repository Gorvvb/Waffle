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

	// Fullscreen post chain (bloom + composite) recorded via the RHI CommandBuffer, identical on GL and Vulkan. Framebuffers in/out - never raw texture IDs (that was the GL/Vulkan ID corruption bug). Settings live on CameraComponent; each camera passes its own to Process/ProcessAndPresent.
	class PostProcessing
	{
	public:
		static void Init();
		static void Shutdown();

		// Runs the post chain over `src`'s color attachment and returns the result framebuffer (for ImGui display); nullptr when EnablePostProcessing is off - present the source directly then.
		static Ref<Framebuffer> Process(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height, const PostProcessingSettings& settings);

		// Process + present the result to the screen (swapchain). Runtime path.
		static void ProcessAndPresent(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height, const PostProcessingSettings& settings);

	private:
		static void EnsureResources();
		static void EnsureSizes(uint32_t width, uint32_t height);
	};

}
