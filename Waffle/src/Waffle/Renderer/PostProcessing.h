#pragma once

#include "Waffle/Core/Ref.h"
#include <glm/glm.hpp>

namespace Waffle {

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

	class PostProcessing
	{
	public:
		static void Init();
		static void Shutdown();

		static PostProcessingSettings& GetSettings() { return s_Settings; }
		static uint32_t Process(uint32_t inputTextureID, uint32_t width, uint32_t height);
		static void PresentToScreen(uint32_t textureID, uint32_t width, uint32_t height);

	private:
		inline static PostProcessingSettings s_Settings;
	};

}
