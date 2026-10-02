#include "wfpch.h"
#include "CSharpScriptHost.h"
#include "CSharpScriptEngine.h"

#include "Waffle/Core/Log.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Core/Input.h"
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"
#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Audio/AudioEngine.h"
#include "Waffle/Renderer/Texture.h"

#include <box2d/b2_body.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world.h>
#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>
#include <imgui.h>

#include <cctype>
#include <cstring>

namespace Waffle {

	namespace {

		// Shared helpers

		bool PathEscapesAssetRoot(const std::string& path)
		{
			std::filesystem::path p = path;
			for (const auto& part : p)
			{
				std::string s = part.string();
				if (s == ".." || (s.size() >= 2 && s[1] == ':'))
					return true;
			}
			return false;
		}

		// Input arbitration: gameplay input is gated only outside the game viewport rect in the editor; the exported runtime is never gated.
		bool GameplayMouseBlocked()
		{
			ImGuiIO& io = ImGui::GetIO();
			if (!io.WantCaptureMouse)
				return false;
			if (!CSharpScriptEngine::HasGameViewport())
				return false;

			glm::vec2 m = Input::GetMousePosition();
			glm::vec2 o = CSharpScriptEngine::GetGameViewportOrigin();
			glm::vec2 sz = CSharpScriptEngine::GetGameViewportSize();
			if (sz.x <= 0.0f || sz.y <= 0.0f)
				return false;
			bool insideGame = (m.x >= o.x && m.y >= o.y && m.x < o.x + sz.x && m.y < o.y + sz.y);
			return !insideGame;
		}

		bool GameplayKeyboardBlocked()
		{
			return ImGui::GetIO().WantTextInput;
		}

		Scene* ActiveScene() { return CSharpScriptEngine::GetSceneContext(); }

		Entity GetEntity(uint32_t id)
		{
			return Entity{ (entt::entity)id, ActiveScene() };
		}

		void SyncBodyToTransform(Entity entity)
		{
			if (!entity.HasComponent<Rigidbody2DComponent>())
				return;
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			b2Body* body = (b2Body*)rb2d.RuntimeBody;
			if (!body)
				return;
			const auto& tc = entity.GetComponent<TransformComponent>();
			rb2d.RuntimePrevValid = false; // teleports must not be interpolation-smeared
			body->SetTransform(b2Vec2(tc.Translation.x, tc.Translation.y), tc.Rotation.z);
			body->SetAwake(true);
		}

		// Editor gizmo preview: mutating calls no-op with a warning (scripts may run OnDrawGizmos while editing - they must only read and draw).
		bool GizmoPassGate(const char* apiName)
		{
			if (CSharpScriptEngine::IsEditorGizmoPass())
			{
				WF_CORE_WARN("Editor gizmo preview: {0} has no effect outside play mode", apiName);
				return true;
			}
			return false;
		}

		// Copies a std::string into a caller buffer; returns needed length (excluding terminator), or -1 when the buffer is too small.
		int CopyToBuffer(const std::string& text, char* buffer, int bufferSize)
		{
			if (!buffer || bufferSize <= 0)
				return -1;
			size_t needed = text.size() + 1;
			if (needed > (size_t)bufferSize)
				return -1;
			std::memcpy(buffer, text.c_str(), needed);
			return (int)text.size();
		}

		// Logging

		void WF_LogInfo(const char* utf8) { if (utf8) WF_INFO("{0}", utf8); }
		void WF_LogWarn(const char* utf8) { if (utf8) WF_WARN("{0}", utf8); }
		void WF_LogError(const char* utf8) { if (utf8) WF_ERROR("{0}", utf8); }

		// Scene / framework

		void WF_GetViewportSize(float* outW, float* outH)
		{
			Scene* scene = ActiveScene();
			if (!scene) { *outW = 0; *outH = 0; return; }
			*outW = (float)scene->GetViewportWidth();
			*outH = (float)scene->GetViewportHeight();
		}

		void WF_ScreenToWorld(float sx, float sy, float* outWx, float* outWy)
		{
			Scene* scene = ActiveScene();
			if (!scene) { *outWx = sx; *outWy = sy; return; }

			Entity camEntity = scene->GetPrimaryCameraEntity();
			if (!camEntity || !camEntity.HasComponent<CameraComponent>() || !camEntity.HasComponent<TransformComponent>())
			{
				*outWx = sx; *outWy = sy;
				return;
			}

			const auto& camComp = camEntity.GetComponent<CameraComponent>();
			// WORLD position - a parented camera's local translation is not where the view is centered.
			glm::vec3 camPos = glm::vec3(scene->GetWorldTransform(camEntity)[3]);
			float orthoSize = camComp.Camera.GetOrthographicSize();
			float aspectRatio = camComp.Camera.GetAspectRatio();
			float vw = (float)scene->GetViewportWidth();
			float vh = (float)scene->GetViewportHeight();
			if (vw == 0 || vh == 0) { *outWx = sx; *outWy = sy; return; }

			*outWx = camPos.x + (sx / vw - 0.5f) * orthoSize * aspectRatio;
			*outWy = camPos.y + (0.5f - sy / vh) * orthoSize; // Y flipped
		}

