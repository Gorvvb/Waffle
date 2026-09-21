#include "wfpch.h"
#include "UIRenderer.h"

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Renderer/Renderer2D.h"
#include "Waffle/Renderer/Camera.h"
#include "Waffle/Scripting/LuaScriptEngine.h"
#include "Waffle/Core/Input.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Waffle {

	glm::vec2 UIRenderer::s_LastCanvasScale(1.0f, 1.0f);

	namespace
	{
		// Canvas geometry for one frame. Canvas units are design pixels; the
		// uniform Scale maps them to real screen pixels with the UI centered
		// (letterboxed) in the viewport.
		struct CanvasLayout
		{
			glm::vec2 Viewport{ 0.0f };
			glm::vec2 Size{ 0.0f };     // canvas size in canvas units
			glm::vec2 Scale{ 1.0f };    // screen px per canvas unit
			glm::vec2 Offset{ 0.0f };   // screen px of the canvas top-left
		};

		// Screen-space rect (Y down) of a UI element for one frame.
		struct UIRect
		{
			glm::vec2 Min{ 0.0f };
			glm::vec2 Max{ 0.0f };
			float Rotation = 0.0f;

			bool Contains(const glm::vec2& p) const
			{
				return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y;
			}
		};

		// Anchor point as fractions of the reference rect. Y grows downward,
		// so "Top" is y=0.
		glm::vec2 AnchorFractions(UIAnchor anchor)
		{
			switch (anchor)
			{
				case UIAnchor::TopLeft:      return { 0.0f, 0.0f };
				case UIAnchor::TopCenter:    return { 0.5f, 0.0f };
				case UIAnchor::TopRight:     return { 1.0f, 0.0f };
				case UIAnchor::MiddleLeft:   return { 0.0f, 0.5f };
				case UIAnchor::MiddleRight:  return { 1.0f, 0.5f };
				case UIAnchor::BottomLeft:   return { 0.0f, 1.0f };
				case UIAnchor::BottomCenter: return { 0.5f, 1.0f };
				case UIAnchor::BottomRight:  return { 1.0f, 1.0f };
				case UIAnchor::MiddleCenter:
				default:                     return { 0.5f, 0.5f };
			}
		}

		bool ComputeCanvasLayout(Scene* scene, CanvasLayout& out)
		{
			uint32_t vw = scene->GetViewportWidth();
			uint32_t vh = scene->GetViewportHeight();
			if (vw == 0 || vh == 0)
				return false;
			out.Viewport = glm::vec2((float)vw, (float)vh);

			UICanvasComponent* canvas = nullptr;
			for (auto e : scene->GetAllEntitiesWith<UICanvasComponent>())
			{
				Entity entity{ e, scene };
				if (entity.HasComponent<DisabledComponent>())
					continue;
				canvas = &entity.GetComponent<UICanvasComponent>();
				break;
			}
			if (!canvas)
				return false;

			if (canvas->ScaleWithScreen &&
				canvas->ReferenceResolution.x > 1.0f && canvas->ReferenceResolution.y > 1.0f)
			{
				float s = std::min(out.Viewport.x / canvas->ReferenceResolution.x,
					out.Viewport.y / canvas->ReferenceResolution.y);
				out.Scale = glm::vec2(s, s);
				out.Size = canvas->ReferenceResolution;
			}
			else
			{
				out.Scale = glm::vec2(1.0f, 1.0f);
				out.Size = out.Viewport;
			}

			out.Offset = (out.Viewport - out.Size * out.Scale) * 0.5f;
			return true;
		}

		// Screen rect of a UI element in canvas units (Y down). The element's
		// TransformComponent drives the layout: Translation = offset from the
		// anchor in canvas pixels (+Y up, flipped into screen space here),
		// Scale = size in canvas pixels, Rotation.z = tilt. Anchors reference
		// the parent's rect when the parent has a RectTransform, else the
		// canvas.
		UIRect ComputeRect(Scene* scene, Entity entity, const CanvasLayout& layout,
			std::unordered_map<uint32_t, UIRect>& cache)
		{
			auto key = (uint32_t)(entt::entity)entity;
			auto found = cache.find(key);
			if (found != cache.end())
				return found->second;

			auto& rt = entity.GetComponent<RectTransformComponent>();
			glm::vec2 frac = AnchorFractions(rt.Anchor);

			glm::vec2 baseMin(0.0f), baseMax = layout.Size;
			if (entity.HasComponent<RelationshipComponent>())
			{
				auto& rel = entity.GetComponent<RelationshipComponent>();
				Entity parent = rel.Parent ? scene->GetEntityByUUID(rel.Parent) : Entity();
				if (parent && parent.HasComponent<RectTransformComponent>())
				{
					// Depth cap guards against hierarchy cycles.
					int depth = 0;
					Entity ancestor = parent;
					while (ancestor && depth < 32)
					{
						ancestor = scene->GetParent(ancestor);
						depth++;
					}
					if (depth < 32)
					{
						UIRect pr = ComputeRect(scene, parent, layout, cache);
						baseMin = pr.Min;
						baseMax = pr.Max;
					}
				}
			}

			glm::mat4 world = scene->GetWorldTransform(entity);
			glm::vec2 translation(world[3].x, -world[3].y); // +Y up -> Y down
			glm::vec2 size(glm::length(world[0]), glm::length(world[1]));
			float rotation = -glm::atan(world[0].y, world[0].x);

			glm::vec2 attach = baseMin + (baseMax - baseMin) * frac + translation;
			UIRect rect;
			rect.Min = attach - rt.Pivot * size;
			rect.Max = rect.Min + size;
			rect.Rotation = rotation;

			cache[key] = rect;
			return rect;
		}

		struct UIItem
		{
			Entity E;
			UIRect Rect;
			int Order = 0;
		};

		std::vector<UIItem> GatherUIItems(Scene* scene, const CanvasLayout& layout,
			std::unordered_map<uint32_t, UIRect>& cache)
		{
			std::vector<UIItem> items;
			for (auto e : scene->GetAllEntitiesWith<RectTransformComponent>())
			{
				Entity entity{ e, scene };
				if (entity.HasComponent<DisabledComponent>())
					continue;
				auto& rt = entity.GetComponent<RectTransformComponent>();
				items.push_back({ entity, ComputeRect(scene, entity, layout, cache), rt.Order });
			}

			std::stable_sort(items.begin(), items.end(), [](const UIItem& a, const UIItem& b)
				{
					if (a.Order != b.Order)
						return a.Order < b.Order;
					return (uint32_t)(entt::entity)a.E < (uint32_t)(entt::entity)b.E;
				});
			return items;
		}

		glm::mat4 BuildDrawTransform(const UIRect& rect, const CanvasLayout& layout)
		{
			glm::vec2 center = (rect.Min + rect.Max) * 0.5f;
			glm::vec2 size = rect.Max - rect.Min;
			glm::vec2 screenCenter = layout.Offset + center * layout.Scale;
			glm::vec2 screenSize = size * layout.Scale;
			return glm::translate(glm::mat4(1.0f), glm::vec3(screenCenter, 0.0f))
				* glm::rotate(glm::mat4(1.0f), rect.Rotation, glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::scale(glm::mat4(1.0f), glm::vec3(screenSize, 1.0f));
		}

		void DrawCenteredText(const std::string& text, float fontSize, const glm::vec4& color,
			const UIRect& rect, const CanvasLayout& layout, int entityID)
		{
			// Bake the font at the on-screen size so scaling the canvas down
			// does not blur the glyphs.
			float effectiveSize = fontSize * layout.Scale.y;
			Ref<Font> font = Font::Resolve(std::string(), effectiveSize);
			if (!font)
				return;

			float glyphScale = effectiveSize / font->GetSize();
			glm::vec2 screenMin = layout.Offset + rect.Min * layout.Scale;
			glm::vec2 screenMax = layout.Offset + rect.Max * layout.Scale;
			glm::vec2 screenCenter = (screenMin + screenMax) * 0.5f;

			float textWidth = font->GetTextWidth(text, glyphScale);
			float x = screenCenter.x - textWidth * 0.5f;
			float baseline = screenCenter.y + (font->GetAscent() - font->GetDescent()) * 0.5f * glyphScale;

			Renderer2D::DrawString(text, font, glm::vec2(x, baseline), glyphScale, color, entityID);
		}
	}

	void UIRenderer::RenderUI(Scene* scene)
	{
		CanvasLayout layout;
		if (!ComputeCanvasLayout(scene, layout))
			return;

		glm::mat4 projection = glm::ortho(0.0f, layout.Viewport.x, layout.Viewport.y, 0.0f);
		Camera uiCamera(projection);
		Renderer2D::BeginScene(uiCamera, glm::mat4(1.0f));

		std::unordered_map<uint32_t, UIRect> cache;
		auto items = GatherUIItems(scene, layout, cache);

		for (const auto& item : items)
		{
			Entity entity = item.E;
			glm::mat4 transform = BuildDrawTransform(item.Rect, layout);
			int entityID = (int)(uint32_t)(entt::entity)entity;

			if (entity.HasComponent<UIImageComponent>())
			{
				auto& image = entity.GetComponent<UIImageComponent>();
				if (image.Texture)
					Renderer2D::DrawQuad(transform, image.Texture, glm::vec2(1.0f), image.Color, entityID);
				else
					Renderer2D::DrawQuad(transform, image.Color, entityID);
			}

			if (entity.HasComponent<UIProgressBarComponent>())
			{
				auto& bar = entity.GetComponent<UIProgressBarComponent>();
				Renderer2D::DrawQuad(transform, bar.BackgroundColor, entityID);

				glm::vec2 screenMin = layout.Offset + item.Rect.Min * layout.Scale;
				glm::vec2 screenSize = (item.Rect.Max - item.Rect.Min) * layout.Scale;
				float pad = bar.Padding * layout.Scale.x;
				glm::vec2 fillMin = screenMin + glm::vec2(pad, pad);
				glm::vec2 fillSize = glm::max(screenSize - glm::vec2(2.0f * pad), glm::vec2(0.0f));
				fillSize.x *= glm::clamp(bar.Value, 0.0f, 1.0f);
				if (fillSize.x > 0.05f && fillSize.y > 0.05f)
				{
					glm::vec2 fillCenter = fillMin + fillSize * 0.5f;
					glm::mat4 fillTransform = glm::translate(glm::mat4(1.0f), glm::vec3(fillCenter, 0.0f))
						* glm::scale(glm::mat4(1.0f), glm::vec3(fillSize, 1.0f));
					Renderer2D::DrawQuad(fillTransform, bar.FillColor, entityID);
				}
			}

			if (entity.HasComponent<UIButtonComponent>())
			{
				auto& button = entity.GetComponent<UIButtonComponent>();
				glm::vec4 tint = button.Pressed ? button.PressedColor
					: button.Hovered ? button.HoverColor : button.Color;
				Ref<Texture2D> texture = (button.Pressed && button.PressedTexture) ? button.PressedTexture
					: (button.Hovered && button.HoverTexture) ? button.HoverTexture
					: button.NormalTexture;
				if (texture)
					Renderer2D::DrawQuad(transform, texture, glm::vec2(1.0f), tint, entityID);
				else
					Renderer2D::DrawQuad(transform, tint, entityID);

				if (!button.Label.empty())
					DrawCenteredText(button.Label, button.LabelSize, button.LabelColor, item.Rect, layout, entityID);
			}

			if (entity.HasComponent<UITextComponent>())
			{
				auto& text = entity.GetComponent<UITextComponent>();
				if (!text.Text.empty())
				{
					float effectiveSize = text.FontSize * layout.Scale.y;
					Ref<Font> font = Font::Resolve(text.FontPath, effectiveSize);
					if (font)
					{
						float glyphScale = effectiveSize / font->GetSize();
						glm::vec2 screenMin = layout.Offset + item.Rect.Min * layout.Scale;
						glm::vec2 screenMax = layout.Offset + item.Rect.Max * layout.Scale;
						glm::vec2 screenCenter = (screenMin + screenMax) * 0.5f;

						float textWidth = font->GetTextWidth(text.Text, glyphScale);
						float x = screenMin.x;
						if (text.Alignment == UITextAlignment::Center)
							x = screenCenter.x - textWidth * 0.5f;
						else if (text.Alignment == UITextAlignment::Right)
							x = screenMax.x - textWidth;

						float baseline = screenCenter.y
							+ (font->GetAscent() - font->GetDescent()) * 0.5f * glyphScale;

						Renderer2D::DrawString(text.Text, font, glm::vec2(x, baseline), glyphScale, text.Color, entityID);
					}
				}
			}
		}

		Renderer2D::EndScene();
		s_LastCanvasScale = layout.Scale;
	}

	void UIRenderer::UpdateUIInteraction(Scene* scene)
	{
		CanvasLayout layout;
		if (!ComputeCanvasLayout(scene, layout))
			return;

		glm::vec2 mouse = Input::GetMousePosition(); // window-relative, Y down
		glm::vec2 origin = LuaScriptEngine::HasGameViewport()
			? LuaScriptEngine::GetGameViewportOrigin() : glm::vec2(0.0f);
		glm::vec2 viewportMouse = mouse - origin;
		glm::vec2 canvasMouse = (viewportMouse - layout.Offset) / layout.Scale;

		bool blocked = LuaScriptEngine::IsGameplayMouseBlocked();
		bool clicked = !blocked && Input::IsMouseButtonPressed(Mouse::ButtonLeft);

		std::unordered_map<uint32_t, UIRect> cache;
		auto items = GatherUIItems(scene, layout, cache);

		// Front-most (highest order) buttons consume the mouse first.
		bool hoverConsumed = false;
		for (auto it = items.rbegin(); it != items.rend(); ++it)
		{
			Entity entity = it->E;
			if (!entity.HasComponent<UIButtonComponent>())
				continue;
			auto& button = entity.GetComponent<UIButtonComponent>();

			bool inside = !blocked && it->Rect.Contains(canvasMouse);
			bool consumes = inside && !hoverConsumed;
			button.Hovered = consumes;
			button.Pressed = consumes && (Input::IsMouseButtonHeld(Mouse::ButtonLeft) || clicked);

			if (consumes)
			{
				hoverConsumed = true;
				if (clicked && !button.OnClick.empty())
					LuaScriptEngine::CallUIHandler(button.OnClick, (uint32_t)(entt::entity)entity);
			}
		}
	}

}
