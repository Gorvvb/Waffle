#pragma once

#include "Waffle/Core/Ref.h"
#include "Waffle/Renderer/Shader.h"

namespace Waffle {

	// -------------------------------------------------------------------------
	// GraphicsPipeline
	// An explicit pipeline: shader + topology + fixed render state.
	// Replaces the old implicit model where the backend resolved a pipeline
	// from whatever shader / vertex array / render target happened to be
	// "bound" in global state.
	//   - Vulkan: front-loads shader & layout handles; the format-dependent
	//     VkPipeline is resolved from the pipeline's internal cache at first
	//     use against a render target (see VulkanGraphicsPipeline::Resolve).
	//   - OpenGL: applies the program and fixed-function state on bind.
	// -------------------------------------------------------------------------
	class GraphicsPipeline : public RefCounted
	{
	public:
		enum class Topology { Triangles, Lines };

		struct Desc
		{
			enum class BlendMode
			{
				Opaque,    // blending disabled
				SrcAlpha,  // standard premultiplied-off alpha blending
				Additive,  // GL_ONE / GL_ONE (bloom upsample)
			};

			Ref<Shader> Shader;
			Topology    Topology   = Topology::Triangles;
			BlendMode   Blending   = BlendMode::SrcAlpha;
			bool        DepthTest  = true;
			bool        DepthWrite = true;
		};

		static Ref<GraphicsPipeline> Create(const Desc& desc);

		using BlendMode = Desc::BlendMode;

		virtual ~GraphicsPipeline() = default;

		const Desc& GetDescription() const { return m_Desc; }
		const Ref<Shader>& GetShader() const { return m_Desc.Shader; }

	protected:
		GraphicsPipeline(const Desc& desc) : m_Desc(desc) {}
		Desc m_Desc;
	};

}
