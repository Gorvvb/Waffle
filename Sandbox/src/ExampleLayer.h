#pragma once

#include "Waffle.h"

class ExampleLayer : public Waffle::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;
	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Waffle::Timestep dt) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Waffle::Event& e) override;
private:
	Waffle::ShaderLibrary m_ShaderLibrary;
	Waffle::Ref<Waffle::Shader> m_Shader;
	Waffle::Ref<Waffle::VertexArray> m_VertexArray;

	Waffle::Ref<Waffle::Shader> m_FlatColorShader, m_TextureShader;
	Waffle::Ref<Waffle::VertexArray> m_SquareVA;

	Waffle::Ref<Waffle::Texture2D> m_Texture, m_GorvLogo;

	Waffle::OrthographicCameraController m_CameraController;

	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};