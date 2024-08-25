#pragma once

#include "Waffle.h"

class Sandbox2D : public Waffle::Layer
{
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;
	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Waffle::Timestep dt) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Waffle::Event& e) override;
private:
	Waffle::OrthographicCameraController m_CameraController;

	// Temporary
	Waffle::Ref<Waffle::VertexArray> m_SquareVA;
	Waffle::Ref<Waffle::Shader> m_FlatColorShader;

	glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
};