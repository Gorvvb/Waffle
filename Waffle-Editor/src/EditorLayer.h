#pragma once

#include "Waffle.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"

#include "Waffle/Renderer/EditorCamera.h"

namespace Waffle {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;
		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep dt) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;
	private:
		bool OnkeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnWindowDrop(WindowDropEvent& e);

		void OnOverlayRender();

		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();

		void NewProject();
		void OpenProject();
		void CreateProject(const std::string& projectName);

		void SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);

		void OnScenePlay();
		void OnSceneStop();
		void OnScenePause();

		void OnDuplicateEntity();

		// UI Panels
		void UI_Toolbar();
	private:
		OrthographicCameraController m_CameraController;

		// Temporary
		Ref<VertexArray> m_SquareVA;
		Ref<Shader> m_FlatColorShader;
		Ref<Framebuffer> m_Framebuffer;

		Entity m_HoveredEntity;
		
		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene, m_RuntimeScene;

		std::filesystem::path m_EditorScenePath;

		bool m_MainCamera = true;

		EditorCamera m_EditorCamera;

		Ref<Texture2D> m_CheckerboardTexture;

		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0, 0 };
		glm::vec2 m_ViewportBounds[2];

		int m_GizmoType = -1;

		bool m_ShowPhysicsColliders = false;

		enum class SceneState
		{
			Edit = 0, Play = 1
		};

		SceneState m_SceneState = SceneState::Edit;

		bool m_ShowNewProjectModal = false;
		bool m_ShowExportModal = false;
		std::string m_ExportStatusMessage;
		bool m_ExportSuccess = false;
		char m_ProjectNameBuffer[128] = "NewProject";
		std::string m_ProjectName;

		char m_ExportAppNameBuffer[128] = "";
		char m_ExportIconPathBuffer[256] = "";
		int m_SelectedExportSceneIndex = 0;
		std::vector<std::filesystem::path> m_ExportAvailableScenes;

		void ExportProject();
		void UpdateWindowTitle();
		void OpenProjectAtPath(const std::filesystem::path& projectPath);
		static std::filesystem::path FindDefaultProjectPath();

		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;

		float m_fps = 0.0f;
		bool m_ClickedOnButton = false;

		// Editor resources
		Ref<Texture2D> m_IconPlay, m_IconStop, m_IconPause, m_IconStep;
		Ref<Texture2D> m_IconPauseInactive, m_IconStepInactive;
		Ref<Texture2D> m_IconNoGizmo, m_IconTransformGizmo, m_IconRotationGizmo, m_IconScaleGizmo;
		Ref<Texture2D> m_IconNoGizmoActive, m_IconTransformGizmoActive, m_IconRotationGizmoActive, m_IconScaleGizmoActive;
	};
}