#pragma once

#include <glm/glm.hpp>

namespace Waffle {

	class Scene;

	// Screen-space game UI renderer. Elements are scene entities (UICanvasComponent on a root, RectTransformComponent + UIImage/UIText/UIButton/UIProgressBar on elements) rendered in pixel space on top of the world, inside the game viewport. See Components.h for the model.
	class UIRenderer
	{
	public:
		// Renders every canvas of the scene; opens/closes its own Renderer2D scene with a Y-down pixel ortho camera. Call after the world pass.
		static void RenderUI(Scene* scene);

		// Runtime-only: updates button hover/press state and dispatches the OnClick handler of the topmost button under the mouse.
		static void UpdateUIInteraction(Scene* scene);

		// UI-pixels-per-screen-pixel of the last rendered frame; the editor uses this to convert gizmo deltas into RectTransform positions.
		static glm::vec2 GetLastCanvasScale() { return s_LastCanvasScale; }

	private:
		static glm::vec2 s_LastCanvasScale;
	};

}
