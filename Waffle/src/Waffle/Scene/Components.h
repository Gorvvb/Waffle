#pragma once

#include "SceneCamera.h"
#include "Waffle/Core/UUID.h"
#include "Waffle/Core/Log.h"
#include "Waffle/Renderer/Texture.h"
#include "Waffle/Renderer/SubTexture2D.h"
#include "Waffle/Renderer/Font.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Waffle {

	struct IDComponent
	{
		UUID ID;
		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& uuid)
			: ID(uuid) {}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(glm::vec3& translation)
			: Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct RelationshipComponent
	{
		UUID Parent = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		RelationshipComponent(UUID parent)
			: Parent(parent) {}
	};

	enum class LuaFieldType { Float, Int, Bool, String };

	struct LuaField
	{
		std::string  Name;
		LuaFieldType Type = LuaFieldType::Float;
		float        FloatVal = 0.f;
		int          IntVal = 0;
		bool         BoolVal = false;
		std::string  StringVal;

		bool UserModified = false; // true = user changed this, don't overwrite from script
	};

	struct ScriptComponent
	{
		std::string              ClassName;
		std::vector<std::string> ScriptPaths;
		std::vector<std::string> ScriptTableKeys; // runtime only

		// Key = script path, value = public fields for that script
		std::unordered_map<std::string, std::vector<LuaField>> Fields;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
	};

	struct LifetimeComponent
	{
		float Lifetime = 5.0f;
		float RemainingTime = 5.0f;

		LifetimeComponent() = default;
		LifetimeComponent(const LifetimeComponent&) = default;
		LifetimeComponent(float lifetime)
			: Lifetime(lifetime), RemainingTime(lifetime) {}
	};

	// How a sprite maps onto the entity quad when its native aspect ratio
	// differs from the quad's. Fit/Fill keep the pixels square - essential
	// for animations whose frames have different native sizes.
	enum class SpriteAspectMode : int8_t { Stretch = 0, Fit = 1, Fill = 2 };

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		glm::vec2 TilingFactor = { 1.0f, 1.0f };

		TextureFilter FilterMode = TextureFilter::Linear;
		SpriteAspectMode AspectMode = SpriteAspectMode::Stretch;

		int SortingLayer = 0;
		int SortingOrder = 0;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {}
	};

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		int SortingLayer = 0;
		int SortingOrder = 0;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct CameraComponent
	{
		Waffle::SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		glm::vec4 BackgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };
		Ref<Texture2D> BackgroundImage = nullptr;
		std::string BackgroundImagePath = "";
		glm::vec2 BackgroundTilingFactor = { 1.0f, 1.0f };
		TextureFilter BackgroundFilterMode = TextureFilter::Linear;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	// TODO: Consider removing native scripting, since it is not used in the current engine architecture.
	// Forward declaration
	class ScriptableEntity;

	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		// Default-initialized to null: components constructed without Bind()
		// previously called through garbage function pointers.
		ScriptableEntity* (*InstanciateScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		template<typename T>
		void Bind()
		{
			InstanciateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	// Physics

	struct Rigidbody2DComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;
		// 0 = derive mass from Density x collider area (the Box2D default).
		// The old default of 1.0 made SetMassData run for every dynamic body
		// and silently override density-driven mass.
		float Mass = 0.0f;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		// Render interpolation state (runtime only, not serialized): the
		// body state before the last physics step. Rendering blends
		// prev->current by the accumulator fraction so bodies move
		// smoothly between fixed steps instead of stair-stepping.
		glm::vec2 RuntimePrevPosition{ 0.0f, 0.0f };
		float RuntimePrevAngle = 0.0f;
		bool RuntimePrevValid = false;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f; // Bounciness
		float RestitutionThreshold = 0.5f; // Threshold to stop endless bouncing

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 0.5f;

		// TODO: Perhaps make it possible for the user to make a physics material, that will change these settnings.
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	struct PolygonCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		std::vector<glm::vec2> Vertices;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		PolygonCollider2DComponent() = default;
		PolygonCollider2DComponent(const PolygonCollider2DComponent&) = default;
	};

	// Tag component that marks an entity as inactive.
	// When present, the entity is skipped by the script engine and renderer.
	struct DisabledComponent {};

	struct AnimationClip
	{
		std::string Name = "Default";
		std::string TexturePath;
		Ref<Texture2D> Texture = nullptr;

		int Columns = 1;
		int Rows = 1;
		int StartFrame = 0;
		int EndFrame = 0;
		float FPS = 12.0f;
		bool Loop = true;

		std::vector<std::string> KeyframeImagePaths;
		std::vector<Ref<SubTexture2D>> SubTextures;

		// Largest frame of the clip in texture pixels (runtime cache).
		glm::vec2 MaxFramePixelSize{ 0.0f, 0.0f };

		// Largest visible-content size across the frames, in texture pixels.
		glm::vec2 MaxContentPixelSize{ 0.0f, 0.0f };

		// Per-frame opaque-content rect as fractions of the frame window
		// (xy = bottom-left, zw = top-right, in UV space where y grows up).
		// Frames are aligned and scaled by their visible content so
		// differently cropped frames don't glide; equals the full frame
		// when the pixels can't be inspected.
		std::vector<glm::vec4> SubContentFracs;

		// Per-frame pivot override in RENDER space (y up: (0.5,0) = frame
		// bottom). Negative x = unset: the animator's FramePivot applies.
		// Comes from spritesheet region pivots.
		std::vector<glm::vec2> SubPivots;

		// Appends a frame and derives its content rect from an EXACT pixel
		// window (top-left origin). The window must never be reconstructed
		// from UVs: the float round-trip drifts by a pixel and bleeds into
		// adjacent regions of tightly packed sheets, skewing alignment.
		void PushFrame(const Ref<SubTexture2D>& sub, const std::string& imagePath, const glm::vec4& windowPx,
			const glm::vec2& pivotOverride = glm::vec2(-1.0f))
		{
			SubTextures.push_back(sub);
			SubPivots.push_back(pivotOverride);

			glm::vec4 frac(0.0f, 0.0f, 1.0f, 1.0f);
			glm::vec2 framePx(windowPx.z - windowPx.x, windowPx.w - windowPx.y);
			if (sub && sub->GetTexture() && framePx.x > 0.0f && framePx.y > 0.0f)
			{
				MaxFramePixelSize = glm::max(MaxFramePixelSize, framePx);

				glm::vec4 bounds = ComputeOpaqueBounds(imagePath, windowPx);
				glm::vec2 contentPx(bounds.z - bounds.x, bounds.w - bounds.y);
				if (contentPx.x > 0.0f && contentPx.y > 0.0f)
				{
					// Image space (y down) -> frame fractions in UV space (y up).
					frac = glm::vec4(
						(bounds.x - windowPx.x) / framePx.x,
						1.0f - (bounds.w - windowPx.y) / framePx.y,
						(bounds.z - windowPx.x) / framePx.x,
						1.0f - (bounds.y - windowPx.y) / framePx.y);
					MaxContentPixelSize = glm::max(MaxContentPixelSize, contentPx);
				}
			}
			SubContentFracs.push_back(frac);
		}

		void RefreshSubTextures()
		{
			SubTextures.clear();
			SubContentFracs.clear();
			SubPivots.clear();
			MaxFramePixelSize = glm::vec2(0.0f);
			MaxContentPixelSize = glm::vec2(0.0f);

			if (!KeyframeImagePaths.empty())
			{
				for (const auto& path : KeyframeImagePaths)
				{
					if (path.empty())
					{
						PushFrame(nullptr, std::string(), glm::vec4(0.0f));
						continue;
					}

					Ref<SubTexture2D> sub = nullptr;
					glm::vec4 windowPx(0.0f);   // exact pixel window, top-left origin
					glm::vec2 pivotOverride(-1.0f); // render-space (y up)
					std::string scanPath = path;

					size_t pipePos = path.find('|');
					size_t colonPos = (pipePos == std::string::npos) ? path.find(':') : std::string::npos;
					if (pipePos != std::string::npos)
					{
						// Pixel-rect format: "texturePath|minX,minY,maxX,maxY"
						// with an optional third segment "|pivotX,pivotY"
						// (normalized, image space y-down; -1 or missing =
						// inherit the animator Frame Pivot).
						std::string texPath = path.substr(0, pipePos);
						std::string rectStr = path.substr(pipePos + 1);
						size_t secondPipe = rectStr.find('|');
						if (secondPipe != std::string::npos)
						{
							float pvx = -1.0f, pvy = -1.0f;
							if (sscanf_s(rectStr.substr(secondPipe + 1).c_str(), "%f,%f", &pvx, &pvy) == 2 &&
								pvx >= 0.0f && pvx <= 1.0f && pvy >= 0.0f && pvy <= 1.0f)
							{
								pivotOverride = { pvx, 1.0f - pvy }; // y-down -> y-up
							}
							rectStr = rectStr.substr(0, secondPipe);
						}
						scanPath = texPath;
						try {
							Ref<Texture2D> tex = Texture2D::Create(texPath, TextureFilter::Nearest);
							if (tex)
							{
								float minX, minY, maxX, maxY;
								if (sscanf_s(rectStr.c_str(), "%f,%f,%f,%f", &minX, &minY, &maxX, &maxY) == 4)
								{
									// Convert pixel coords to UV (flip Y for OpenGL)
									float w = (float)tex->GetWidth();
									float h = (float)tex->GetHeight();
									glm::vec2 uvMin = { minX / w, 1.0f - maxY / h };
									glm::vec2 uvMax = { maxX / w, 1.0f - minY / h };
									sub = CreateRef<SubTexture2D>(tex, uvMin, uvMax);
									windowPx = glm::vec4(minX, minY, maxX, maxY);
								}
							}
						} catch (...) {}
					}
					else if (colonPos != std::string::npos)
					{
						std::string sheetPath = path.substr(0, colonPos);
						// stoi sits inside its own try: any path containing ':'
						// (every absolute Windows path C:/...) lands here and
						// would throw std::invalid_argument out of a render call.
						int frameIdx = 0;
						bool frameIdxValid = false;
						try {
							frameIdx = std::stoi(path.substr(colonPos + 1));
							frameIdxValid = frameIdx >= 0;
						} catch (...) {}
						try {
							if (frameIdxValid)
							{
							YAML::Node data = YAML::LoadFile(sheetPath);
							std::string texName = data["Spritesheet"].as<std::string>("");
							int cols = data["Columns"].as<int>(1);
							int rows = data["Rows"].as<int>(1);
							if (cols > 0 && rows > 0)
							{
							std::filesystem::path fullTex = std::filesystem::path(sheetPath).parent_path() / texName;
							if (std::filesystem::exists(fullTex))
							{
								Ref<Texture2D> tex = Texture2D::Create(fullTex.string(), TextureFilter::Nearest);
								if (tex)
								{
									float cellW = (float)tex->GetWidth() / (float)cols;
									float cellH = (float)tex->GetHeight() / (float)rows;
									int col = frameIdx % cols;
									int row = rows - 1 - (frameIdx / cols);
									sub = SubTexture2D::CreateFromCoords(tex, { (float)col, (float)row }, { cellW, cellH });
									windowPx = glm::vec4(
										(float)col * cellW, (float)row * cellH,
										(float)(col + 1) * cellW, (float)(row + 1) * cellH);
									scanPath = fullTex.string();
								}
							}
							}
							}
						} catch (...) {}
					}
					else
					{
						// Only load if the path looks like an image file - guard against
						// stale keyframe paths that point to .spritesheet / .yaml files.
						std::string lext = std::filesystem::path(path).extension().string();
						for (auto& c : lext) c = (char)::tolower(c);
						if (lext == ".png" || lext == ".jpg" || lext == ".jpeg" || lext == ".bmp" || lext == ".tga")
						{
							Ref<Texture2D> tex = Texture2D::Create(path, TextureFilter::Nearest);
							if (tex)
							{
								sub = CreateRef<SubTexture2D>(tex, glm::vec2(0, 0), glm::vec2(1, 1));
								windowPx = glm::vec4(0.0f, 0.0f, (float)tex->GetWidth(), (float)tex->GetHeight());
							}
						}
					}

					PushFrame(sub, scanPath, windowPx, pivotOverride);
				}
				return;
			}

			if (!Texture || Columns <= 0 || Rows <= 0) return;

			float cellWidth = (float)Texture->GetWidth() / (float)Columns;
			float cellHeight = (float)Texture->GetHeight() / (float)Rows;

			int totalFrames = Columns * Rows;
			int start = std::max(0, std::min(StartFrame, totalFrames - 1));
			int end = std::max(start, std::min(EndFrame, totalFrames - 1));

			for (int i = start; i <= end; i++)
			{
				int col = i % Columns;
				int row = Rows - 1 - (i / Columns);
				auto sub = SubTexture2D::CreateFromCoords(Texture, { (float)col, (float)row }, { cellWidth, cellHeight });
				glm::vec4 windowPx(
					(float)col * cellWidth, (float)row * cellHeight,
					(float)(col + 1) * cellWidth, (float)(row + 1) * cellHeight);
				PushFrame(sub, Texture->GetPath(), windowPx);
			}
		}
	};

	struct AnimatorComponent
	{
		std::unordered_map<std::string, AnimationClip> Clips;
		std::string CurrentClip = "";
		// Where a frame sits inside the entity quad when it is smaller than
		// the clip bounds (Sprite Aspect = Fit): (0,0) bottom-left, (0.5,0.5)
		// center, (1,1) top-right. Bottom-center keeps the feet planted
		// across frames of different sizes.
		glm::vec2 FramePivot = { 0.5f, 0.0f };
		int CurrentFrameIndex = 0;
		float Timer = 0.0f;
		bool IsPlaying = false;

		AnimatorComponent() = default;
		AnimatorComponent(const AnimatorComponent&) = default;

		void Play(const std::string& clipName)
		{
			if (Clips.find(clipName) != Clips.end())
			{
				CurrentClip = clipName;
				CurrentFrameIndex = 0;
				Timer = 0.0f;
				IsPlaying = true;
			}
		}

		void Stop()
		{
			IsPlaying = false;
			CurrentFrameIndex = 0;
			Timer = 0.0f;
		}

		void Pause()
		{
			IsPlaying = false;
		}

		// Largest visible-content size across ALL clips of this animator, in
		// texture pixels. Rendering normalizes every clip against this one
		// value, so the character keeps the same on-screen size when clips
		// (whose largest frames differ) switch.
		glm::vec2 GetMaxContentPixelSize()
		{
			glm::vec2 result(0.0f);
			for (auto& clipEntry : Clips)
			{
				auto& clip = clipEntry.second;
				if (clip.SubTextures.empty())
					clip.RefreshSubTextures();
				result = glm::max(result, clip.MaxContentPixelSize);
			}
			return result;
		}

		// Opaque-content rect of the current frame as fractions of the frame
		// window (UV space, y up). Returns x < 0 when there is no current
		// frame; (0,0,1,1) when the pixels couldn't be inspected.
		glm::vec4 GetCurrentFrameContentFrac()
		{
			auto it = Clips.find(CurrentClip);
			if (it == Clips.end()) return glm::vec4(-1.0f);
			auto& clip = it->second;
			if (clip.SubTextures.empty())
				clip.RefreshSubTextures();
			if (clip.SubContentFracs.empty()) return glm::vec4(-1.0f);

			int idx = CurrentFrameIndex;
			if (idx < 0 || idx >= (int)clip.SubContentFracs.size())
				idx = 0;
			return clip.SubContentFracs[idx];
		}

		// Effective pivot for the current frame: the region's pivot
		// override when set, else the animator-wide FramePivot.
		glm::vec2 GetCurrentFramePivot()
		{
			auto it = Clips.find(CurrentClip);
			if (it == Clips.end()) return FramePivot;
			auto& clip = it->second;
			if (clip.SubTextures.empty())
				clip.RefreshSubTextures();

			int idx = CurrentFrameIndex;
			if (idx < 0 || idx >= (int)clip.SubPivots.size())
				return FramePivot;
			const glm::vec2& p = clip.SubPivots[idx];
			return (p.x >= 0.0f && p.y >= 0.0f) ? p : FramePivot;
		}

		Ref<SubTexture2D> GetCurrentSubTexture()
		{
			auto it = Clips.find(CurrentClip);
			if (it == Clips.end()) return nullptr;
			auto& clip = it->second;
			if (clip.SubTextures.empty())
				clip.RefreshSubTextures();

			if (clip.SubTextures.empty()) return nullptr;
			if (CurrentFrameIndex < 0 || CurrentFrameIndex >= (int)clip.SubTextures.size())
				CurrentFrameIndex = 0;

			return clip.SubTextures[CurrentFrameIndex];
		}

		void Update(float dt)
		{
			if (!IsPlaying || CurrentClip.empty()) return;

			auto it = Clips.find(CurrentClip);
			if (it == Clips.end()) return;
			auto& clip = it->second;

			if (clip.SubTextures.empty())
				clip.RefreshSubTextures();

			if (clip.SubTextures.empty() || clip.FPS <= 0.0f) return;

			Timer += dt;
			float frameTime = 1.0f / clip.FPS;

			while (Timer >= frameTime)
			{
				Timer -= frameTime;
				CurrentFrameIndex++;
				if (CurrentFrameIndex >= (int)clip.SubTextures.size())
				{
					if (clip.Loop)
					{
						CurrentFrameIndex = 0;
					}
					else
					{
						CurrentFrameIndex = (int)clip.SubTextures.size() - 1;
						IsPlaying = false;
						break;
					}
				}
			}
		}
	};

	// -------------------------------------------------------------------------
	// Game UI system.
	//
	// A UI canvas renders in screen space (pixels, origin top-left, Y down)
	// on top of the world, inside the game viewport. Elements are ordinary
	// entities: add a UICanvasComponent to one root entity, then give child
	// elements a RectTransformComponent plus any of UIImage/UIText/UIButton/
	// UIProgressBar below. RectTransforms anchor to the canvas, or to the
	// parent's rect when the parent itself has a RectTransform.
	// -------------------------------------------------------------------------

	struct UICanvasComponent
	{
		// Design resolution the layout was authored against. The whole UI is
		// uniformly scaled to fit the actual viewport (letterboxed, centered).
		glm::vec2 ReferenceResolution = { 1920.0f, 1080.0f };
		bool ScaleWithScreen = true;

		UICanvasComponent() = default;
		UICanvasComponent(const UICanvasComponent&) = default;
	};

	// 3x3 anchor points. In canvas space Y grows downward, so "Top" is y=0.
	enum class UIAnchor : int8_t
	{
		TopLeft = 0, TopCenter, TopRight,
		MiddleLeft, MiddleCenter, MiddleRight,
		BottomLeft, BottomCenter, BottomRight
	};

	enum class UITextAlignment : int8_t { Left = 0, Center, Right };

	struct RectTransformComponent
	{
		// Which point of the canvas (or of the parent rect, when the parent has
		// a RectTransform) the element attaches to.
		UIAnchor Anchor = UIAnchor::MiddleCenter;
		// Point of the rect placed at the anchor: (0,0) top-left, (1,1)
		// bottom-right (screen space, Y down).
		glm::vec2 Pivot = { 0.5f, 0.5f };
		int Order = 0;         // draw order within the canvas (lower first)

		// Position, size and rotation come from the entity's regular
		// TransformComponent: Translation = offset in canvas pixels from the
		// anchor (world convention, +Y up), Scale = element size in canvas
		// pixels, Rotation.z = tilt. The gizmo therefore just works.

		RectTransformComponent() = default;
		RectTransformComponent(const RectTransformComponent&) = default;
	};

	struct UIImageComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		std::string TexturePath;
		TextureFilter FilterMode = TextureFilter::Linear;

		UIImageComponent() = default;
		UIImageComponent(const UIImageComponent&) = default;
	};

	struct UITextComponent
	{
		std::string Text = "Text";
		// Empty = engine default font (Assets/fonts/OpenSans/OpenSans-Regular.ttf).
		std::string FontPath;
		float FontSize = 28.0f;
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		UITextAlignment Alignment = UITextAlignment::Center;

		// Resolved lazily at render time; not serialized.
		Ref<Font> RuntimeFont;

		UITextComponent() = default;
		UITextComponent(const UITextComponent&) = default;
	};

	struct UIButtonComponent
	{
		glm::vec4 Color{ 0.16f, 0.17f, 0.20f, 1.0f };
		glm::vec4 HoverColor{ 0.22f, 0.23f, 0.27f, 1.0f };
		glm::vec4 PressedColor{ 0.11f, 0.12f, 0.14f, 1.0f };

		Ref<Texture2D> NormalTexture;
		Ref<Texture2D> HoverTexture;
		Ref<Texture2D> PressedTexture;
		std::string NormalTexturePath;
		std::string HoverTexturePath;
		std::string PressedTexturePath;

		// Optional label rendered centered on the button with the engine font
		// system (same fonts as UIText). Empty = no label.
		std::string Label = "Button";
		float LabelSize = 22.0f;
		glm::vec4 LabelColor{ 1.0f, 1.0f, 1.0f, 1.0f };

		// Name of the Lua function called on click: function OnClick(entity) end
		std::string OnClick;

		// Runtime interaction state; not serialized.
		bool Hovered = false;
		bool Pressed = false;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};

	struct UIProgressBarComponent
	{
		float Value = 1.0f; // 0..1
		glm::vec4 BackgroundColor{ 0.08f, 0.09f, 0.11f, 0.85f };
		glm::vec4 FillColor{ 0.914f, 0.608f, 0.176f, 1.0f };
		float Padding = 2.0f;

		UIProgressBarComponent() = default;
		UIProgressBarComponent(const UIProgressBarComponent&) = default;
	};

	// -------------------------------------------------------------------------
	// Tilemaps.
	//
	// A TilemapComponent owns a tilesheet sliced into TileSize x TileSize
	// pixel tiles and a sparse grid of placed tiles (cell -> tile index).
	// Each tile renders one transform-scale square: the entity scale is the
	// tile size in world units (per axis) and its translation offsets the
	// whole, unbounded map. Solid tiles + the optional TilemapColliderComponent
	// bake into a small set of merged static colliders at runtime start
	// (greedy rectangle merge - one fixture per run of solid tiles, not one
	// per tile).
	// -------------------------------------------------------------------------

	struct TilemapComponent
	{
		Ref<Texture2D> TilesetTexture;
		std::string TexturePath;
		TextureFilter FilterMode = TextureFilter::Nearest;

		int TileSize = 16;          // pixels per tile in the sheet
		glm::vec4 Tint{ 1.0f, 1.0f, 1.0f, 1.0f };

		int SortingLayer = 0;
		int SortingOrder = 0;

		// Sparse map data: grid cell (x grows right, y grows up) -> tile
		// index (row-major from the sheet's top-left).
		std::map<std::pair<int, int>, int> Tiles;

		TilemapComponent() = default;
		TilemapComponent(const TilemapComponent&) = default;

		// Runtime tile cache - not serialized.
		std::vector<Ref<SubTexture2D>> TileCache;
		int CacheTileSize = 0;

		int TileColumns() const
		{
			if (!TilesetTexture || TileSize <= 0) return 0;
			return (int)TilesetTexture->GetWidth() / TileSize;
		}

		int TileRows() const
		{
			if (!TilesetTexture || TileSize <= 0) return 0;
			return (int)TilesetTexture->GetHeight() / TileSize;
		}

		Ref<SubTexture2D> GetTileSubTexture(int index)
		{
			if (!TilesetTexture || TileSize <= 0) return nullptr;
			int cols = TileColumns();
			int rows = TileRows();
			if (cols <= 0 || rows <= 0 || index < 0 || index >= cols * rows) return nullptr;

			if ((int)TileCache.size() != cols * rows || CacheTileSize != TileSize)
			{
				TileCache.clear();
				TileCache.resize((size_t)cols * rows);
				CacheTileSize = TileSize;
			}

			if (!TileCache[index])
			{
				int col = index % cols;
				// Tile indices count rows from the sheet's top; UV space
				// counts from the bottom.
				int row = rows - 1 - (index / cols);
				// Small UV inset: sampling exactly on the tile's borders
				// lets neighbouring sheet pixels bleed through at seams.
				// A tenth of a texel is enough to avoid boundary rounding
				// while keeping the edge pixels (nearly) full width.
				const float texWf = (float)TilesetTexture->GetWidth();
				const float texHf = (float)TilesetTexture->GetHeight();
				const float epsX = 0.1f / texWf;
				const float epsY = 0.1f / texHf;
				glm::vec2 uvMin = { (col * (float)TileSize) / texWf + epsX,
					(row * (float)TileSize) / texHf + epsY };
				glm::vec2 uvMax = { ((col + 1) * (float)TileSize) / texWf - epsX,
					((row + 1) * (float)TileSize) / texHf - epsY };
				TileCache[index] = CreateRef<SubTexture2D>(TilesetTexture, uvMin, uvMax);
			}
			return TileCache[index];
		}
	};

	struct TilemapColliderComponent
	{
		// Tiles whose indices appear here are solid. Empty = no collision.
		std::vector<int> SolidTileIndices;

		// Surface material of the merged fixtures. Box2D mixes friction per
		// contact pair (geometric mean): a wall/obstacle tilemap with
		// Friction 0 never grips a falling body, while ground tilemaps keep
		// their grip.
		float Friction = 0.6f;
		float Restitution = 0.0f;

		// Runtime: merged fixtures live on one static body; not serialized.
		void* RuntimeBody = nullptr;

		TilemapColliderComponent() = default;
		TilemapColliderComponent(const TilemapColliderComponent&) = default;
	};
}