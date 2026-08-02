#pragma once

#include "Waffle/Core/UUID.h"
#include "Waffle/Core/Timestep.h"
#include "Waffle/Renderer/EditorCamera.h"

#include "entt.hpp"

class b2World;
class b2Body;

namespace Waffle {

	class Entity;

	class Scene
	{
	private:
		std::string m_Name = "Untitled";
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		bool m_IsRunning = false;
		bool m_IsPaused = false;
		int m_StepFrames = 0;

		b2World* m_PhysicsWorld = nullptr;
		std::unordered_map<b2Body*, uint32_t> m_BodyEntityMap;

		float m_GravityY = -9.8f;

		float m_PhysicsAccumulator = 0.0f;
		static constexpr float m_PhysicsFixedStep = 1.0f / 60.0f;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
		friend class LuaScriptEngine;
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(Ref<Scene> other);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		Entity GetEntityByUUID(UUID uuid);
		void DestroyEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		int OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnViewportResize(uint32_t width, uint32_t height);

		void DuplicateEntity(Entity entity);
		void ParentEntity(Entity child, Entity parent);
		void UnparentEntity(Entity child);

		uint32_t GetViewportWidth()  const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		void SetGravity(float g) { m_GravityY = g; }

		Entity GetPrimaryCameraEntity();

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		bool IsRunning() const { return m_IsRunning; }
		bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused) { m_IsPaused = paused; }

		void Step(int frames = 1);

		bool IsEntityValid(entt::entity entity) const { return m_Registry.valid(entity); }

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		b2World* GetPhysicsWorld() { return m_PhysicsWorld; }
		std::unordered_map<b2Body*, uint32_t>& GetBodyEntityMap() { return m_BodyEntityMap; }

		entt::registry& GetRegistry() { return m_Registry; }
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	};
}