		int WF_EditorGizmoPass()
		{
			return CSharpScriptEngine::IsEditorGizmoPass() ? 1 : 0;
		}

		// Input

		int WF_IsKeyPressed(int code)
		{
			if (GameplayKeyboardBlocked())
				return 0;
			return (code != 0) && Input::IsKeyPressed((KeyCode)code) ? 1 : 0;
		}

		int WF_IsMouseButtonPressed(int code)
		{
			if (GameplayMouseBlocked())
				return 0;
			return Input::IsMouseButtonPressed((MouseCode)code) ? 1 : 0;
		}

		int WF_IsKeyJustPressed(int code)
		{
			if (code == 0 || GameplayKeyboardBlocked())
				return 0;
			CSharpScriptEngine::TrackKey(code);
			bool prev = CSharpScriptEngine::s_PrevKeyStates.count(code) ? CSharpScriptEngine::s_PrevKeyStates[code] : false;
			bool curr = CSharpScriptEngine::s_CurrKeyStates.count(code) ? CSharpScriptEngine::s_CurrKeyStates[code] : false;
			return (curr && !prev) ? 1 : 0;
		}

		int WF_IsKeyJustReleased(int code)
		{
			if (code == 0)
				return 0;
			CSharpScriptEngine::TrackKey(code);
			bool prev = CSharpScriptEngine::s_PrevKeyStates.count(code) ? CSharpScriptEngine::s_PrevKeyStates[code] : false;
			bool curr = CSharpScriptEngine::s_CurrKeyStates.count(code) ? CSharpScriptEngine::s_CurrKeyStates[code] : false;
			return (!curr && prev) ? 1 : 0;
		}

		int WF_IsMouseJustPressed(int code)
		{
			if (GameplayMouseBlocked())
				return 0;
			CSharpScriptEngine::TrackMouse(code);
			bool prev = CSharpScriptEngine::s_PrevMouseStates.count(code) ? CSharpScriptEngine::s_PrevMouseStates[code] : false;
			bool curr = CSharpScriptEngine::s_CurrMouseStates.count(code) ? CSharpScriptEngine::s_CurrMouseStates[code] : false;
			return (curr && !prev) ? 1 : 0;
		}

		int WF_IsMouseJustReleased(int code)
		{
			if (GameplayMouseBlocked())
				return 0;
			CSharpScriptEngine::TrackMouse(code);
			bool prev = CSharpScriptEngine::s_PrevMouseStates.count(code) ? CSharpScriptEngine::s_PrevMouseStates[code] : false;
			bool curr = CSharpScriptEngine::s_CurrMouseStates.count(code) ? CSharpScriptEngine::s_CurrMouseStates[code] : false;
			return (!curr && prev) ? 1 : 0;
		}

		void WF_GetMousePosition(float* outX, float* outY)
		{
			// Viewport-relative: pairs with ScreenToWorld and the viewport size.
			glm::vec2 pos = Input::GetMousePosition() - CSharpScriptEngine::GetGameViewportOrigin();
			*outX = pos.x;
			*outY = pos.y;
		}

		float WF_GetAxis(const char* axisUtf8)
		{
			if (!axisUtf8)
				return 0.0f;
			return Input::GetAxis(axisUtf8);
		}

		// Scene management

		void WF_ChangeScene(int index) { if (!GizmoPassGate("ChangeScene")) CSharpScriptEngine::SetPendingSceneChange(index); }
		int  WF_GetCurrentSceneIndex() { return CSharpScriptEngine::GetCurrentSceneIndex(); }
		void WF_SetCurrentSceneIndex(int index) { CSharpScriptEngine::SetCurrentSceneIndex(index); }
		void WF_RequestQuit() { if (!GizmoPassGate("Quit")) CSharpScriptEngine::RequestQuit(); }

		// Entity management

		int WF_CreateEntity(const char* nameUtf8, float x, float y)
		{
			if (GizmoPassGate("CreateEntity"))
				return -1;
			Scene* scene = ActiveScene();
			if (!scene)
				return -1;

			Entity entity = scene->CreateEntity(nameUtf8 ? nameUtf8 : "Entity");
			if (entity.HasComponent<TransformComponent>())
			{
				auto& tc = entity.GetComponent<TransformComponent>();
				tc.Translation.x = x;
				tc.Translation.y = y;
			}
			return (int)(uint32_t)(entt::entity)entity;
		}

		void WF_DestroyEntity(uint32_t entityId)
		{
			if (GizmoPassGate("DestroyEntity"))
				return;
			// Deferred: processed after the script update loop (iterator invalidation).
			CSharpScriptEngine::s_PendingDestroys.push_back(entityId);
		}

		void WF_DestroyEntityDelayed(uint32_t entityId, float delay)
		{
			if (GizmoPassGate("DestroyEntityDelayed"))
				return;
			CSharpScriptEngine::s_DelayedDestroys.push_back({ entityId, delay });
		}

		int WF_CloneEntity(uint32_t entityId)
		{
			if (GizmoPassGate("CloneEntity"))
				return -1;
			Scene* scene = ActiveScene();
			if (!scene)
				return -1;
			Entity source = GetEntity(entityId);
			if (!source)
				return -1;
			Entity newEntity = scene->DuplicateEntity(source);
			return newEntity ? (int)(uint32_t)(entt::entity)newEntity : -1;
		}

