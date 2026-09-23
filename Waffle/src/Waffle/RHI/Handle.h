#pragma once

#include <cstdint>
#include <functional>

namespace Waffle::RHI {

	// -------------------------------------------------------------------------
	// Typed, backend-agnostic resource handles.
	//
	// Raw native handles (GLuones fit in 32 bits; VkBuffer/VkImage/VkDescriptorSet
	// are 64-bit pointers) must never cross the backend boundary - the original
	// "IDs break between OpenGL and Vulkan" bug was exactly such a crossing
	// (a VkDescriptorSet pointer round-tripped through uint32_t). Handles are
	// index+generation pairs into a backend-owned resource table, so:
	//   - no native handle size leaks into public APIs,
	//   - truncation is unrepresentable,
	//   - a stale handle to a destroyed/recreated resource is detected by
	//     generation mismatch instead of silently aliasing a new resource.
	// -------------------------------------------------------------------------
	template<typename Tag>
	struct Handle
	{
		uint32_t Index = 0;
		uint32_t Generation = 0;

		constexpr bool IsNull() const { return Index == 0 && Generation == 0; }
		constexpr uint64_t Packed() const { return (uint64_t(Index) << 32) | uint64_t(Generation); }
	};

	template<typename Tag>
	constexpr bool operator==(Handle<Tag> a, Handle<Tag> b) { return a.Packed() == b.Packed(); }
	template<typename Tag>
	constexpr bool operator!=(Handle<Tag> a, Handle<Tag> b) { return !(a == b); }

	struct BufferTag;      using BufferHandle      = Handle<BufferTag>;
	struct TextureTag;     using TextureHandle     = Handle<TextureTag>;
	struct ShaderTag;      using ShaderHandle      = Handle<ShaderTag>;
	struct PipelineTag;    using PipelineHandle    = Handle<PipelineTag>;
	struct FramebufferTag; using FramebufferHandle = Handle<FramebufferTag>;
	struct SamplerTag;     using SamplerHandle     = Handle<SamplerTag>;

	// Index 0 / generation 0 is the null handle; real slots start at 1.
	constexpr uint32_t kNullHandleIndex = 0;
}

template<typename Tag>
struct std::hash<Waffle::RHI::Handle<Tag>>
{
	size_t operator()(Waffle::RHI::Handle<Tag> h) const
	{
		return std::hash<uint64_t>{}(h.Packed());
	}
};
