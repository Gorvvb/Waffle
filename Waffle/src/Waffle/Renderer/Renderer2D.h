#pragma once

#include "OrthographicCamera.h"

#include "Texture.h"
#include "SubTexture2D.h"

#include "Waffle/Renderer/Camera.h"
#include "Waffle/Renderer/EditorCamera.h"

#include "Waffle/Scene/Components.h"

#include "Waffle/Renderer/Frustum2D.h"

namespace Waffle {

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const OrthographicCamera& camera); // TODO: REMOVE
		static void EndScene();
		static void Flush();

		static const Frustum2D& GetFrustum();
		static bool IsVisibleInFrustum(const AABB2D& bounds);
		static bool IsVisibleInFrustum(const glm::vec2& position, const glm::vec2& size);
		// Z-aware overloads: with a perspective camera the visible XY region
		// depends on Z, so culling must test the object's actual depth.
		static bool IsVisibleInFrustum(const glm::vec3& min, const glm::vec3& max);
		static bool IsVisibleInFrustum(const glm::vec3& center, const glm::vec2& size);

		static void DrawLine(const glm::vec3& p0, glm::vec3& p1, const glm::vec4& color, int entityID = -1);

		static float GetLineWidth();
		static void SetLineWidth(float width);

	static void DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID = -1);
	static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f, int entityID = -1);

	static void DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID = -1);
	static void DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
	static void DrawRoundedRect(const glm::mat4& transform, const glm::vec4& color, float cornerRadius = 0.06f, int cornerSegments = 4, int entityID = -1);

	static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
	static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f));
	static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f));

	static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
	static void DrawRoundedQuad(const glm::mat4& transform, const glm::vec4& color, float cornerRadius = 0.04f, int cornerSegments = 4, int entityID = -1);
	static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1, SpriteAspectMode aspectMode = SpriteAspectMode::Stretch);
	static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subTexture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1, SpriteAspectMode aspectMode = SpriteAspectMode::Stretch, const glm::vec2& framePivot = glm::vec2(0.5f, 0.5f), const glm::vec2& referencePixelSize = glm::vec2(0.0f), const glm::vec4* contentFrac = nullptr);

	// Bakes one textured quad per glyph into the sprite batch. penPosition is
	// the start of the baseline in the current camera space (UI space: Y down).
	static void DrawString(const std::string& text, const Ref<Font>& font, const glm::vec2& penPosition, float scale, const glm::vec4& color, int entityID = -1);

		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color, int entityID = -1);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), int entityID = -1);

		// Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;
			uint32_t CulledQuadCount = 0;
			uint32_t LineVertexCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4 + LineVertexCount; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};

		static void ResetStats();
		static Statistics& GetStats();

	private:
		static void StartBatch();
		static void NextBatch();
	};
}