		int WF_InstantiatePrefab(const char* pathUtf8, float x, float y)
		{
			if (GizmoPassGate("InstantiatePrefab"))
				return -1;
			if (!pathUtf8)
				return -1;
			if (PathEscapesAssetRoot(pathUtf8))
			{
				WF_CORE_WARN("InstantiatePrefab: rejected path with '..' segments: '{0}'", pathUtf8);
				return -1;
			}

			Scene* scene = ActiveScene();
			if (!scene)
				return -1;

			std::filesystem::path fullPath = CSharpScriptEngine::GetAssetPath() / pathUtf8;
			if (!VFS::Exists(fullPath))
				fullPath = pathUtf8;
			if (!VFS::Exists(fullPath))
			{
				WF_CORE_ERROR("InstantiatePrefab: Prefab file '{0}' not found", pathUtf8);
				return -1;
			}

			Entity entity = SceneSerializer::DeserializePrefabToEntity(scene, fullPath.string(), x, y);
			if (!entity)
				return -1;

			// Physics body right away so the physics write-back works this frame.
			scene->CreateRuntimePhysicsBody(entity);

			// Init + OnStart scripts immediately, or the instance registry stays empty and EntityUpdate silently skips the entity.
			CSharpScriptEngine::InitScriptsForEntity(scene, entity);

			return (int)(uint32_t)(entt::entity)entity;
		}

		int WF_GetEntityName(uint32_t entityId, char* buffer, int bufferSize)
		{
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TagComponent>())
				return CopyToBuffer("", buffer, bufferSize);
			return CopyToBuffer(entity.GetComponent<TagComponent>().Tag, buffer, bufferSize);
		}

		int WF_FindEntityByName(const char* nameUtf8)
		{
			if (!nameUtf8)
				return -1;
			Scene* scene = ActiveScene();
			if (!scene)
				return -1;

			auto view = scene->GetRegistry().view<TagComponent>();
			for (auto entityID : view)
			{
				const auto& tag = view.get<TagComponent>(entityID);
				if (tag.Tag == nameUtf8)
					return (int)(uint32_t)entityID;
			}
			return -1;
		}

		int WF_FindAllEntitiesByName(const char* nameUtf8, uint32_t* outIds, int maxCount)
		{
			int count = 0;
			if (!nameUtf8 || !outIds)
				return 0;
			Scene* scene = ActiveScene();
			if (!scene)
				return 0;

			auto view = scene->GetRegistry().view<TagComponent>();
			for (auto entityID : view)
			{
				const auto& tag = view.get<TagComponent>(entityID);
				if (tag.Tag == nameUtf8 && count < maxCount)
					outIds[count++] = (uint32_t)entityID;
			}
			return count;
		}

		int WF_GetAllEntities(uint32_t* outIds, int maxCount)
		{
			int count = 0;
			if (!outIds)
				return 0;
			Scene* scene = ActiveScene();
			if (!scene)
				return 0;

			auto view = scene->GetRegistry().view<TagComponent>();
			for (auto entityID : view)
			{
				if (count < maxCount)
					outIds[count++] = (uint32_t)entityID;
			}
			return count;
		}

