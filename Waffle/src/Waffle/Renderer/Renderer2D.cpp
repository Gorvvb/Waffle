#include "wfpch.h"
#include "Renderer2D.h"

#include "Waffle/Core/Log.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Waffle/Renderer/UniformBuffer.h"
#include "RenderCommand.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	namespace
	{
		// Width/height ratio of the quad described by a transform (lengths of
		// its local X/Y axes).
		float QuadAspectOf(const glm::mat4& transform)
		{
			glm::vec2 scale(glm::length(transform[0]), glm::length(transform[1]));
			return (scale.y != 0.0f) ? (scale.x / scale.y) : 0.0f;
		}

		// Scales the quad's local X/Y axes so the drawn rectangle keeps the
		// sprite's native aspect ratio, letterboxed (centered) inside the
		// original quad.
		glm::mat4 FitTransformToAspect(const glm::mat4& transform, float spriteAspect)
		{
			glm::vec2 scale(glm::length(transform[0]), glm::length(transform[1]));
			if (scale.x <= 0.0f || scale.y <= 0.0f || spriteAspect <= 0.0f)
				return transform;

			float quadAspect = scale.x / scale.y;
			float sx = 1.0f, sy = 1.0f;
			if (spriteAspect > quadAspect)
				sy = quadAspect / spriteAspect;   // sprite wider: shrink height
			else
				sx = spriteAspect / quadAspect;   // sprite taller: shrink width
			return transform * glm::scale(glm::mat4(1.0f), glm::vec3(sx, sy, 1.0f));
		}

		// Crops the [uvMin, uvMax] window (centered) so the sampled region
		// has the quad's aspect ratio - the "cover" behaviour.
		void CropUVsToAspect(glm::vec2& uvMin, glm::vec2& uvMax, float spriteAspect, float quadAspect)
		{
			if (spriteAspect <= 0.0f || quadAspect <= 0.0f)
				return;

			if (spriteAspect > quadAspect)
			{
				float cut = (1.0f - quadAspect / spriteAspect) * 0.5f;
				float w = uvMax.x - uvMin.x;
				uvMin.x += w * cut;
				uvMax.x -= w * cut;
			}
			else
			{
				float cut = (1.0f - spriteAspect / quadAspect) * 0.5f;
				float h = uvMax.y - uvMin.y;
				uvMin.y += h * cut;
				uvMax.y -= h * cut;
			}
		}
	}

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float textureIndex;
		glm::vec2 TilingFactor;

		// Editor only
		int EntityID;
	};

	struct CircleVertex
	{
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;

		// Editor-only
		int EntityID;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;

		// Editor-only
		int EntityID;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;

		// Hard upper bound (TextureSlots array capacity). The usable count is
		// clamped to the device limit in Init() - some GPUs expose only 16
		// texture image units and a u_Textures[32] array fails to link there.
		static const uint32_t MaxTextureSlotCapacity = 32;
		uint32_t MaxTextureSlots = MaxTextureSlotCapacity;

		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> QuadShader;
		Ref<Texture2D> WhiteTexture;

		Ref<VertexArray> CircleVertexArray;
		Ref<VertexBuffer> CircleVertexBuffer;
		Ref<Shader> CircleShader;

		Ref<VertexArray> LineVertexArray;
		Ref<VertexBuffer> LineVertexBuffer;
		Ref<Shader> LineShader;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		uint32_t CircleIndexCount = 0;
		CircleVertex* CircleVertexBufferBase = nullptr;
		CircleVertex* CircleVertexBufferPtr = nullptr;

		uint32_t LineVertexCount = 0;
		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;

		float LineWidth = 2.0f;

		std::array<Ref<Texture2D>, MaxTextureSlotCapacity> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = white texture;
		
		glm::vec4 QuadVertexPositions[4];

		enum class PrimitiveType { None = 0, Quad, Circle, Line };

		struct RenderCommandEntry
		{
			PrimitiveType Type = PrimitiveType::None;
			uint32_t ElementOffset = 0;
			uint32_t ElementCount = 0;
			uint32_t TextureCount = 0;
		};

		std::vector<RenderCommandEntry> Commands;

		Renderer2D::Statistics Stats;
		Frustum2D ActiveFrustum;

		struct CameraData
		{
			glm::mat4 ViewProjection;
		};
		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;
	};

	static Renderer2DData s_Data;

	static void RecordQuadCommand(uint32_t indexCount)
	{
		if (s_Data.Commands.empty() || s_Data.Commands.back().Type != Renderer2DData::PrimitiveType::Quad)
		{
			s_Data.Commands.push_back({ Renderer2DData::PrimitiveType::Quad, s_Data.QuadIndexCount - indexCount, 0, s_Data.TextureSlotIndex });
		}
		s_Data.Commands.back().ElementCount += indexCount;
		s_Data.Commands.back().TextureCount = s_Data.TextureSlotIndex;
	}

	static void RecordCircleCommand(uint32_t indexCount)
	{
		if (s_Data.Commands.empty() || s_Data.Commands.back().Type != Renderer2DData::PrimitiveType::Circle)
		{
			s_Data.Commands.push_back({ Renderer2DData::PrimitiveType::Circle, s_Data.CircleIndexCount - indexCount, 0, 0 });
		}
		s_Data.Commands.back().ElementCount += indexCount;
	}

	static void RecordLineCommand(uint32_t vertexCount)
	{
		if (s_Data.Commands.empty() || s_Data.Commands.back().Type != Renderer2DData::PrimitiveType::Line)
		{
			s_Data.Commands.push_back({ Renderer2DData::PrimitiveType::Line, s_Data.LineVertexCount - vertexCount, 0, 0 });
		}
		s_Data.Commands.back().ElementCount += vertexCount;
	}

	void Renderer2D::Init()
	{
		WF_PROFILE_FUNCTION();

		// Quads
		s_Data.QuadVertexArray = VertexArray::Create();

		s_Data.QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
		s_Data.QuadVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"		},
			{ ShaderDataType::Float4, "a_Color"			},
			{ ShaderDataType::Float2, "a_TexCoord"		},
			{ ShaderDataType::Float,  "a_textureIndex"	},
			{ ShaderDataType::Float2, "a_TilingFactor"	},
			{ ShaderDataType::Int,    "a_EntityID"		}
		});
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);

		s_Data.QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];

		uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIB);
		delete[] quadIndices;

		// Circles
		s_Data.CircleVertexArray = VertexArray::Create();

		s_Data.CircleVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(CircleVertex));
		s_Data.CircleVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_WorldPosition" },
			{ ShaderDataType::Float3, "a_LocalPosition" },
			{ ShaderDataType::Float4, "a_Color"         },
			{ ShaderDataType::Float,  "a_Thickness"     },
			{ ShaderDataType::Float,  "a_Fade"          },
			{ ShaderDataType::Int,    "a_EntityID"      }
			});
		s_Data.CircleVertexArray->AddVertexBuffer(s_Data.CircleVertexBuffer);
		s_Data.CircleVertexArray->SetIndexBuffer(quadIB);
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];

		// Lines
		s_Data.LineVertexArray = VertexArray::Create();

		s_Data.LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
		s_Data.LineVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Int,    "a_EntityID" }
			});
		s_Data.LineVertexArray->AddVertexBuffer(s_Data.LineVertexBuffer);
		s_Data.LineVertexBufferBase = new LineVertex[s_Data.MaxVertices];

		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		// Clamp the batcher to this device's sampler limit and publish it as
		// WF_MAX_TEXTURE_SLOTS BEFORE creating shaders, so u_Textures[] is
		// declared with a size this GPU can actually link.
		uint32_t deviceSlots = RenderCommand::GetMaxTextureSlots();
		s_Data.MaxTextureSlots = (deviceSlots < Renderer2DData::MaxTextureSlotCapacity)
			? deviceSlots : Renderer2DData::MaxTextureSlotCapacity;
		Shader::AddGlobalDefine("WF_MAX_TEXTURE_SLOTS", std::to_string(s_Data.MaxTextureSlots));
		if (s_Data.MaxTextureSlots < Renderer2DData::MaxTextureSlotCapacity)
			WF_CORE_WARN("Renderer2D: device supports only {0} texture units; batching clamped accordingly.", s_Data.MaxTextureSlots);

		std::vector<int32_t> samplers(s_Data.MaxTextureSlots);
		for (int32_t i = 0; i < (int32_t)s_Data.MaxTextureSlots; i++)
			samplers[i] = i;

		s_Data.QuadShader = Shader::Create("assets/shaders/2DQuadShader.glsl");
		s_Data.QuadShader->Bind();
		s_Data.QuadShader->SetIntArray("u_Textures", samplers.data(), s_Data.MaxTextureSlots);

		s_Data.CircleShader = Shader::Create("assets/shaders/2DCircleShader.glsl");
		s_Data.LineShader = Shader::Create("assets/shaders/2DLineShader.glsl");
		
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBufferBase;
		delete[] s_Data.CircleVertexBufferBase;
		delete[] s_Data.LineVertexBufferBase;
		s_Data.QuadVertexBufferBase = nullptr;
		s_Data.CircleVertexBufferBase = nullptr;
		s_Data.LineVertexBufferBase = nullptr;
		s_Data.QuadVertexBufferPtr = nullptr;
		s_Data.CircleVertexBufferPtr = nullptr;
		s_Data.LineVertexBufferPtr = nullptr;

		// Release GPU objects here, while the graphics context is still alive.
		// The static s_Data Refs would otherwise destruct after main() when the
		// context is already gone.
		s_Data.QuadVertexArray = nullptr;
		s_Data.QuadVertexBuffer = nullptr;
		s_Data.QuadShader = nullptr;
		s_Data.WhiteTexture = nullptr;

		s_Data.CircleVertexArray = nullptr;
		s_Data.CircleVertexBuffer = nullptr;
		s_Data.CircleShader = nullptr;

		s_Data.LineVertexArray = nullptr;
		s_Data.LineVertexBuffer = nullptr;
		s_Data.LineShader = nullptr;

		s_Data.CameraUniformBuffer = nullptr;
		for (auto& slot : s_Data.TextureSlots)
			slot = nullptr;
		s_Data.Commands.clear();
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		WF_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		s_Data.ActiveFrustum = Frustum2D::FromProjectionAndView(camera.GetProjectionMatrix(), camera.GetViewMatrix());

		StartBatch();
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		WF_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		s_Data.ActiveFrustum = Frustum2D::FromProjectionAndView(camera.GetProjection(), glm::inverse(transform));

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		WF_PROFILE_FUNCTION();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
		s_Data.ActiveFrustum = Frustum2D::FromProjectionAndView(camera.GetProjection(), camera.GetViewMatrix());

		StartBatch();
	}

	const Frustum2D& Renderer2D::GetFrustum()
	{
		return s_Data.ActiveFrustum;
	}

	bool Renderer2D::IsVisibleInFrustum(const AABB2D& bounds)
	{
		return s_Data.ActiveFrustum.IsVisible(bounds);
	}

	bool Renderer2D::IsVisibleInFrustum(const glm::vec2& position, const glm::vec2& size)
	{
		return s_Data.ActiveFrustum.IsVisible(position, size);
	}


	// Z-aware culling: with a perspective camera the visible XY region
	// tapers with depth, so the object's actual Z must be tested.
	bool Renderer2D::IsVisibleInFrustum(const glm::vec3& min, const glm::vec3& max)
	{
		return s_Data.ActiveFrustum.IsVisible(min, max);
	}

	bool Renderer2D::IsVisibleInFrustum(const glm::vec3& center, const glm::vec2& size)
	{
		glm::vec2 halfSize = size * 0.5f;
		return s_Data.ActiveFrustum.IsVisible(
			glm::vec3(center.x - halfSize.x, center.y - halfSize.y, center.z),
			glm::vec3(center.x + halfSize.x, center.y + halfSize.y, center.z));
	}

	void Renderer2D::EndScene()
	{
		WF_PROFILE_FUNCTION();
		Flush();
	}

	void Renderer2D::Flush()
	{
		if (s_Data.Commands.empty())
			return;

		if (s_Data.QuadIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);
		}
		if (s_Data.CircleIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase);
			s_Data.CircleVertexBuffer->SetData(s_Data.CircleVertexBufferBase, dataSize);
		}
		if (s_Data.LineVertexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.LineVertexBufferPtr - (uint8_t*)s_Data.LineVertexBufferBase);
			s_Data.LineVertexBuffer->SetData(s_Data.LineVertexBufferBase, dataSize);
		}

		for (const auto& cmd : s_Data.Commands)
		{
			if (cmd.ElementCount == 0) continue;

			switch (cmd.Type)
			{
			case Renderer2DData::PrimitiveType::Quad:
			{
				for (uint32_t i = 0; i < cmd.TextureCount; i++)
					s_Data.TextureSlots[i]->Bind(i);

				s_Data.QuadShader->Bind();
				RenderCommand::DrawIndexed(s_Data.QuadVertexArray, cmd.ElementCount, cmd.ElementOffset);
				s_Data.Stats.DrawCalls++;
				break;
			}
			case Renderer2DData::PrimitiveType::Circle:
			{
				s_Data.CircleShader->Bind();
				RenderCommand::DrawIndexed(s_Data.CircleVertexArray, cmd.ElementCount, cmd.ElementOffset);
				s_Data.Stats.DrawCalls++;
				break;
			}
			case Renderer2DData::PrimitiveType::Line:
			{
				s_Data.LineShader->Bind();
				RenderCommand::SetLineWidth(s_Data.LineWidth);
				RenderCommand::DrawLines(s_Data.LineVertexArray, cmd.ElementCount, cmd.ElementOffset);
				s_Data.Stats.DrawCalls++;
				break;
			}
			default:
				break;
			}
		}
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, glm::vec3& p1, const glm::vec4& color, int entityID)
	{
		if (s_Data.LineVertexCount >= s_Data.MaxVertices)
			NextBatch();

		s_Data.LineVertexBufferPtr->Position = p0;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;

		s_Data.LineVertexBufferPtr->Position = p1;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;

		s_Data.LineVertexCount += 2;
		s_Data.Stats.LineVertexCount += 2;
		RecordLineCommand(2);
	}

	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID)
	{
		glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
		glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

		DrawLine(p0, p1, color, entityID);
		DrawLine(p1, p2, color, entityID);
		DrawLine(p2, p3, color, entityID);
		DrawLine(p3, p0, color, entityID);
	}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		glm::vec3 lineVertices[4];
		for (size_t i = 0; i < 4; i++)
			lineVertices[i] = transform * s_Data.QuadVertexPositions[i];

		DrawLine(lineVertices[0], lineVertices[1], color, entityID);
		DrawLine(lineVertices[1], lineVertices[2], color, entityID);
		DrawLine(lineVertices[2], lineVertices[3], color, entityID);
		DrawLine(lineVertices[3], lineVertices[0], color, entityID);
	}

	void Renderer2D::DrawRoundedRect(const glm::mat4& transform, const glm::vec4& color, float cornerRadius, int cornerSegments, int entityID)
	{
		WF_PROFILE_FUNCTION();

		cornerRadius = glm::clamp(cornerRadius, 0.0f, 0.45f);
		if (cornerRadius <= 0.001f)
		{
			DrawRect(transform, color, entityID);
			return;
		}

		float r = cornerRadius;
		float xMin = -0.5f + r, xMax = 0.5f - r;
		float yMin = -0.5f + r, yMax = 0.5f - r;

		glm::vec2 centers[4] = {
			{ xMax, yMin },
			{ xMax, yMax },
			{ xMin, yMax },
			{ xMin, yMin }
		};

		float startAngles[4] = {
			-glm::half_pi<float>(),
			0.0f,
			glm::half_pi<float>(),
			glm::pi<float>()
		};

		// Reused scratch buffer - this runs per sprite per frame and must not heap-allocate.
		static std::vector<glm::vec3> points;
		points.clear();
		points.reserve(4 * ((size_t)cornerSegments + 1));

		for (int i = 0; i < 4; i++)
		{
			float baseAngle = startAngles[i];
			float step = glm::half_pi<float>() / (float)cornerSegments;
			for (int j = 0; j <= cornerSegments; j++)
			{
				float angle = baseAngle + (float)j * step;
				glm::vec2 pos = centers[i] + glm::vec2(cosf(angle), sinf(angle)) * r;
				points.push_back(transform * glm::vec4(pos.x, pos.y, 0.0f, 1.0f));
			}
		}

		size_t count = points.size();
		for (size_t i = 0; i < count; i++)
		{
			glm::vec3 p0 = points[i];
			glm::vec3 p1 = points[(i + 1) % count];
			DrawLine(p0, p1, color, entityID);
		}
	}

	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID)
	{
		if (src.Texture)
		{
			DrawQuad(transform, src.Texture, src.TilingFactor, src.Color, entityID, src.AspectMode);
		}
		else
			DrawQuad(transform, src.Color, entityID);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness /*= 1.0f*/, float fade /*= 0.005f*/, int entityID /*= -1*/)
	{
		WF_PROFILE_FUNCTION();

		glm::vec3 minPt( 1e9f);
		glm::vec3 maxPt(-1e9f);
		for (size_t i = 0; i < 4; i++)
		{
			glm::vec4 worldPos = transform * s_Data.QuadVertexPositions[i];
			minPt.x = glm::min(minPt.x, worldPos.x);
			minPt.y = glm::min(minPt.y, worldPos.y);			minPt.z = glm::min(minPt.z, worldPos.z);
			maxPt.x = glm::max(maxPt.x, worldPos.x);
			maxPt.y = glm::max(maxPt.y, worldPos.y);			maxPt.z = glm::max(maxPt.z, worldPos.z);
		}

		if (!IsVisibleInFrustum(minPt, maxPt))
		{
			s_Data.Stats.CulledQuadCount++;
			return;
		}

		if (s_Data.CircleIndexCount >= s_Data.MaxIndices)
			NextBatch();

		for (size_t i = 0; i < 4; i++)
		{
			s_Data.CircleVertexBufferPtr->WorldPosition = transform * s_Data.QuadVertexPositions[i];
			s_Data.CircleVertexBufferPtr->LocalPosition = s_Data.QuadVertexPositions[i] * 2.0f;
			s_Data.CircleVertexBufferPtr->Color = color;
			s_Data.CircleVertexBufferPtr->Thickness = thickness;
			s_Data.CircleVertexBufferPtr->Fade = fade;
			s_Data.CircleVertexBufferPtr->EntityID = entityID;
			s_Data.CircleVertexBufferPtr++;
		}

		s_Data.CircleIndexCount += 6;
		RecordCircleCommand(6);

		s_Data.Stats.QuadCount++;
	}

	float Renderer2D::GetLineWidth()
	{
		return s_Data.LineWidth;
	}

	void Renderer2D::SetLineWidth(float width)
	{
		s_Data.LineWidth = width;
	}

	void Renderer2D::StartBatch()
	{
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;

		s_Data.CircleIndexCount = 0;
		s_Data.CircleVertexBufferPtr = s_Data.CircleVertexBufferBase;

		s_Data.LineVertexCount = 0;
		s_Data.LineVertexBufferPtr = s_Data.LineVertexBufferBase;

		s_Data.TextureSlotIndex = 1;
		s_Data.Commands.clear();
	}

	void Renderer2D::NextBatch()
	{
		Flush();
		StartBatch();
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor, const glm::vec4& tintColor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor, const glm::vec4& tintColor)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		WF_PROFILE_FUNCTION();

		glm::vec3 minPt( 1e9f);
		glm::vec3 maxPt(-1e9f);
		for (size_t i = 0; i < 4; i++)
		{
			glm::vec4 worldPos = transform * s_Data.QuadVertexPositions[i];
			minPt.x = glm::min(minPt.x, worldPos.x);
			minPt.y = glm::min(minPt.y, worldPos.y);			minPt.z = glm::min(minPt.z, worldPos.z);
			maxPt.x = glm::max(maxPt.x, worldPos.x);
			maxPt.y = glm::max(maxPt.y, worldPos.y);			maxPt.z = glm::max(maxPt.z, worldPos.z);
		}

		if (!IsVisibleInFrustum(minPt, maxPt))
		{
			s_Data.Stats.CulledQuadCount++;
			return;
		}

		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		const float textureIndex = 0.0f; // White texture
		const glm::vec2 tilingFactor = glm::vec2(1.0f, 1.0f);

		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		RecordQuadCommand(6);

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawRoundedQuad(const glm::mat4& transform, const glm::vec4& color, float cornerRadius, int cornerSegments, int entityID)
	{
		WF_PROFILE_FUNCTION();

		cornerRadius = glm::clamp(cornerRadius, 0.0f, 0.45f);
		if (cornerRadius <= 0.001f)
		{
			DrawQuad(transform, color, entityID);
			return;
		}

		float r = cornerRadius;
		float xMin = -0.5f + r, xMax = 0.5f - r;
		float yMin = -0.5f + r, yMax = 0.5f - r;

		glm::vec2 centers[4] = {
			{ xMax, yMin },
			{ xMax, yMax },
			{ xMin, yMax },
			{ xMin, yMin }
		};

		float startAngles[4] = {
			-glm::half_pi<float>(),
			0.0f,
			glm::half_pi<float>(),
			glm::pi<float>()
		};

		// Reused scratch buffer - this runs per sprite per frame and must not heap-allocate.
		static std::vector<glm::vec3> points;
		points.clear();
		points.reserve(4 * ((size_t)cornerSegments + 1));

		for (int i = 0; i < 4; i++)
		{
			float baseAngle = startAngles[i];
			float step = glm::half_pi<float>() / (float)cornerSegments;
			for (int j = 0; j <= cornerSegments; j++)
			{
				float angle = baseAngle + (float)j * step;
				glm::vec2 pos = centers[i] + glm::vec2(cosf(angle), sinf(angle)) * r;
				points.push_back(transform * glm::vec4(pos.x, pos.y, 0.0f, 1.0f));
			}
		}

		glm::vec3 center = transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const float textureIndex = 0.0f;
		const glm::vec2 tilingFactor = glm::vec2(1.0f, 1.0f);

		size_t count = points.size();
		for (size_t i = 0; i < count; i++)
		{
			if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
				NextBatch();

			glm::vec3 p0 = points[i];
			glm::vec3 p1 = points[(i + 1) % count];

			s_Data.QuadVertexBufferPtr->Position = center;
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = { 0.5f, 0.5f };
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;

			s_Data.QuadVertexBufferPtr->Position = center;
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = { 0.5f, 0.5f };
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;

			s_Data.QuadVertexBufferPtr->Position = p0;
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = { 0.0f, 0.0f };
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;

			s_Data.QuadVertexBufferPtr->Position = p1;
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = { 1.0f, 1.0f };
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;

			s_Data.QuadIndexCount += 6;
			RecordQuadCommand(6);
		}
		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transformIn, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor, const glm::vec4& tintColor, int entityID, SpriteAspectMode aspectMode)
	{
		WF_PROFILE_FUNCTION();

		if (!texture)
		{
			DrawQuad(transformIn, tintColor, entityID);
			return;
		}

		glm::mat4 transform = transformIn;
		glm::vec2 texCoordsOverride[4];
		const glm::vec2* texCoords = nullptr;

		if (aspectMode != SpriteAspectMode::Stretch && texture->GetWidth() > 0 && texture->GetHeight() > 0)
		{
			float spriteAspect = (float)texture->GetWidth() / (float)texture->GetHeight();
			float quadAspect = QuadAspectOf(transformIn);

			if (aspectMode == SpriteAspectMode::Fit)
			{
				transform = FitTransformToAspect(transformIn, spriteAspect);
			}
			else // Fill: crop the UV window, keep the quad
			{
				glm::vec2 uvMin(0.0f, 0.0f), uvMax(1.0f, 1.0f);
				CropUVsToAspect(uvMin, uvMax, spriteAspect, quadAspect);
				texCoordsOverride[0] = { uvMin.x, uvMin.y };
				texCoordsOverride[1] = { uvMax.x, uvMin.y };
				texCoordsOverride[2] = { uvMax.x, uvMax.y };
				texCoordsOverride[3] = { uvMin.x, uvMax.y };
				texCoords = texCoordsOverride;
			}
		}

		glm::vec3 minPt( 1e9f);
		glm::vec3 maxPt(-1e9f);
		for (size_t i = 0; i < 4; i++)
		{
			glm::vec4 worldPos = transform * s_Data.QuadVertexPositions[i];
			minPt.x = glm::min(minPt.x, worldPos.x);
			minPt.y = glm::min(minPt.y, worldPos.y);			minPt.z = glm::min(minPt.z, worldPos.z);
			maxPt.x = glm::max(maxPt.x, worldPos.x);
			maxPt.y = glm::max(maxPt.y, worldPos.y);			maxPt.z = glm::max(maxPt.z, worldPos.z);
		}

		if (!IsVisibleInFrustum(minPt, maxPt))
		{
			s_Data.Stats.CulledQuadCount++;
			return;
		}

		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 defaultTextureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		if (!texCoords)
			texCoords = defaultTextureCoords;

		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;

		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
		{
			if (*s_Data.TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		RecordQuadCommand(6);

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color, int entityID)
	{
		WF_PROFILE_FUNCTION();

		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		const float textureIndex = 0.0f; // White texture
		const glm::vec2 tilingFactor = glm::vec2(1.0f, 1.0f);

		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		RecordQuadCommand(6);

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor, const glm::vec4& tintColor, int entityID)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor, entityID);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, const glm::vec2& tilingFactor, const glm::vec4& tintColor, int entityID)
	{
		WF_PROFILE_FUNCTION();

		if (!texture)
		{
			DrawRotatedQuad(position, size, rotation, tintColor, entityID);
			return;
		}

		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		// Same conservative-AABB culling as every other transform-based
		// draw - rotated quads previously always entered the batch.
		{
			glm::vec3 minPt(1e9f), maxPt(-1e9f);
			for (size_t i = 0; i < 4; i++)
			{
				glm::vec4 worldPos = transform * s_Data.QuadVertexPositions[i];
				minPt.x = glm::min(minPt.x, worldPos.x);
				minPt.y = glm::min(minPt.y, worldPos.y);				minPt.z = glm::min(minPt.z, worldPos.z);
				maxPt.x = glm::max(maxPt.x, worldPos.x);
				maxPt.y = glm::max(maxPt.y, worldPos.y);				maxPt.z = glm::max(maxPt.z, worldPos.z);
			}
			if (!IsVisibleInFrustum(minPt, maxPt))
			{
				s_Data.Stats.CulledQuadCount++;
				return;
			}
		}

		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;

		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
		{
			if (*s_Data.TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		RecordQuadCommand(6);

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transformIn, const Ref<SubTexture2D>& subTexture, const glm::vec2& tilingFactor, const glm::vec4& tintColor, int entityID, SpriteAspectMode aspectMode, const glm::vec2& framePivot, const glm::vec2& referencePixelSize, const glm::vec4* contentFrac)
	{
		WF_PROFILE_FUNCTION();

		if (!subTexture || !subTexture->GetTexture()) return;

		constexpr size_t quadVertexCount = 4;
		const glm::vec2* textureCoords = subTexture->GetTexCoords();
		const Ref<Texture2D>& texture = subTexture->GetTexture();

		glm::mat4 transform = transformIn;
		glm::vec2 texCoordsOverride[4];

		// Frame-native aspect from the sub-texture's UV window - this is what
		// keeps animation frames with different sizes from distorting.
		if (aspectMode != SpriteAspectMode::Stretch && texture->GetWidth() > 0 && texture->GetHeight() > 0)
		{
			glm::vec2 uvMin = textureCoords[0];
			glm::vec2 uvMax = textureCoords[2];
			float spriteW = (float)texture->GetWidth() * (uvMax.x - uvMin.x);
			float spriteH = (float)texture->GetHeight() * (uvMax.y - uvMin.y);
			if (spriteW > 0.0f && spriteH > 0.0f)
			{
				float spriteAspect = spriteW / spriteH;
				float quadAspect = QuadAspectOf(transformIn);

				if (aspectMode == SpriteAspectMode::Fit)
				{
					// Normalized content fit: every frame shares the
					// pixels-per-unit of the animator's LARGEST visible
					// content (referencePixelSize; falls back to this frame
					// when unknown), so differently sized frames keep one
					// consistent scale instead of each filling the quad.
					// Frames are anchored by their opaque CONTENT, not their
					// crop rect: the pivot point within the content box
					// lands on the matching pivot point of the quad, so
					// tighter/looser crops don't make the art glide.
					glm::vec2 quadSize(glm::length(transformIn[0]), glm::length(transformIn[1]));
					glm::vec2 refSize = (referencePixelSize.x > 0.0f && referencePixelSize.y > 0.0f)
						? referencePixelSize
						: glm::vec2(spriteW, spriteH);

					// Anchor fraction within the frame: the pivot point of
					// the content box, or just framePivot when no content
					// rect is known (content == whole frame). Clamped so a
					// stray fraction degrades gracefully instead of
					// switching the frame to a different alignment mode.
					glm::vec2 anchorFrac = framePivot;
					if (contentFrac)
					{
						glm::vec2 cMin(glm::clamp(contentFrac->x, 0.0f, 1.0f),
							glm::clamp(contentFrac->y, 0.0f, 1.0f));
						glm::vec2 cMax(glm::clamp(contentFrac->z, 0.0f, 1.0f),
							glm::clamp(contentFrac->w, 0.0f, 1.0f));
						glm::vec2 cSize = cMax - cMin;
						if (cSize.x > 1e-4f && cSize.y > 1e-4f)
							anchorFrac = cMin + framePivot * cSize;
					}

					if (quadSize.x > 0.0f && quadSize.y > 0.0f)
					{
						float pxPerUnit = std::min(quadSize.x / refSize.x, quadSize.y / refSize.y);
						glm::vec2 s(spriteW * pxPerUnit / quadSize.x, spriteH * pxPerUnit / quadSize.y);
						glm::vec2 c = (framePivot - glm::vec2(0.5f)) + s * (glm::vec2(0.5f) - anchorFrac);
						transform = transformIn
							* glm::translate(glm::mat4(1.0f), glm::vec3(c, 0.0f))
							* glm::scale(glm::mat4(1.0f), glm::vec3(s, 1.0f));
					}
				}
				else // Fill
				{
					glm::vec2 croppedMin = uvMin, croppedMax = uvMax;
					CropUVsToAspect(croppedMin, croppedMax, spriteAspect, quadAspect);
					texCoordsOverride[0] = { croppedMin.x, croppedMin.y };
					texCoordsOverride[1] = { croppedMax.x, croppedMin.y };
					texCoordsOverride[2] = { croppedMax.x, croppedMax.y };
					texCoordsOverride[3] = { croppedMin.x, croppedMax.y };
					textureCoords = texCoordsOverride;
				}
			}
		}

		glm::vec3 minPt( 1e9f);
		glm::vec3 maxPt(-1e9f);
		for (size_t i = 0; i < 4; i++)
		{
			glm::vec4 worldPos = transform * s_Data.QuadVertexPositions[i];
			minPt.x = glm::min(minPt.x, worldPos.x);
			minPt.y = glm::min(minPt.y, worldPos.y);			minPt.z = glm::min(minPt.z, worldPos.z);
			maxPt.x = glm::max(maxPt.x, worldPos.x);
			maxPt.y = glm::max(maxPt.y, worldPos.y);			maxPt.z = glm::max(maxPt.z, worldPos.z);
		}

		if (!IsVisibleInFrustum(minPt, maxPt))
		{
			s_Data.Stats.CulledQuadCount++;
			return;
		}

		if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;

		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
		{
			if (*s_Data.TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		RecordQuadCommand(6);
		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawString(const std::string& text, const Ref<Font>& font, const glm::vec2& penPosition, float scale, const glm::vec4& color, int entityID)
	{
		if (!font || !font->GetAtlas())
			return;

		const Ref<Texture2D>& atlas = font->GetAtlas();
		const glm::vec2 tiling(1.0f, 1.0f);
		float penX = penPosition.x;
		const float baseline = penPosition.y;

		// Resolved lazily per glyph: a NextBatch() inside the loop clears the
		// texture slots, so the atlas index must be looked up again after it.
		auto findOrAddAtlasSlot = [&atlas]() -> float
		{
			for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
			{
				if (*s_Data.TextureSlots[i].get() == *atlas.get())
					return (float)i;
			}

			if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
				NextBatch();

			float index = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = atlas;
			s_Data.TextureSlotIndex++;
			return index;
		};

		for (char c : text)
		{
			const Font::Glyph* g = font->GetGlyph(c);
			if (!g)
				continue;

			bool visible = (c != ' ') && (g->X1 > g->X0) && (g->Y1 > g->Y0);
			if (visible)
			{
				if (s_Data.QuadIndexCount >= s_Data.MaxIndices)
					NextBatch();

				float textureIndex = findOrAddAtlasSlot();

				const float x0 = penX + g->X0 * scale;
				const float x1 = penX + g->X1 * scale;
				const float y0 = baseline + g->Y0 * scale;
				const float y1 = baseline + g->Y1 * scale;

				const glm::vec2 corners[4] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
				const glm::vec2 uvs[4] = { { g->U0, g->V0 }, { g->U1, g->V0 }, { g->U1, g->V1 }, { g->U0, g->V1 } };

				for (int i = 0; i < 4; i++)
				{
					s_Data.QuadVertexBufferPtr->Position = glm::vec3(corners[i].x, corners[i].y, 0.0f);
					s_Data.QuadVertexBufferPtr->Color = color;
					s_Data.QuadVertexBufferPtr->TexCoord = uvs[i];
					s_Data.QuadVertexBufferPtr->textureIndex = textureIndex;
					s_Data.QuadVertexBufferPtr->TilingFactor = tiling;
					s_Data.QuadVertexBufferPtr->EntityID = entityID;
					s_Data.QuadVertexBufferPtr++;
				}

				s_Data.QuadIndexCount += 6;
				RecordQuadCommand(6);
				s_Data.Stats.QuadCount++;
			}

			penX += g->Advance * scale;
		}
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}

	Renderer2D::Statistics& Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}
}