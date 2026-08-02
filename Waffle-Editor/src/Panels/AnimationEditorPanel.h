#pragma once

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"

#include <filesystem>
#include <string>

namespace Waffle {

	class AnimationEditorPanel
	{
	public:
		AnimationEditorPanel();
		~AnimationEditorPanel() = default;

		void OnImGuiRender();
		void SetContext(const Ref<Scene>& context);
		void SetSelectedEntity(Entity entity);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;

		// Active editing clip
		std::string m_SelectedClipName;
		char m_NewClipBuffer[64] = "NewClip";

		// Spritesheet Slicer Tool State
		std::filesystem::path m_SlicerTexturePath;
		Ref<Texture2D> m_SlicerTexture;
		int m_SlicerColumns = 4;
		int m_SlicerRows = 4;
		int m_SlicerStartFrame = 0;
		int m_SlicerEndFrame = 3;
		float m_SlicerFPS = 12.0f;
		bool m_SlicerLoop = true;

		// Selected keyframe for dragging individual images
		int m_SelectedKeyframe = 0;
	};

}