		int WF_GetParent(uint32_t entityId)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return -1;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<RelationshipComponent>())
				return -1;

			UUID parent = entity.GetComponent<RelationshipComponent>().Parent;
			if (parent == 0)
				return -1;
			Entity parentEntity = scene->GetEntityByUUID(parent);
			return parentEntity ? (int)(uint32_t)(entt::entity)parentEntity : -1;
		}

		int WF_GetChildren(uint32_t entityId, uint32_t* outIds, int maxCount)
		{
			int count = 0;
			Scene* scene = ActiveScene();
			if (!scene || !outIds)
				return 0;

			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<RelationshipComponent>())
				return 0;

			for (UUID child : entity.GetComponent<RelationshipComponent>().Children)
			{
				Entity childEntity = scene->GetEntityByUUID(child);
				if (childEntity && count < maxCount)
					outIds[count++] = (uint32_t)(entt::entity)childEntity;
			}
			return count;
		}

		void WF_SetParent(uint32_t childId, uint32_t parentId)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity child = GetEntity(childId);
			Entity parent = GetEntity(parentId);
			if (child && parent)
				scene->ParentEntity(child, parent);
		}

		void WF_Unparent(uint32_t childId)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity child = GetEntity(childId);
			if (child)
				scene->UnparentEntity(child);
		}

		void WF_SetActive(uint32_t entityId, int active)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			entt::registry& registry = scene->GetRegistry();
			if (!registry.valid((entt::entity)entityId))
				return;

			bool wasDisabled = registry.all_of<DisabledComponent>((entt::entity)entityId);
			if (active)
			{
				if (wasDisabled)
				{
					registry.remove<DisabledComponent>((entt::entity)entityId);
					CSharpScriptEngine::FireEnable(entityId, true);
				}
			}
			else
			{
				if (!wasDisabled)
				{
					registry.emplace<DisabledComponent>((entt::entity)entityId);
					CSharpScriptEngine::FireEnable(entityId, false);
				}
			}
		}

		int WF_IsActive(uint32_t entityId)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return 1;
			entt::registry& registry = scene->GetRegistry();
			if (!registry.valid((entt::entity)entityId))
				return 1;
			return registry.all_of<DisabledComponent>((entt::entity)entityId) ? 0 : 1;
		}

		// Transform

		void WF_Translate(uint32_t entityId, float dx, float dy, float dz)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TransformComponent>())
				return;

			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation.x += dx;
			tc.Translation.y += dy;
			tc.Translation.z += dz;

			if (entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
				{
					switch (rb2d.Type)
					{
					case Rigidbody2DComponent::BodyType::Kinematic:
						SyncBodyToTransform(entity);
						break;
					case Rigidbody2DComponent::BodyType::Dynamic:
						// The solver owns position; Translate on a dynamic body is a misuse.
						WF_CORE_WARN("Translate: called on Dynamic body (entity {0}). Use SetLinearVelocity or ApplyForce instead.", entityId);
						tc.Translation.x -= dx;
						tc.Translation.y -= dy;
						tc.Translation.z -= dz;
						break;
					case Rigidbody2DComponent::BodyType::Static:
						tc.Translation.x -= dx;
						tc.Translation.y -= dy;
						tc.Translation.z -= dz;
						break;
					}
				}
			}
		}

		void WF_SetPosition(uint32_t entityId, float x, float y, float z)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TransformComponent>())
				return;

			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation.x = x;
			tc.Translation.y = y;
			tc.Translation.z = z;
			SyncBodyToTransform(entity);
		}

		void WF_GetPosition(uint32_t entityId, float* outX, float* outY, float* outZ)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<TransformComponent>())
			{
				const auto& tc = entity.GetComponent<TransformComponent>();
				*outX = tc.Translation.x;
				*outY = tc.Translation.y;
				*outZ = tc.Translation.z;
				return;
			}
			*outX = 0; *outY = 0; *outZ = 0;
		}

		void WF_SetRotation(uint32_t entityId, float x, float y, float z)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TransformComponent>())
				return;
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Rotation.x = x;
			tc.Rotation.y = y;
			tc.Rotation.z = z;
			if (entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
				{
					rb2d.RuntimePrevValid = false;
					body->SetTransform(body->GetPosition(), tc.Rotation.z);
				}
			}
		}

		void WF_SetRotation2D(uint32_t entityId, float z)
		{
			Scene* scene = ActiveScene();
			if (!scene)
				return;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TransformComponent>())
				return;
			entity.GetComponent<TransformComponent>().Rotation.z = z;
			if (entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
				{
					rb2d.RuntimePrevValid = false;
					body->SetTransform(body->GetPosition(), entity.GetComponent<TransformComponent>().Rotation.z);
				}
			}
		}

		void WF_GetRotation(uint32_t entityId, float* outX, float* outY, float* outZ)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<TransformComponent>())
			{
				const auto& tc = entity.GetComponent<TransformComponent>();
				*outX = tc.Rotation.x;
				*outY = tc.Rotation.y;
				*outZ = tc.Rotation.z;
				return;
			}
			*outX = 0; *outY = 0; *outZ = 0;
		}

		void WF_SetScale(uint32_t entityId, float x, float y, float z)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<TransformComponent>())
			{
				auto& tc = entity.GetComponent<TransformComponent>();
				tc.Scale.x = x;
				tc.Scale.y = y;
				tc.Scale.z = z;
			}
		}

		void WF_GetScale(uint32_t entityId, float* outX, float* outY, float* outZ)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<TransformComponent>())
			{
				const auto& tc = entity.GetComponent<TransformComponent>();
				*outX = tc.Scale.x;
				*outY = tc.Scale.y;
				*outZ = tc.Scale.z;
				return;
			}
			*outX = 1; *outY = 1; *outZ = 1;
		}

		// Physics 2D

		void WF_SetLinearVelocity(uint32_t entityId, float vx, float vy)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
				{
					body->SetAwake(true);
					body->SetLinearVelocity(b2Vec2(vx, vy));
				}
			}
		}

		void WF_GetLinearVelocity(uint32_t entityId, float* outVx, float* outVy)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
				{
					b2Vec2 vel = body->GetLinearVelocity();
					*outVx = vel.x;
					*outVy = vel.y;
					return;
				}
			}
			*outVx = 0; *outVy = 0;
		}

		void WF_ApplyLinearImpulse(uint32_t entityId, float ix, float iy)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
				{
					body->SetAwake(true);
					body->ApplyLinearImpulseToCenter(b2Vec2(ix, iy), true);
				}
			}
		}

		void WF_ApplyForce(uint32_t entityId, float fx, float fy)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
				{
					body->SetAwake(true);
					body->ApplyForceToCenter(b2Vec2(fx, fy), true);
				}
			}
		}

		float WF_GetAngularVelocity(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
					return body->GetAngularVelocity();
			}
			return 0.0f;
		}

		void WF_SetAngularVelocity(uint32_t entityId, float omega)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body) { body->SetAwake(true); body->SetAngularVelocity(omega); }
			}
		}

		void WF_ApplyTorque(uint32_t entityId, float torque)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body) { body->SetAwake(true); body->ApplyTorque(torque, true); }
			}
		}

		void WF_ApplyAngularImpulse(uint32_t entityId, float impulse)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body) { body->SetAwake(true); body->ApplyAngularImpulse(impulse, true); }
			}
		}

		void WF_SetSensor(uint32_t entityId, int sensor)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return;
			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc = entity.GetComponent<BoxCollider2DComponent>();
				bc.IsTrigger = sensor != 0;
				if (bc.RuntimeFixture)
					((b2Fixture*)bc.RuntimeFixture)->SetSensor(sensor != 0);
			}
			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc = entity.GetComponent<CircleCollider2DComponent>();
				cc.IsTrigger = sensor != 0;
				if (cc.RuntimeFixture)
					((b2Fixture*)cc.RuntimeFixture)->SetSensor(sensor != 0);
			}
		}

		int WF_IsSensor(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return 0;
			if (entity.HasComponent<BoxCollider2DComponent>())
				return entity.GetComponent<BoxCollider2DComponent>().IsTrigger ? 1 : 0;
			if (entity.HasComponent<CircleCollider2DComponent>())
				return entity.GetComponent<CircleCollider2DComponent>().IsTrigger ? 1 : 0;
			return 0;
		}

		void WF_SetGravityScale(uint32_t entityId, float scale)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb = entity.GetComponent<Rigidbody2DComponent>();
				if (rb.RuntimeBody)
					((b2Body*)rb.RuntimeBody)->SetGravityScale(scale);
			}
		}

		float WF_GetGravityScale(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
					return body->GetGravityScale();
				}
			return 1.0f;
		}

		void WF_SetFixedRotation(uint32_t entityId, int fixedRotation)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb = entity.GetComponent<Rigidbody2DComponent>();
				rb.FixedRotation = fixedRotation != 0;
				b2Body* body = (b2Body*)rb.RuntimeBody;
				if (body)
					body->SetFixedRotation(fixedRotation != 0);
			}
		}

		int WF_IsFixedRotation(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
					return body->IsFixedRotation() ? 1 : 0;
				return entity.GetComponent<Rigidbody2DComponent>().FixedRotation ? 1 : 0;
			}
			return 0;
		}

		void WF_SetFriction(uint32_t entityId, float friction)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return;
			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc = entity.GetComponent<BoxCollider2DComponent>();
				bc.Friction = friction;
				if (bc.RuntimeFixture)
					((b2Fixture*)bc.RuntimeFixture)->SetFriction(friction);
			}
			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc = entity.GetComponent<CircleCollider2DComponent>();
				cc.Friction = friction;
				if (cc.RuntimeFixture)
					((b2Fixture*)cc.RuntimeFixture)->SetFriction(friction);
			}
		}

		void WF_SetRestitution(uint32_t entityId, float restitution)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return;
			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc = entity.GetComponent<BoxCollider2DComponent>();
				bc.Restitution = restitution;
				if (bc.RuntimeFixture)
					((b2Fixture*)bc.RuntimeFixture)->SetRestitution(restitution);
			}
			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc = entity.GetComponent<CircleCollider2DComponent>();
				cc.Restitution = restitution;
				if (cc.RuntimeFixture)
					((b2Fixture*)cc.RuntimeFixture)->SetRestitution(restitution);
			}
		}

		void WF_SetRigidBodyType(uint32_t entityId, int type)
		{
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
				return;
			auto& rb = entity.GetComponent<Rigidbody2DComponent>();
			b2BodyType bodyType = b2_dynamicBody;
			if (type == 0) bodyType = b2_staticBody;
			else if (type == 1) bodyType = b2_kinematicBody;

			rb.Type = (Rigidbody2DComponent::BodyType)type;
			if (rb.RuntimeBody)
				((b2Body*)rb.RuntimeBody)->SetType(bodyType);
		}

		int WF_GetRigidBodyType(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body)
				{
					b2BodyType t = body->GetType();
					if (t == b2_staticBody) return 0;
					if (t == b2_kinematicBody) return 1;
					return 2;
				}
			}
			return 0;
		}

		int WF_Raycast(uint32_t entityId, float offsetX, float offsetY, float dirX, float dirY, float distance,
			int32_t* outEntityId, float* outHitX, float* outHitY, float* outNormalX, float* outNormalY)
		{
			*outEntityId = -1; *outHitX = 0; *outHitY = 0; *outNormalX = 0; *outNormalY = 0;

			Scene* scene = ActiveScene();
			if (!scene || !scene->GetPhysicsWorld())
				return 0;
			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<TransformComponent>())
				return 0;

			const auto& tc = entity.GetComponent<TransformComponent>();
			b2Vec2 origin(tc.Translation.x + offsetX, tc.Translation.y + offsetY);
			b2Vec2 end(origin.x + dirX * distance, origin.y + dirY * distance);
			if (origin.x == end.x && origin.y == end.y)
				return 0;

			struct RayCallback : public b2RayCastCallback
			{
				uint32_t ignoreID;
				bool     hit = false;
				int32_t  hitEntity = -1;
				b2Vec2   hitPoint = { 0, 0 };
				b2Vec2   hitNormal = { 0, 0 };
				float    minFraction = 1.0f;
				std::unordered_map<b2Body*, uint32_t>* bodyMap;

				float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override
				{
					b2Body* body = fixture->GetBody();
					auto it = bodyMap->find(body);
					if (it != bodyMap->end() && it->second == ignoreID)
						return -1.0f; // skip self

					if (fraction < minFraction)
					{
						minFraction = fraction;
						hit = true;
						hitPoint = point;
						hitNormal = normal;
						hitEntity = (it != bodyMap->end()) ? (int32_t)it->second : -1;
					}
					return fraction; // continue to find closest
				}
			};

			RayCallback cb;
			cb.ignoreID = entityId;
			cb.bodyMap = &scene->GetBodyEntityMap();
			scene->GetPhysicsWorld()->RayCast(&cb, origin, end);

			if (!cb.hit)
				return 0;
			*outEntityId = cb.hitEntity;
			*outHitX = cb.hitPoint.x;
			*outHitY = cb.hitPoint.y;
			*outNormalX = cb.hitNormal.x;
			*outNormalY = cb.hitNormal.y;
			return 1;
		}

		int WF_OverlapCircle(float cx, float cy, float radius, uint32_t excludeId, uint32_t* outIds, int maxCount)
		{
			int count = 0;
			Scene* scene = ActiveScene();
			if (!scene || !scene->GetPhysicsWorld() || !outIds)
				return 0;

			struct OverlapCallback : public b2QueryCallback
			{
				std::vector<uint32_t> hits;
				uint32_t excludeID;
				float cx, cy, radius;
				std::unordered_map<b2Body*, uint32_t>* bodyMap;

				bool ReportFixture(b2Fixture* fixture) override
				{
					b2Body* body = fixture->GetBody();
					auto it = bodyMap->find(body);
					if (it == bodyMap->end() || it->second == excludeID)
						return true;

					b2Vec2 pos = body->GetPosition();
					float dx = pos.x - cx, dy = pos.y - cy;
					if (dx * dx + dy * dy <= radius * radius)
					{
						if (std::find(hits.begin(), hits.end(), it->second) == hits.end())
							hits.push_back(it->second);
					}
					return true;
				}
			} cb;

			cb.excludeID = excludeId;
			cb.cx = cx; cb.cy = cy; cb.radius = radius;
			cb.bodyMap = &scene->GetBodyEntityMap();

			b2AABB aabb;
			aabb.lowerBound = { cx - radius, cy - radius };
			aabb.upperBound = { cx + radius, cy + radius };
			scene->GetPhysicsWorld()->QueryAABB(&cb, aabb);

			for (uint32_t id : cb.hits)
			{
				if (count >= maxCount)
					break;
				outIds[count++] = id;
			}
			return count;
		}

		int WF_OverlapBox(float cx, float cy, float halfW, float halfH, uint32_t excludeId, uint32_t* outIds, int maxCount)
		{
			int count = 0;
			Scene* scene = ActiveScene();
			if (!scene || !scene->GetPhysicsWorld() || !outIds)
				return 0;

			struct BoxOverlapCB : public b2QueryCallback
			{
				std::vector<uint32_t> hits;
				uint32_t excludeID;
				std::unordered_map<b2Body*, uint32_t>* bodyMap;

				bool ReportFixture(b2Fixture* fixture) override
				{
					b2Body* body = fixture->GetBody();
					auto it = bodyMap->find(body);
					if (it == bodyMap->end() || it->second == excludeID)
						return true;
					if (std::find(hits.begin(), hits.end(), it->second) == hits.end())
						hits.push_back(it->second);
					return true;
				}
			} cb;

			cb.excludeID = excludeId;
			cb.bodyMap = &scene->GetBodyEntityMap();

			b2AABB aabb;
			aabb.lowerBound = { cx - halfW, cy - halfH };
			aabb.upperBound = { cx + halfW, cy + halfH };
			scene->GetPhysicsWorld()->QueryAABB(&cb, aabb);

			for (uint32_t id : cb.hits)
			{
				if (count >= maxCount)
					break;
				outIds[count++] = id;
			}
			return count;
		}

		// Visual

		void WF_SetColor(uint32_t entityId, float r, float g, float b, float a)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return;
			if (entity.HasComponent<SpriteRendererComponent>())
				entity.GetComponent<SpriteRendererComponent>().Color = { r, g, b, a };
			else if (entity.HasComponent<CircleRendererComponent>())
				entity.GetComponent<CircleRendererComponent>().Color = { r, g, b, a };
		}

		void WF_GetColor(uint32_t entityId, float* outR, float* outG, float* outB, float* outA)
		{
			glm::vec4 c = { 1, 1, 1, 1 };
			Entity entity = GetEntity(entityId);
			if (entity)
			{
				if (entity.HasComponent<SpriteRendererComponent>())
					c = entity.GetComponent<SpriteRendererComponent>().Color;
				else if (entity.HasComponent<CircleRendererComponent>())
					c = entity.GetComponent<CircleRendererComponent>().Color;
			}
			*outR = c.r; *outG = c.g; *outB = c.b; *outA = c.a;
		}

		void WF_SetAlpha(uint32_t entityId, float a)
		{
			Entity entity = GetEntity(entityId);
			if (!entity)
				return;
			if (entity.HasComponent<SpriteRendererComponent>())
				entity.GetComponent<SpriteRendererComponent>().Color.a = a;
			else if (entity.HasComponent<CircleRendererComponent>())
				entity.GetComponent<CircleRendererComponent>().Color.a = a;
		}

		void WF_SetTexture(uint32_t entityId, const char* pathUtf8)
		{
			if (!pathUtf8)
				return;
			if (PathEscapesAssetRoot(pathUtf8))
			{
				WF_CORE_WARN("SetTexture: rejected path with '..' segments: '{0}'", pathUtf8);
				return;
			}

			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<SpriteRendererComponent>())
				return;

			std::filesystem::path fullPath = CSharpScriptEngine::GetAssetPath() / pathUtf8;
			if (!std::filesystem::exists(fullPath))
				fullPath = pathUtf8;

			if (std::filesystem::exists(fullPath))
			{
				auto& src = entity.GetComponent<SpriteRendererComponent>();
				src.Texture = Texture2D::Create(fullPath.string(), src.FilterMode);
			}
		}

		// Game UI

		void WF_SetUIText(uint32_t entityId, const char* textUtf8)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<UITextComponent>())
				entity.GetComponent<UITextComponent>().Text = textUtf8 ? textUtf8 : "";
		}

		int WF_GetUIText(uint32_t entityId, char* buffer, int bufferSize)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<UITextComponent>())
				return CopyToBuffer(entity.GetComponent<UITextComponent>().Text, buffer, bufferSize);
			return -1;
		}

		void WF_SetUIProgress(uint32_t entityId, float value)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<UIProgressBarComponent>())
				entity.GetComponent<UIProgressBarComponent>().Value = glm::clamp(value, 0.0f, 1.0f);
		}

		void WF_SetUIImage(uint32_t entityId, const char* pathUtf8)
		{
			if (!pathUtf8)
				return;
			if (PathEscapesAssetRoot(pathUtf8))
			{
				WF_CORE_WARN("SetUIImage: rejected path with '..' segments: '{0}'", pathUtf8);
				return;
			}

			Entity entity = GetEntity(entityId);
			if (!entity || !entity.HasComponent<UIImageComponent>())
				return;

			std::filesystem::path fullPath = CSharpScriptEngine::GetAssetPath() / pathUtf8;
			if (!std::filesystem::exists(fullPath))
				fullPath = pathUtf8;

			if (std::filesystem::exists(fullPath))
			{
				auto& image = entity.GetComponent<UIImageComponent>();
				image.Texture = Texture2D::Create(fullPath.string(), image.FilterMode);
				image.TexturePath = fullPath.string();
			}
		}

		// Animation

		void WF_PlayAnimation(uint32_t entityId, const char* clipUtf8)
		{
			if (!clipUtf8)
				return;
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<AnimatorComponent>())
				entity.GetComponent<AnimatorComponent>().Play(clipUtf8);
		}

		void WF_StopAnimation(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<AnimatorComponent>())
				entity.GetComponent<AnimatorComponent>().Stop();
		}

		void WF_PauseAnimation(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<AnimatorComponent>())
				entity.GetComponent<AnimatorComponent>().Pause();
		}

		void WF_SetAnimationFrame(uint32_t entityId, int frameIndex)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<AnimatorComponent>())
				entity.GetComponent<AnimatorComponent>().CurrentFrameIndex = frameIndex;
		}

		int WF_IsAnimationPlaying(uint32_t entityId)
		{
			Entity entity = GetEntity(entityId);
			if (entity && entity.HasComponent<AnimatorComponent>())
				return entity.GetComponent<AnimatorComponent>().IsPlaying ? 1 : 0;
			return 0;
		}

		// Audio

		int WF_PlaySound(const char* pathUtf8, float volume, float pitch, int loop)
		{
			if (!pathUtf8)
				return 0;
			return AudioEngine::PlaySound(pathUtf8, volume, pitch, loop != 0) ? 1 : 0;
		}

		void WF_StopSound(const char* pathUtf8)
		{
			if (pathUtf8)
				AudioEngine::StopSound(pathUtf8);
			else
				AudioEngine::StopAllSounds();
		}

		void WF_SetSoundVolume(const char* pathUtf8, float volume)
		{
			if (pathUtf8)
				AudioEngine::SetSoundVolume(pathUtf8, volume);
		}

		void WF_SetMasterVolume(float volume)
		{
			AudioEngine::SetMasterVolume(volume);
		}

		// Editor debug gizmos

		void WF_GizmoDrawRay(float x, float y, float dx, float dy, float distance, float r, float g, float b, float a)
		{
			CSharpScriptEngine::s_DebugLines.push_back(
				{ { x, y }, { x + dx * distance, y + dy * distance }, { r, g, b, a } });
		}

		void WF_GizmoDrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a)
		{
			CSharpScriptEngine::s_DebugLines.push_back({ { x1, y1 }, { x2, y2 }, { r, g, b, a } });
		}

		void WF_GizmoDrawWireCircle(float x, float y, float radius, float r, float g, float b, float a)
		{
			CSharpScriptEngine::s_DebugCircles.push_back({ { x, y }, radius, { r, g, b, a } });
		}

	} // anonymous namespace

	// Fills the host function table handed to the managed runtime at init.
	void PopulateHostFunctions(CSharpScriptHost::HostFunctions& fns)
	{
		// logging
		fns.LogInfo = &WF_LogInfo;
		fns.LogWarn = &WF_LogWarn;
		fns.LogError = &WF_LogError;

		// scene / framework
		fns.GetViewportSize = &WF_GetViewportSize;
		fns.ScreenToWorld = &WF_ScreenToWorld;
		fns.EditorGizmoPass = &WF_EditorGizmoPass;

		// input
		fns.IsKeyPressed = &WF_IsKeyPressed;
		fns.IsMouseButtonPressed = &WF_IsMouseButtonPressed;
		fns.IsKeyJustPressed = &WF_IsKeyJustPressed;
		fns.IsKeyJustReleased = &WF_IsKeyJustReleased;
		fns.IsMouseJustPressed = &WF_IsMouseJustPressed;
		fns.IsMouseJustReleased = &WF_IsMouseJustReleased;
		fns.GetMousePosition = &WF_GetMousePosition;
		fns.GetAxis = &WF_GetAxis;

		// scene management
		fns.ChangeScene = &WF_ChangeScene;
		fns.GetCurrentSceneIndex = &WF_GetCurrentSceneIndex;
		fns.SetCurrentSceneIndex = &WF_SetCurrentSceneIndex;
		fns.RequestQuit = &WF_RequestQuit;

		// entity management
		fns.CreateEntity = &WF_CreateEntity;
		fns.DestroyEntity = &WF_DestroyEntity;
		fns.DestroyEntityDelayed = &WF_DestroyEntityDelayed;
		fns.CloneEntity = &WF_CloneEntity;
		fns.InstantiatePrefab = &WF_InstantiatePrefab;
		fns.GetEntityName = &WF_GetEntityName;
		fns.FindEntityByName = &WF_FindEntityByName;
		fns.FindAllEntitiesByName = &WF_FindAllEntitiesByName;
		fns.GetAllEntities = &WF_GetAllEntities;
		fns.GetParent = &WF_GetParent;
		fns.GetChildren = &WF_GetChildren;
		fns.SetParent = &WF_SetParent;
		fns.Unparent = &WF_Unparent;
		fns.SetActive = &WF_SetActive;
		fns.IsActive = &WF_IsActive;

		// transform
		fns.Translate = &WF_Translate;
		fns.SetPosition = &WF_SetPosition;
		fns.GetPosition = &WF_GetPosition;
		fns.SetRotation = &WF_SetRotation;
		fns.SetRotation2D = &WF_SetRotation2D;
		fns.GetRotation = &WF_GetRotation;
		fns.SetScale = &WF_SetScale;
		fns.GetScale = &WF_GetScale;

		// physics 2D
		fns.SetLinearVelocity = &WF_SetLinearVelocity;
		fns.GetLinearVelocity = &WF_GetLinearVelocity;
		fns.ApplyLinearImpulse = &WF_ApplyLinearImpulse;
		fns.ApplyForce = &WF_ApplyForce;
		fns.GetAngularVelocity = &WF_GetAngularVelocity;
		fns.SetAngularVelocity = &WF_SetAngularVelocity;
		fns.ApplyTorque = &WF_ApplyTorque;
		fns.ApplyAngularImpulse = &WF_ApplyAngularImpulse;
		fns.SetSensor = &WF_SetSensor;
		fns.IsSensor = &WF_IsSensor;
		fns.SetGravityScale = &WF_SetGravityScale;
		fns.GetGravityScale = &WF_GetGravityScale;
		fns.SetFixedRotation = &WF_SetFixedRotation;
		fns.IsFixedRotation = &WF_IsFixedRotation;
		fns.SetFriction = &WF_SetFriction;
		fns.SetRestitution = &WF_SetRestitution;
		fns.SetRigidBodyType = &WF_SetRigidBodyType;
		fns.GetRigidBodyType = &WF_GetRigidBodyType;
		fns.Raycast = &WF_Raycast;
		fns.OverlapCircle = &WF_OverlapCircle;
		fns.OverlapBox = &WF_OverlapBox;

		// visual
		fns.SetColor = &WF_SetColor;
		fns.GetColor = &WF_GetColor;
		fns.SetAlpha = &WF_SetAlpha;
		fns.SetTexture = &WF_SetTexture;

		// game UI
		fns.SetUIText = &WF_SetUIText;
		fns.GetUIText = &WF_GetUIText;
		fns.SetUIProgress = &WF_SetUIProgress;
		fns.SetUIImage = &WF_SetUIImage;

		// animation
		fns.PlayAnimation = &WF_PlayAnimation;
		fns.StopAnimation = &WF_StopAnimation;
		fns.PauseAnimation = &WF_PauseAnimation;
		fns.SetAnimationFrame = &WF_SetAnimationFrame;
		fns.IsAnimationPlaying = &WF_IsAnimationPlaying;

		// audio
		fns.PlaySound = &WF_PlaySound;
		fns.StopSound = &WF_StopSound;
		fns.SetSoundVolume = &WF_SetSoundVolume;
		fns.SetMasterVolume = &WF_SetMasterVolume;

		// editor debug gizmos
		fns.GizmoDrawRay = &WF_GizmoDrawRay;
		fns.GizmoDrawLine = &WF_GizmoDrawLine;
		fns.GizmoDrawWireCircle = &WF_GizmoDrawWireCircle;
	}

}
