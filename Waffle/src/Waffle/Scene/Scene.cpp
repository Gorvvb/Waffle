#include "wfpch.h"
#include "Scene.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Waffle/Scripting/LuaScriptEngine.h"
#include "Waffle/Renderer/Renderer2D.h"
#include "Waffle/Math/Math.h"

#include <glm/glm.hpp>

#include "Entity.h"

// Box2D
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"

namespace Waffle {

	static b2BodyType Rigid2DTypeToBox2DBody(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:	return b2BodyType::b2_staticBody;
			case Rigidbody2DComponent::BodyType::Dynamic:	return b2BodyType::b2_dynamicBody;
			case Rigidbody2DComponent::BodyType::Kinematic: return b2BodyType::b2_kinematicBody;
		}

		WF_CORE_ASSERT(false, "Unknown body type!");
		return b2_staticBody;
	}

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
	}

	template<typename Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto view = src.view<Component>();
		for (auto e : view)
		{
			UUID uuid = src.get<IDComponent>(e).ID;
			WF_CORE_ASSERT(enttMap.find(uuid) != enttMap.end());
			entt::entity dstEnttID = enttMap.at(uuid);

			auto& component = src.get<Component>(e);
			dst.emplace_or_replace<Component>(dstEnttID, component);
		}
	}

	template<typename Component>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		if (src.HasComponent<Component>())
			dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other)
	{
		Ref<Scene> newScene = CreateRef<Scene>();

		newScene->m_Name = other->m_Name;
		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene
		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto e : idView)
		{
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent<RelationshipComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<LifetimeComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<PolygonCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<AnimatorComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		return newScene;
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Empty Entity" : name;
		m_EntityMap[uuid] = (entt::entity)entity;
		return entity;
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto it = m_EntityMap.find(uuid);
		if (it != m_EntityMap.end() && m_Registry.valid(it->second))
			return Entity(it->second, this);

		// Fallback: linear scan (covers entities created outside CreateEntityWithUUID)
		auto view = m_Registry.view<IDComponent>();
		for (auto entity : view)
		{
			if (view.get<IDComponent>(entity).ID == uuid)
			{
				m_EntityMap[uuid] = entity;
				return Entity(entity, this);
			}
		}
		return Entity();
	}

	Entity Scene::GetParent(Entity entity)
	{
		if (!entity || !entity.HasComponent<RelationshipComponent>())
			return Entity();

		UUID parentUUID = entity.GetComponent<RelationshipComponent>().Parent;
		return parentUUID != 0 ? GetEntityByUUID(parentUUID) : Entity();
	}

	glm::mat4 Scene::GetWorldTransform(Entity entity)
	{
		if (!entity || !entity.HasComponent<TransformComponent>())
			return glm::mat4(1.0f);

		glm::mat4 transform = entity.GetComponent<TransformComponent>().GetTransform();

		// Compose with ancestor transforms (depth-limited as a cycle guard)
		Entity parent = GetParent(entity);
		int depth = 0;
		while (parent && parent.HasComponent<TransformComponent>() && depth++ < 64)
		{
			transform = parent.GetComponent<TransformComponent>().GetTransform() * transform;
			parent = GetParent(parent);
		}

		return transform;
	}

	void Scene::ParentEntity(Entity child, Entity parent)
	{
		if (!child || !parent || child == parent)
			return;

		// Cycle detection
		Entity currentParent = parent;
		while (currentParent && currentParent.HasComponent<RelationshipComponent>())
		{
			if (currentParent == child)
				return;
			UUID parentUUID = currentParent.GetComponent<RelationshipComponent>().Parent;
			currentParent = parentUUID != 0 ? GetEntityByUUID(parentUUID) : Entity{};
		}

		// Capture the child's world transform so it doesn't visually jump when parented
		glm::mat4 childWorld = GetWorldTransform(child);

		UnparentEntity(child);

		auto& childRel = child.HasComponent<RelationshipComponent>() ? child.GetComponent<RelationshipComponent>() : child.AddComponent<RelationshipComponent>();
		childRel.Parent = parent.GetUUID();

		auto& parentRel = parent.HasComponent<RelationshipComponent>() ? parent.GetComponent<RelationshipComponent>() : parent.AddComponent<RelationshipComponent>();
		parentRel.Children.push_back(child.GetUUID());

		// Convert the child's transform into the parent's local space
		if (child.HasComponent<TransformComponent>() && parent.HasComponent<TransformComponent>())
		{
			glm::mat4 localMatrix = glm::inverse(GetWorldTransform(parent)) * childWorld;
			glm::vec3 translation, rotation, scale;
			if (Math::DecomposeTransform(localMatrix, translation, rotation, scale))
			{
				auto& tc = child.GetComponent<TransformComponent>();
				tc.Translation = translation;
				tc.Rotation = rotation;
				tc.Scale = scale;
			}
		}
	}

	void Scene::UnparentEntity(Entity child)
	{
		if (!child || !child.HasComponent<RelationshipComponent>())
			return;

		auto& childRel = child.GetComponent<RelationshipComponent>();
		if (childRel.Parent != 0)
		{
			// Preserve the child's world transform when it becomes a root entity
			if (child.HasComponent<TransformComponent>())
			{
				glm::vec3 translation, rotation, scale;
				if (Math::DecomposeTransform(GetWorldTransform(child), translation, rotation, scale))
				{
					auto& tc = child.GetComponent<TransformComponent>();
					tc.Translation = translation;
					tc.Rotation = rotation;
					tc.Scale = scale;
				}
			}

			Entity parent = GetEntityByUUID(childRel.Parent);
			if (parent && parent.HasComponent<RelationshipComponent>())
			{
				auto& parentRel = parent.GetComponent<RelationshipComponent>();
				auto it = std::find(parentRel.Children.begin(), parentRel.Children.end(), child.GetUUID());
				if (it != parentRel.Children.end())
					parentRel.Children.erase(it);
			}
			childRel.Parent = 0;
		}
	}

	void Scene::DestroyEntity(Entity entity)
	{
		UnparentEntity(entity);
		if (entity.HasComponent<RelationshipComponent>())
		{
			auto children = entity.GetComponent<RelationshipComponent>().Children;
			for (auto childUUID : children)
			{
				Entity child = GetEntityByUUID(childUUID);
				if (child)
					UnparentEntity(child);
			}
		}

		// Remove the runtime physics body so it doesn't keep simulating (and leaking)
		if (m_PhysicsWorld && entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			if (rb2d.RuntimeBody)
			{
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				m_BodyEntityMap.erase(body);
				m_PhysicsWorld->DestroyBody(body);
				rb2d.RuntimeBody = nullptr;
			}
		}

		if (entity.HasComponent<IDComponent>())
			m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);
		m_Registry.destroy(entity);
	}

	void Scene::OnRuntimeStart()
	{
		m_IsRunning = true;
		m_PhysicsWorld = new b2World({ 0.0f, m_GravityY });
		m_BodyEntityMap.clear();

		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view)
		{
			// Stale pointers may survive from a previous run (the world was deleted)
			view.get<Rigidbody2DComponent>(e).RuntimeBody = nullptr;
			CreateRuntimePhysicsBody(Entity{ e, this });
		}

		LuaScriptEngine::OnRuntimeStart(this);
	}

	// Creates the Box2D body + fixtures for an entity. Safe to call for entities
	// spawned mid-runtime (e.g. prefabs instantiated from scripts); no-ops when
	// the physics world doesn't exist or the body was already created.
	void Scene::CreateRuntimePhysicsBody(Entity entity)
	{
		if (!m_PhysicsWorld || !entity || !entity.HasComponent<Rigidbody2DComponent>() || !entity.HasComponent<TransformComponent>())
			return;

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		if (rb2d.RuntimeBody)
			return;

		auto& transform = entity.GetComponent<TransformComponent>();

		// Physics operates in world space, so parented bodies start at their world transform
		glm::vec3 worldTranslation = transform.Translation;
		glm::vec3 worldRotation = transform.Rotation;
		glm::vec3 worldScale = transform.Scale; // fall back to local if decompose fails
		Math::DecomposeTransform(GetWorldTransform(entity), worldTranslation, worldRotation, worldScale);

		b2BodyDef bodyDef;
		bodyDef.type = Rigid2DTypeToBox2DBody(rb2d.Type);
		bodyDef.position.Set(worldTranslation.x, worldTranslation.y);
		bodyDef.angle = worldRotation.z;
		b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
		body->SetFixedRotation(rb2d.FixedRotation);
		rb2d.RuntimeBody = body;
		m_BodyEntityMap[body] = (uint32_t)(entt::entity)entity;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				boxShape.SetAsBox(bc2d.Size.x * worldScale.x, bc2d.Size.y * worldScale.y, b2Vec2(bc2d.Offset.x, bc2d.Offset.y), 0.0f);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc2d.Density > 0.0f ? bc2d.Density : 1.0f;
				fixtureDef.friction = bc2d.Friction;
				fixtureDef.restitution = bc2d.Restitution;
				fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
				fixtureDef.isSensor = bc2d.IsTrigger;
				body->CreateFixture(&fixtureDef);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				circleShape.m_p.Set(cc2d.Offset.x, cc2d.Offset.y);
				circleShape.m_radius = worldScale.x * cc2d.Radius;

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc2d.Density > 0.0f ? cc2d.Density : 1.0f;
				fixtureDef.friction = cc2d.Friction;
				fixtureDef.restitution = cc2d.Restitution;
				fixtureDef.restitutionThreshold = cc2d.RestitutionThreshold;
				fixtureDef.isSensor = cc2d.IsTrigger;
				body->CreateFixture(&fixtureDef);
			}

			if (entity.HasComponent<PolygonCollider2DComponent>())
			{
				auto& pc2d = entity.GetComponent<PolygonCollider2DComponent>();

				if (pc2d.Vertices.size() >= 3)
				{
					b2PolygonShape polyShape;
					std::vector<b2Vec2> b2Verts(pc2d.Vertices.size());
					for (size_t i = 0; i < pc2d.Vertices.size(); ++i)
					{
						b2Verts[i].Set(pc2d.Vertices[i].x * worldScale.x + pc2d.Offset.x, pc2d.Vertices[i].y * worldScale.y + pc2d.Offset.y);
					}
					polyShape.Set(b2Verts.data(), (int32)b2Verts.size());

					b2FixtureDef fixtureDef;
					fixtureDef.shape = &polyShape;
					fixtureDef.density = pc2d.Density > 0.0f ? pc2d.Density : 1.0f;
					fixtureDef.friction = pc2d.Friction;
					fixtureDef.restitution = pc2d.Restitution;
					fixtureDef.restitutionThreshold = pc2d.RestitutionThreshold;
					fixtureDef.isSensor = pc2d.IsTrigger;
					body->CreateFixture(&fixtureDef);
				}
			}

			if (body->GetFixtureList() == nullptr && rb2d.Type == Rigidbody2DComponent::BodyType::Dynamic)
			{
				b2PolygonShape boxShape;
				boxShape.SetAsBox(worldScale.x * 0.5f, worldScale.y * 0.5f);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = 1.0f;
				fixtureDef.friction = 0.5f;
				body->CreateFixture(&fixtureDef);
			}

			// Apply mass after all fixtures so Box2D doesn't overwrite it
			if (rb2d.Type == Rigidbody2DComponent::BodyType::Dynamic && rb2d.Mass > 0.0f)
			{
				b2MassData massData = body->GetMassData();
				massData.mass = rb2d.Mass;
				body->SetMassData(&massData);
			}
	}

	void Scene::OnRuntimeStop()
	{
		m_IsRunning = false;
		LuaScriptEngine::OnRuntimeStop(this);

		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}

	int Scene::OnUpdateRuntime(Timestep ts)
	{
		if (!m_IsPaused || m_StepFrames-- > 0)
		{
			// Update scripts
			{
				LuaScriptEngine::OnRuntimeUpdate(this, ts);

				m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
					{
						if (!nsc.Instance)
						{
							nsc.Instance = nsc.InstanciateScript();
							nsc.Instance->m_Entity = Entity{ entity, this };
							nsc.Instance->OnCreate();
						}
						nsc.Instance->OnUpdate(ts);
					});
			}

			// Lifetime — tick down and destroy expired entities.
			// Collect first, then destroy, to avoid invalidating the view mid-iteration.
			{
				std::vector<Entity> expired;
				auto lifetimeView = m_Registry.view<LifetimeComponent>();
				for (auto e : lifetimeView)
				{
					auto& lifetime = lifetimeView.get<LifetimeComponent>(e);
					lifetime.RemainingTime -= ts;
					if (lifetime.RemainingTime <= 0.0f)
						expired.push_back(Entity{ e, this });
				}
				for (auto entity : expired)
					DestroyEntity(entity);
			}

			// Physics
			{
				const int32_t velocityIterations = 6;
				const int32_t positionIterations = 2;

				m_PhysicsAccumulator += ts;
				if (m_PhysicsAccumulator > 0.2f)
					m_PhysicsAccumulator = 0.2f;

				while (m_PhysicsAccumulator >= m_PhysicsFixedStep)
				{
					m_PhysicsWorld->Step(m_PhysicsFixedStep, velocityIterations, positionIterations);
					m_PhysicsAccumulator -= m_PhysicsFixedStep;
				}

				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;
					if (!body)
					{
						// Entity was spawned mid-runtime (e.g. prefab instantiated
						// from a script) — create its body now instead of crashing.
						CreateRuntimePhysicsBody(entity);
						body = (b2Body*)rb2d.RuntimeBody;
						if (!body)
							continue;
					}

					const auto& position = body->GetPosition();

					// Physics reports world-space transforms; convert back to
					// parent-relative (local) space for parented entities.
					Entity parent = GetParent(entity);
					if (parent && parent.HasComponent<TransformComponent>())
					{
						glm::mat4 parentWorld = GetWorldTransform(parent);
						glm::mat4 world = GetWorldTransform(entity);
						glm::vec3 worldPos = { position.x, position.y, world[3].z };
						glm::vec3 localPos = glm::vec3(glm::inverse(parentWorld) * glm::vec4(worldPos, 1.0f));

						glm::vec3 pTrans, pRot, pScale;
						if (!Math::DecomposeTransform(parentWorld, pTrans, pRot, pScale))
							pRot = glm::vec3(0.0f);

						transform.Translation.x = localPos.x;
						transform.Translation.y = localPos.y;
						transform.Rotation.z = body->GetAngle() - pRot.z;
					}
					else
					{
						transform.Translation.x = position.x;
						transform.Translation.y = position.y;
						transform.Rotation.z = body->GetAngle();
					}
				}
			}
		}

		// Render 2D — always runs regardless of pause state
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		CameraComponent* mainCameraComp = nullptr;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = GetWorldTransform(Entity{ entity, this });
					mainCameraComp = &view.get<CameraComponent>(entity);
					break;
				}
			}
		}

		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			if (mainCameraComp && mainCameraComp->BackgroundImage)
			{
				float orthoSize = mainCameraComp->Camera.GetOrthographicSize();
				float aspectRatio = mainCameraComp->Camera.GetAspectRatio();
				glm::vec3 camPos = cameraTransform[3];
				glm::mat4 bgTransform = glm::translate(glm::mat4(1.0f), glm::vec3(camPos.x, camPos.y, -0.9f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(orthoSize * aspectRatio * 2.0f, orthoSize * 2.0f, 1.0f));
				Renderer2D::DrawQuad(bgTransform, mainCameraComp->BackgroundImage, mainCameraComp->BackgroundTilingFactor);
			}

		// Draw sprites — skip disabled entities
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>, entt::exclude<DisabledComponent>);
		for (auto entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(Entity{ entity, this });

			auto* animator = m_Registry.try_get<AnimatorComponent>(entity);
			if (animator)
			{
				animator->Update(ts);
				Ref<SubTexture2D> subTexture = animator->GetCurrentSubTexture();
				if (subTexture)
				{
					Renderer2D::DrawQuad(worldTransform, subTexture, sprite.TilingFactor, sprite.Color, (int)entity);
					continue;
				}
			}

			Renderer2D::DrawSprite(worldTransform, sprite, (int)entity);
		}

		// Draw circles — skip disabled entities
		auto view = m_Registry.view<TransformComponent, CircleRendererComponent>(entt::exclude<DisabledComponent>);
		for (auto entity : view)
		{
			auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
			Renderer2D::DrawCircle(GetWorldTransform(Entity{ entity, this }), circle.Color, circle.Thickness, circle.Fade, (int)entity);
		}

			Renderer2D::EndScene();
		}

		// Scene change — checked after render so the current frame still draws
		int pending = LuaScriptEngine::GetPendingSceneChange();
		if (pending != -1)
			LuaScriptEngine::ClearPendingSceneChange();
		return pending;
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
	{
		Renderer2D::BeginScene(camera);

		Entity primaryCamEntity = GetPrimaryCameraEntity();
		if (primaryCamEntity && primaryCamEntity.HasComponent<CameraComponent>())
		{
			auto& camComp = primaryCamEntity.GetComponent<CameraComponent>();
			if (camComp.BackgroundImage)
			{
				glm::vec3 camWorldPos = glm::vec3(GetWorldTransform(primaryCamEntity)[3]);
				float orthoSize = camComp.Camera.GetOrthographicSize();
				float aspectRatio = camComp.Camera.GetAspectRatio();
				glm::mat4 bgTransform = glm::translate(glm::mat4(1.0f), glm::vec3(camWorldPos.x, camWorldPos.y, -0.9f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(orthoSize * aspectRatio * 2.0f, orthoSize * 2.0f, 1.0f));
				Renderer2D::DrawQuad(bgTransform, camComp.BackgroundImage, camComp.BackgroundTilingFactor);
			}
		}

		// Draw sprites
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(Entity{ entity, this });

			auto* animator = m_Registry.try_get<AnimatorComponent>(entity);
			if (animator)
			{
				animator->Update(ts);
				Ref<SubTexture2D> subTexture = animator->GetCurrentSubTexture();
				if (subTexture)
				{
					Renderer2D::DrawQuad(worldTransform, subTexture, sprite.TilingFactor, sprite.Color, (int)entity);
					continue;
				}
			}

			Renderer2D::DrawSprite(worldTransform, sprite, (int)entity);
		}

		// Draw circles
		auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
		for (auto entity : view)
		{
			auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

			Renderer2D::DrawCircle(GetWorldTransform(Entity{ entity, this }), circle.Color, circle.Thickness, circle.Fade, (int)entity);
		}

		Renderer2D::EndScene();
	}

	void Scene::Step(int frames)
	{
		m_StepFrames = frames;
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize the cameras that haven't locked their viewport aspect ratio
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		// Attach the duplicate to the same parent as the original
		return DuplicateEntityRecursive(entity, GetParent(entity));
	}

	Entity Scene::DuplicateEntityRecursive(Entity entity, Entity parent)
	{
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);

		// Copy everything except RelationshipComponent — copying it verbatim would
		// corrupt the hierarchy (children pointing at the original parent, the
		// duplicate claiming the original's children, etc.).
		CopyComponentIfExists<ScriptComponent>(newEntity, entity);
		CopyComponentIfExists<LifetimeComponent>(newEntity, entity);
		CopyComponentIfExists<TransformComponent>(newEntity, entity);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CircleRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CameraComponent>(newEntity, entity);
		CopyComponentIfExists<NativeScriptComponent>(newEntity, entity);
		CopyComponentIfExists<Rigidbody2DComponent>(newEntity, entity);
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<PolygonCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<AnimatorComponent>(newEntity, entity);

		// The copies must not share the original's Box2D body/fixture pointers —
		// that would corrupt the simulation and double-destroy bodies later.
		if (newEntity.HasComponent<Rigidbody2DComponent>())
		{
			newEntity.GetComponent<Rigidbody2DComponent>().RuntimeBody = nullptr;
			CreateRuntimePhysicsBody(newEntity); // no-op when not running
		}
		if (newEntity.HasComponent<BoxCollider2DComponent>())
			newEntity.GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
		if (newEntity.HasComponent<CircleCollider2DComponent>())
			newEntity.GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;
		if (newEntity.HasComponent<PolygonCollider2DComponent>())
			newEntity.GetComponent<PolygonCollider2DComponent>().RuntimeFixture = nullptr;

		// Link into the hierarchy directly (not via ParentEntity, which would
		// reinterpret the copied local transform as a world transform).
		if (parent)
		{
			auto& childRel = newEntity.HasComponent<RelationshipComponent>()
				? newEntity.GetComponent<RelationshipComponent>()
				: newEntity.AddComponent<RelationshipComponent>();
			childRel.Parent = parent.GetUUID();

			auto& parentRel = parent.HasComponent<RelationshipComponent>()
				? parent.GetComponent<RelationshipComponent>()
				: parent.AddComponent<RelationshipComponent>();
			parentRel.Children.push_back(newEntity.GetUUID());
		}

		// Duplicate children recursively
		if (entity.HasComponent<RelationshipComponent>())
		{
			auto children = entity.GetComponent<RelationshipComponent>().Children; // copy — list mutates below
			for (auto childUUID : children)
			{
				Entity child = GetEntityByUUID(childUUID);
				if (child)
					DuplicateEntityRecursive(child, newEntity);
			}
		}

		return newEntity;
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{ entity, this };
		}
		return {};
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		static_assert(sizeof(T) != sizeof(T), "Unsupported component type in OnComponentAdded.");
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component) {}

	template<>
	void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component) {}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component) {}

	template<>
	void Scene::OnComponentAdded<LifetimeComponent>(Entity entity, LifetimeComponent& component) {}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) {}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<AnimatorComponent>(Entity entity, AnimatorComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) {}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) {}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<PolygonCollider2DComponent>(Entity entity, PolygonCollider2DComponent& component) {}
}