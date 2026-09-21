#pragma once

#include <glm/glm.hpp>

namespace Waffle {

	class Scene;

	// Screen-space game UI renderer. Elements live in the scene as entities
	// (UICanvasComponent on a root, RectTransformComponent + UIImage/UIText/
	// UIButton/UIProgressBar on elements) and render in pixel space on top of
	// the world, inside the game viewport. See Components.h for the model.
	class UIRenderer
	{
	public:
		// Renders every canvas of the scene. Opens and closes its own
		// Renderer2D scene with a Y-down pixel orthographic camera; call it
		// after the world pass has finished.
		static void RenderUI(Scene* scene);

		// Runtime-only: updates button hover/press state and dispatches the
		// OnClick Lua handler of the topmost button under the mouse.
		static void UpdateUIInteraction(Scene* scene);

		// UI-pixels-per-screen-pixel of the last rendered frame; the editor
		// uses this to convert gizmo deltas into RectTransform positions.
		static glm::vec2 GetLastCanvasScale() { return s_LastCanvasScale; }

	private:
		static glm::vec2 s_LastCanvasScale;
	};

}
