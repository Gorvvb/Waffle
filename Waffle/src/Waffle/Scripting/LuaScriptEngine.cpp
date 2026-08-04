#include "wfpch.h"
#include "LuaScriptEngine.h"

#include "Waffle/Core/Log.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Core/Input.h"
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"
#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Audio/AudioEngine.h"

#include <box2d/b2_body.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world.h>
#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>
#include "LuaIncludes.h"
#include <fstream>
#include <cctype>
#include <unordered_map>

namespace Waffle {

	lua_State* LuaScriptEngine::s_LuaState = nullptr;
	Scene* LuaScriptEngine::s_SceneContext = nullptr;
	int LuaScriptEngine::s_PendingSceneChange = -1;
	int LuaScriptEngine::s_CurrentSceneIndex = 0;

	// -------------------------------------------------------------------------
	// Path resolution
	// -------------------------------------------------------------------------

	std::filesystem::path LuaScriptEngine::s_AssetPath = "Assets";

	// New state variables
	std::unordered_map<int, bool>          LuaScriptEngine::s_PrevKeyStates;
	std::unordered_map<int, bool>          LuaScriptEngine::s_CurrKeyStates;
	std::unordered_map<int, bool>          LuaScriptEngine::s_PrevMouseStates;
	std::unordered_map<int, bool>          LuaScriptEngine::s_CurrMouseStates;
	std::vector<LuaTimerEntry>             LuaScriptEngine::s_Timers;
	uint32_t                               LuaScriptEngine::s_NextTimerID = 0;
	std::vector<uint32_t>                  LuaScriptEngine::s_PendingDestroys;
	std::vector<LuaDelayedDestroy>         LuaScriptEngine::s_DelayedDestroys;
	float                                  LuaScriptEngine::s_CurrentDeltaTime = 0.0f;

	static std::filesystem::path ResolveScriptPath(const std::string& scriptPath)
	{
		std::filesystem::path normalized = std::filesystem::path(scriptPath).make_preferred();
		std::filesystem::path assetRoot = LuaScriptEngine::GetAssetPath();

		// 1. Try as-is
		if (VFS::Exists(normalized))
			return normalized;

		// 2. Try relative to asset root
		std::filesystem::path fromAssets = assetRoot / normalized;
		if (VFS::Exists(fromAssets))
			return fromAssets;

		// 3. Append .lua if missing
		if (!fromAssets.has_extension())
		{
			fromAssets += ".lua";
			if (VFS::Exists(fromAssets))
				return fromAssets;
		}

		// 4. Recursive search inside asset root
		if (std::filesystem::exists(assetRoot))
		{
			std::string searchFilename = normalized.filename().string();
			if (searchFilename.find(".lua") == std::string::npos)
				searchFilename += ".lua";

			for (auto& entry : std::filesystem::recursive_directory_iterator(assetRoot))
			{
				if (entry.is_regular_file() && entry.path().filename().string() == searchFilename)
					return entry.path();
			}
		}

		return normalized;
	}

	// -------------------------------------------------------------------------
	// Input state tracking helpers (for IsKeyJustPressed / IsKeyJustReleased)
	// -------------------------------------------------------------------------

	void LuaScriptEngine::TrackKey(int code)
	{
		if (s_CurrKeyStates.find(code) == s_CurrKeyStates.end())
		{
			s_CurrKeyStates[code] = Input::IsKeyPressed((KeyCode)code);
			s_PrevKeyStates[code] = false;
		}
	}

	void LuaScriptEngine::TrackMouse(int code)
	{
		if (s_CurrMouseStates.find(code) == s_CurrMouseStates.end())
		{
			s_CurrMouseStates[code] = Input::IsMouseButtonPressed((MouseCode)code);
			s_PrevMouseStates[code] = false;
		}
	}

	void LuaScriptEngine::UpdateInputStates()
	{
		for (auto& [key, curr] : s_CurrKeyStates)
		{
			s_PrevKeyStates[key] = curr;
			curr = Input::IsKeyPressed((KeyCode)key);
		}
		for (auto& [btn, curr] : s_CurrMouseStates)
		{
			s_PrevMouseStates[btn] = curr;
			curr = Input::IsMouseButtonPressed((MouseCode)btn);
		}
	}

	// -------------------------------------------------------------------------
	// Lua C bindings - Input
	// -------------------------------------------------------------------------

	static int Lua_IsKeyPressed(lua_State* L)
	{
		KeyCode code = 0;

		if (lua_isnumber(L, 1))
		{
			code = (KeyCode)lua_tonumber(L, 1);
		}
		else if (lua_isstring(L, 1))
		{
			const char* str = lua_tostring(L, 1);
			if (str)
			{
				if (_stricmp(str, "A") == 0) code = Key::A;
				else if (_stricmp(str, "B") == 0) code = Key::B;
				else if (_stricmp(str, "C") == 0) code = Key::C;
				else if (_stricmp(str, "D") == 0) code = Key::D;
				else if (_stricmp(str, "E") == 0) code = Key::E;
				else if (_stricmp(str, "F") == 0) code = Key::F;
				else if (_stricmp(str, "G") == 0) code = Key::G;
				else if (_stricmp(str, "H") == 0) code = Key::H;
				else if (_stricmp(str, "I") == 0) code = Key::I;
				else if (_stricmp(str, "J") == 0) code = Key::J;
				else if (_stricmp(str, "K") == 0) code = Key::K;
				else if (_stricmp(str, "L") == 0) code = Key::L;
				else if (_stricmp(str, "M") == 0) code = Key::M;
				else if (_stricmp(str, "N") == 0) code = Key::N;
				else if (_stricmp(str, "O") == 0) code = Key::O;
				else if (_stricmp(str, "P") == 0) code = Key::P;
				else if (_stricmp(str, "Q") == 0) code = Key::Q;
				else if (_stricmp(str, "R") == 0) code = Key::R;
				else if (_stricmp(str, "S") == 0) code = Key::S;
				else if (_stricmp(str, "T") == 0) code = Key::T;
				else if (_stricmp(str, "U") == 0) code = Key::U;
				else if (_stricmp(str, "V") == 0) code = Key::V;
				else if (_stricmp(str, "W") == 0) code = Key::W;
				else if (_stricmp(str, "X") == 0) code = Key::X;
				else if (_stricmp(str, "Y") == 0) code = Key::Y;
				else if (_stricmp(str, "Z") == 0) code = Key::Z;
				else if (_stricmp(str, "Space") == 0 || strcmp(str, " ") == 0) code = Key::Space;
				else if (_stricmp(str, "Left") == 0) code = Key::Left;
				else if (_stricmp(str, "Right") == 0) code = Key::Right;
				else if (_stricmp(str, "Up") == 0) code = Key::Up;
				else if (_stricmp(str, "Down") == 0) code = Key::Down;
				else if (_stricmp(str, "Escape") == 0) code = Key::Escape;
				else if (_stricmp(str, "Enter") == 0) code = Key::Enter;
				else if (_stricmp(str, "Tab") == 0) code = Key::Tab;
				else if (_stricmp(str, "LeftShift") == 0) code = Key::LeftShift;
				else if (_stricmp(str, "RightShift") == 0) code = Key::RightShift;
				else if (_stricmp(str, "LeftControl") == 0) code = Key::LeftControl;
				else if (_stricmp(str, "RightControl") == 0) code = Key::RightControl;
				else if (_stricmp(str, "LeftAlt") == 0) code = Key::LeftAlt;
				else if (_stricmp(str, "RightAlt") == 0) code = Key::RightAlt;
				else if (strlen(str) == 1)
				{
					char c = (char)toupper((unsigned char)str[0]);
					if (c >= 'A' && c <= 'Z')
						code = (KeyCode)c;
				}
			}
		}

		bool isPressed = (code != 0) && Input::IsKeyPressed(code);
		lua_pushboolean(L, isPressed ? 1 : 0);
		return 1;
	}

	static int Lua_IsMouseButtonPressed(lua_State* L)
	{
		MouseCode code = Mouse::ButtonLeft;

		if (lua_isnumber(L, 1))
		{
			code = (MouseCode)lua_tonumber(L, 1);
		}
		else if (lua_isstring(L, 1))
		{
			const char* str = lua_tostring(L, 1);
			if (str)
			{
				if (_stricmp(str, "Left") == 0 || _stricmp(str, "ButtonLeft") == 0) code = Mouse::ButtonLeft;
				else if (_stricmp(str, "Right") == 0 || _stricmp(str, "ButtonRight") == 0) code = Mouse::ButtonRight;
				else if (_stricmp(str, "Middle") == 0 || _stricmp(str, "ButtonMiddle") == 0) code = Mouse::ButtonMiddle;
			}
		}

		bool isPressed = Input::IsMouseButtonPressed(code);
		lua_pushboolean(L, isPressed ? 1 : 0);
		return 1;
	}

	static int Lua_GetMousePosition(lua_State* L)
	{
		glm::vec2 pos = Input::GetMousePosition();
		lua_pushnumber(L, pos.x);
		lua_pushnumber(L, pos.y);
		return 2;
	}

	// -------------------------------------------------------------------------
	// Lua C bindings - Transform / Physics
	// -------------------------------------------------------------------------

	static int Lua_Translate(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float dx = (float)lua_tonumber(L, 2);
		float dy = (float)lua_tonumber(L, 3);
		float dz = lua_isnumber(L, 4) ? (float)lua_tonumber(L, 4) : 0.0f;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<TransformComponent>())
			return 0;

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
					// Kinematic: drive position directly, Box2D interpolates
					body->SetTransform(
						b2Vec2(tc.Translation.x, tc.Translation.y),
						tc.Rotation.z);
					body->SetAwake(true);
					break;

				case Rigidbody2DComponent::BodyType::Dynamic:
					// Dynamic: never stomp position - the solver owns it.
					// Translate on a dynamic body is a misuse; log and skip.
					WF_CORE_WARN("Lua_Translate: called on Dynamic body (entity {0}). "
						"Use SetLinearVelocity or ApplyForce instead.", entityID);
					// Revert the transform change since Box2D won't follow it
					tc.Translation.x -= dx;
					tc.Translation.y -= dy;
					tc.Translation.z -= dz;
					break;

				case Rigidbody2DComponent::BodyType::Static:
					// Static bodies don't move; silently ignore
					tc.Translation.x -= dx;
					tc.Translation.y -= dy;
					tc.Translation.z -= dz;
					break;
				}
			}
		}
		return 0;
	}

	static int Lua_SetPosition(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float x = (float)lua_tonumber(L, 2);
		float y = (float)lua_tonumber(L, 3);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene)
			return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation.x = x;
			tc.Translation.y = y;
			if (lua_isnumber(L, 4))
				tc.Translation.z = (float)lua_tonumber(L, 4);

			if (entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
				{
					body->SetTransform(b2Vec2(tc.Translation.x, tc.Translation.y), tc.Rotation.z);
					body->SetAwake(true);
				}
			}
		}
		return 0;
	}

	static int Lua_GetPosition(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<TransformComponent>())
			{
				const auto& tc = entity.GetComponent<TransformComponent>();
				lua_pushnumber(L, tc.Translation.x);
				lua_pushnumber(L, tc.Translation.y);
				lua_pushnumber(L, tc.Translation.z);
				return 3;
			}
		}

		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
		return 3;
	}

	static int Lua_SetLinearVelocity(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float vx = (float)lua_tonumber(L, 2);
		float vy = (float)lua_tonumber(L, 3);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene)
			return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			b2Body* body = (b2Body*)rb2d.RuntimeBody;
			if (body)
			{
				body->SetAwake(true);
				body->SetLinearVelocity(b2Vec2(vx, vy));
			}
		}
		return 0;
	}

	static int Lua_GetLinearVelocity(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
				{
					b2Vec2 vel = body->GetLinearVelocity();
					lua_pushnumber(L, vel.x);
					lua_pushnumber(L, vel.y);
					return 2;
				}
			}
		}

		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
		return 2;
	}

	static int Lua_ApplyLinearImpulse(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float impulseX = (float)lua_tonumber(L, 2);
		float impulseY = (float)lua_tonumber(L, 3);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene)
			return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			b2Body* body = (b2Body*)rb2d.RuntimeBody;
			if (body)
			{
				body->SetAwake(true);
				body->ApplyLinearImpulseToCenter(b2Vec2(impulseX, impulseY), true);
			}
		}
		return 0;
	}

	static int Lua_ApplyForce(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float forceX = (float)lua_tonumber(L, 2);
		float forceY = (float)lua_tonumber(L, 3);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene)
			return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			b2Body* body = (b2Body*)rb2d.RuntimeBody;
			if (body)
			{
				body->SetAwake(true);
				body->ApplyForceToCenter(b2Vec2(forceX, forceY), true);
			}
		}
		return 0;
	}

	static int Lua_GetRotation(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene) {
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<TransformComponent>()) {
				const auto& tc = entity.GetComponent<TransformComponent>();
				lua_pushnumber(L, tc.Rotation.x);
				lua_pushnumber(L, tc.Rotation.y);
				lua_pushnumber(L, tc.Rotation.z); // Z is what matters for 2D
				return 3;
			}
		}
		lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
		return 3;
	}

	static int Lua_SetRotation(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			if (lua_isnumber(L, 3)) {
				// Full 3-axis form: SetRotation(id, x, y, z)
				tc.Rotation.x = (float)lua_tonumber(L, 2);
				tc.Rotation.y = (float)lua_tonumber(L, 3);
				tc.Rotation.z = (float)lua_tonumber(L, 4);
			}
			else {
				// 2D shorthand: SetRotation(id, z)
				tc.Rotation.z = (float)lua_tonumber(L, 2);
			}

			// Sync physics body rotation
			if (entity.HasComponent<Rigidbody2DComponent>())
			{
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				if (body)
					body->SetTransform(body->GetPosition(), tc.Rotation.z);
			}
		}
		return 0;
	}

	static int Lua_GetScale(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene) {
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<TransformComponent>()) {
				const auto& tc = entity.GetComponent<TransformComponent>();
				lua_pushnumber(L, tc.Scale.x);
				lua_pushnumber(L, tc.Scale.y);
				lua_pushnumber(L, tc.Scale.z);
				return 3;
			}
		}
		lua_pushnumber(L, 1); lua_pushnumber(L, 1); lua_pushnumber(L, 1);
		return 3;
	}

	static int Lua_SetScale(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;

		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Scale.x = (float)lua_tonumber(L, 2);
			tc.Scale.y = (float)lua_tonumber(L, 3);
			if (lua_isnumber(L, 4))
				tc.Scale.z = (float)lua_tonumber(L, 4);
		}
		return 0;
	}

	static int Lua_FindEntityByName(lua_State* L)
	{
		const char* name = lua_tostring(L, 1);
		if (!name) { lua_pushnumber(L, -1); return 1; }

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, -1); return 1; }

		auto view = scene->GetRegistry().view<TagComponent>();
		for (auto entityID : view)
		{
			const auto& tag = view.get<TagComponent>(entityID);
			if (tag.Tag == name)
			{
				lua_pushnumber(L, (uint32_t)entityID);
				return 1;
			}
		}

		lua_pushnumber(L, -1); // not found sentinel
		return 1;
	}

	static int Lua_ChangeScene(lua_State* L)
	{
		LuaScriptEngine::SetPendingSceneChange((int)lua_tonumber(L, 1));
		return 0;
	}

	static int Lua_GetCurrentSceneIndex(lua_State* L)
	{
		lua_pushinteger(L, LuaScriptEngine::GetCurrentSceneIndex());
		return 1;
	}

	static int Lua_SetCurrentSceneIndex(lua_State* L)
	{
		int index = (int)lua_tointeger(L, 1);
		LuaScriptEngine::SetCurrentSceneIndex(index);
		return 0;
	}

	// -------------------------------------------------------------------------
	// Lua C bindings - Raycasting
	// -------------------------------------------------------------------------

	// Raycast(entityID, offsetX, offsetY, dirX, dirY, distance)
	//   -> hit, hitEntityID, hitX, hitY, normalX, normalY
	//
	// Returns up to 6 values. If miss, returns false, -1, 0, 0, 0, 0.
	// hitEntityID is -1 when the hit body isn't in the entity map.
	static int Lua_Raycast(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float offsetX = (float)lua_tonumber(L, 2);
		float offsetY = (float)lua_tonumber(L, 3);
		float dirX = (float)lua_tonumber(L, 4);
		float dirY = (float)lua_tonumber(L, 5);
		float dist = (float)lua_tonumber(L, 6);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene)
		{
			lua_pushboolean(L, 0); lua_pushnumber(L, -1);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			return 6;
		}

		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<TransformComponent>())
		{
			lua_pushboolean(L, 0); lua_pushnumber(L, -1);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			return 6;
		}

		if (!scene->GetPhysicsWorld())
		{
			lua_pushboolean(L, 0); lua_pushnumber(L, -1);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			return 6;
		}

		const auto& tc = entity.GetComponent<TransformComponent>();
		b2Vec2 origin(tc.Translation.x + offsetX, tc.Translation.y + offsetY);
		b2Vec2 end(origin.x + dirX * dist, origin.y + dirY * dist);

		if (origin.x == end.x && origin.y == end.y)
		{
			lua_pushboolean(L, 0); lua_pushnumber(L, -1);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			lua_pushnumber(L, 0); lua_pushnumber(L, 0);
			return 6;
		}

		struct RayCallback : public b2RayCastCallback
		{
			uint32_t ignoreID;
			bool     hit        = false;
			int32_t  hitEntity  = -1;
			b2Vec2   hitPoint   = { 0, 0 };
			b2Vec2   hitNormal  = { 0, 0 };
			float    minFraction = 1.0f;
			std::unordered_map<b2Body*, uint32_t>* bodyMap;

			float ReportFixture(b2Fixture* fixture,
				const b2Vec2& point,
				const b2Vec2& normal,
				float fraction) override
			{
				b2Body* body = fixture->GetBody();
				auto it = bodyMap->find(body);
				if (it != bodyMap->end() && it->second == ignoreID)
					return -1.0f; // skip self

				if (fraction < minFraction)
				{
					minFraction = fraction;
					hit        = true;
					hitPoint   = point;
					hitNormal  = normal;
					hitEntity  = (it != bodyMap->end()) ? (int32_t)it->second : -1;
				}
				return fraction; // continue to find closest
			}
		};

		RayCallback cb;
		cb.ignoreID = entityID;
		cb.bodyMap  = &scene->GetBodyEntityMap();
		scene->GetPhysicsWorld()->RayCast(&cb, origin, end);

		lua_pushboolean(L, cb.hit ? 1 : 0);
		lua_pushnumber(L, cb.hitEntity);
		lua_pushnumber(L, cb.hitPoint.x);
		lua_pushnumber(L, cb.hitPoint.y);
		lua_pushnumber(L, cb.hitNormal.x);
		lua_pushnumber(L, cb.hitNormal.y);
		return 6;
	}

	// -------------------------------------------------------------------------
	// Lua C bindings - Logging
	// -------------------------------------------------------------------------

	static int Lua_LogInfo(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_INFO("Lua: {0}", msg);
		return 0;
	}

	static int Lua_LogWarn(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_WARN("Lua: {0}", msg);
		return 0;
	}

	static int Lua_LogError(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_ERROR("Lua: {0}", msg);
		return 0;
	}

	// -------------------------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------------------------

	static void ApplyFieldToLua(lua_State* L, const std::string& tableKey, const LuaField& field)
	{
		lua_getglobal(L, tableKey.c_str());
		if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
		lua_getfield(L, -1, "Public");
		if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

		switch (field.Type)
		{
		case LuaFieldType::Float:  lua_pushnumber(L, field.FloatVal);           break;
		case LuaFieldType::Int:    lua_pushinteger(L, field.IntVal);             break;
		case LuaFieldType::Bool:   lua_pushboolean(L, field.BoolVal ? 1 : 0);   break;
		case LuaFieldType::String: lua_pushstring(L, field.StringVal.c_str()); break;
		}

		lua_setfield(L, -2, field.Name.c_str());
		lua_pop(L, 2);
	}

	static void ScrapePublicFields(lua_State* L, const std::string& tableKey,
		const std::string& scriptPath, ScriptComponent& sc)
	{
		auto& fields = sc.Fields[scriptPath];

		lua_getglobal(L, tableKey.c_str());
		if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
		lua_getfield(L, -1, "Public");
		if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			if (!lua_isstring(L, -2)) { lua_pop(L, 1); continue; }

			const char* name = lua_tostring(L, -2);

			// Skip fields the editor already has a value for
			bool alreadyExists = std::any_of(fields.begin(), fields.end(),
				[name](const LuaField& f) { return f.Name == name; });

			if (!alreadyExists)
			{
				LuaField field;
				field.Name = name;

				if (lua_isinteger(L, -1))
				{
					field.Type = LuaFieldType::Int;
					field.IntVal = (int)lua_tointeger(L, -1);
				}
				else if (lua_isnumber(L, -1))
				{
					field.Type = LuaFieldType::Float;
					field.FloatVal = (float)lua_tonumber(L, -1);
				}
				else if (lua_isboolean(L, -1))
				{
					field.Type = LuaFieldType::Bool;
					field.BoolVal = (lua_toboolean(L, -1) != 0);
				}
				else if (lua_isstring(L, -1))
				{
					field.Type = LuaFieldType::String;
					field.StringVal = lua_tostring(L, -1);
				}
				else
				{
					lua_pop(L, 1);
					continue;
				}

				fields.push_back(field);
			}

			lua_pop(L, 1);
		}

		lua_pop(L, 2); // pop Public + env

		// Write all stored editor values back into the live Lua env
		for (const auto& field : fields)
			ApplyFieldToLua(L, tableKey, field);
	}

	void LuaScriptEngine::ScrapeFieldsFromScript(const std::filesystem::path& fullPath,
		const std::string& scriptPath,
		ScriptComponent& sc)
	{
		// Don't overwrite fields that were already loaded (e.g. from the scene file)
		auto it = sc.Fields.find(scriptPath);
		if (it != sc.Fields.end() && !it->second.empty())
			return;

		lua_State* L = luaL_newstate();
		luaL_openlibs(L);

		std::ifstream file(fullPath, std::ios::binary);
		if (!file.is_open()) { lua_close(L); return; }
		std::string source((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();

		std::string wrapper = "return function(_ENV)\n" + source + "\nend";
		if (luaL_loadbufferx(L, wrapper.c_str(), wrapper.size(), "@scrape", "t") != LUA_OK)
		{
			lua_close(L); return;
		}
		if (lua_pcall(L, 0, 1, 0) != LUA_OK)
		{
			lua_close(L); return;
		}

		// Build a minimal env table with a metatable pointing to _G
		lua_newtable(L);                                          // env
		lua_newtable(L);                                          // metatable
		lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
		lua_setfield(L, -2, "__index");
		lua_setmetatable(L, -2);

		// Call the wrapper with the env - this executes the script top-level,
		// which defines Public = { ... } inside the env table
		if (lua_pcall(L, 1, 0, 0) != LUA_OK)
		{
			lua_close(L); return;
		}

		// env is now on the registry - but we left it on the stack before pcall
		// Actually re-push: the env was consumed. Rebuild by running again.
		// Simpler: just look for Public as a global since we used _G as __index
		// Instead, rerun with a named env:
		lua_close(L);

		// Cleaner second attempt - store the env before calling
		L = luaL_newstate();
		luaL_openlibs(L);

		file.open(fullPath, std::ios::binary);
		if (!file.is_open()) { lua_close(L); return; }
		source = std::string((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();

		wrapper = "return function(_ENV)\n" + source + "\nend";
		if (luaL_loadbufferx(L, wrapper.c_str(), wrapper.size(), "@scrape", "t") != LUA_OK)
		{
			lua_close(L); return;
		}
		if (lua_pcall(L, 0, 1, 0) != LUA_OK)
		{
			lua_close(L); return;
		}

		int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		lua_newtable(L);
		lua_newtable(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
		lua_setfield(L, -2, "__index");
		lua_setmetatable(L, -2);
		int envRef = luaL_ref(L, LUA_REGISTRYINDEX);

		lua_rawgeti(L, LUA_REGISTRYINDEX, fnRef);
		lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
		if (lua_pcall(L, 1, 0, 0) != LUA_OK)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, fnRef); luaL_unref(L, LUA_REGISTRYINDEX, envRef); lua_close(L); return;
		}

		// Read Public from the env table
		lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
		lua_getfield(L, -1, "Public");

		if (lua_istable(L, -1))
		{
			auto& fields = sc.Fields[scriptPath];
			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				if (lua_isstring(L, -2))
				{
					const char* name = lua_tostring(L, -2);

					// Check if we already have this field
					auto it = std::find_if(fields.begin(), fields.end(),
						[name](const LuaField& f) { return f.Name == name; });

					if (it != fields.end())
					{
						// Field exists - only update value if user hasn't modified it
						if (!it->UserModified)
						{
							if (lua_isinteger(L, -1))
								it->IntVal = (int)lua_tointeger(L, -1);
							else if (lua_isnumber(L, -1))
								it->FloatVal = (float)lua_tonumber(L, -1);
							else if (lua_isboolean(L, -1))
								it->BoolVal = lua_toboolean(L, -1) != 0;
							else if (lua_isstring(L, -1))
								it->StringVal = lua_tostring(L, -1);
						}
						lua_pop(L, 1);
						continue;
					}

					// New field - read default from script
					LuaField field;
					field.Name = name;
					field.UserModified = false;

					if (lua_isinteger(L, -1))
					{
						field.Type = LuaFieldType::Int;
						field.IntVal = (int)lua_tointeger(L, -1);
					}
					else if (lua_isnumber(L, -1))
					{
						field.Type = LuaFieldType::Float;
						field.FloatVal = (float)lua_tonumber(L, -1);
					}
					else if (lua_isboolean(L, -1))
					{
						field.Type = LuaFieldType::Bool;
						field.BoolVal = lua_toboolean(L, -1) != 0;
					}
					else if (lua_isstring(L, -1))
					{
						field.Type = LuaFieldType::String;
						field.StringVal = lua_tostring(L, -1);
					}
					else
					{
						lua_pop(L, 1);
						continue;
					}

					fields.push_back(field);
				}
				lua_pop(L, 1);
			}
		}

		luaL_unref(L, LUA_REGISTRYINDEX, fnRef);
		luaL_unref(L, LUA_REGISTRYINDEX, envRef);
		lua_close(L);
	}

	// =========================================================================
	// NEW BINDINGS - Input (just-pressed / just-released)
	// =========================================================================

	static int Lua_IsKeyJustPressed(lua_State* L)
	{
		int code = 0;
		if (lua_isnumber(L, 1)) code = (int)lua_tonumber(L, 1);
		else if (lua_isstring(L, 1))
		{
			// Reuse existing string → keycode logic by calling IsKeyPressed helper
			// We only need the code; parse via the same chain already in Lua_IsKeyPressed.
			// Simplest: just call Input directly after resolving via a temporary Lua call.
			// Instead, duplicate the minimal lookup here for common keys.
			const char* str = lua_tostring(L, 1);
			if (str && strlen(str) == 1) { char c = (char)toupper((unsigned char)str[0]); if (c >= 'A' && c <= 'Z') code = (int)c; }
		}
		LuaScriptEngine::TrackKey(code);
		bool prev = LuaScriptEngine::s_PrevKeyStates.count(code) ? LuaScriptEngine::s_PrevKeyStates[code] : false;
		bool curr = LuaScriptEngine::s_CurrKeyStates.count(code) ? LuaScriptEngine::s_CurrKeyStates[code] : false;
		lua_pushboolean(L, (curr && !prev) ? 1 : 0);
		return 1;
	}

	static int Lua_IsKeyJustReleased(lua_State* L)
	{
		int code = 0;
		if (lua_isnumber(L, 1)) code = (int)lua_tonumber(L, 1);
		else if (lua_isstring(L, 1))
		{
			const char* str = lua_tostring(L, 1);
			if (str && strlen(str) == 1) { char c = (char)toupper((unsigned char)str[0]); if (c >= 'A' && c <= 'Z') code = (int)c; }
		}
		LuaScriptEngine::TrackKey(code);
		bool prev = LuaScriptEngine::s_PrevKeyStates.count(code) ? LuaScriptEngine::s_PrevKeyStates[code] : false;
		bool curr = LuaScriptEngine::s_CurrKeyStates.count(code) ? LuaScriptEngine::s_CurrKeyStates[code] : false;
		lua_pushboolean(L, (!curr && prev) ? 1 : 0);
		return 1;
	}

	static int Lua_IsMouseJustPressed(lua_State* L)
	{
		int code = 0;
		if (lua_isnumber(L, 1)) code = (int)lua_tonumber(L, 1);
		LuaScriptEngine::TrackMouse(code);
		bool prev = LuaScriptEngine::s_PrevMouseStates.count(code) ? LuaScriptEngine::s_PrevMouseStates[code] : false;
		bool curr = LuaScriptEngine::s_CurrMouseStates.count(code) ? LuaScriptEngine::s_CurrMouseStates[code] : false;
		lua_pushboolean(L, (curr && !prev) ? 1 : 0);
		return 1;
	}

	static int Lua_IsMouseJustReleased(lua_State* L)
	{
		int code = 0;
		if (lua_isnumber(L, 1)) code = (int)lua_tonumber(L, 1);
		LuaScriptEngine::TrackMouse(code);
		bool prev = LuaScriptEngine::s_PrevMouseStates.count(code) ? LuaScriptEngine::s_PrevMouseStates[code] : false;
		bool curr = LuaScriptEngine::s_CurrMouseStates.count(code) ? LuaScriptEngine::s_CurrMouseStates[code] : false;
		lua_pushboolean(L, (!curr && prev) ? 1 : 0);
		return 1;
	}

	static int Lua_GetAxis(lua_State* L)
	{
		const char* axisName = lua_tostring(L, 1);
		if (!axisName) { lua_pushnumber(L, 0); return 1; }
		float val = Input::GetAxis(axisName);
		lua_pushnumber(L, val);
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Color / Visual
	// =========================================================================

	static int Lua_SetColor(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float r = (float)lua_tonumber(L, 2);
		float g = (float)lua_tonumber(L, 3);
		float b = (float)lua_tonumber(L, 4);
		float a = lua_isnumber(L, 5) ? (float)lua_tonumber(L, 5) : 1.0f;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity) return 0;

		if (entity.HasComponent<SpriteRendererComponent>())
			entity.GetComponent<SpriteRendererComponent>().Color = { r, g, b, a };
		else if (entity.HasComponent<CircleRendererComponent>())
			entity.GetComponent<CircleRendererComponent>().Color = { r, g, b, a };
		return 0;
	}

	static int Lua_GetColor(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity)
			{
				glm::vec4 c = { 1, 1, 1, 1 };
				if (entity.HasComponent<SpriteRendererComponent>())
					c = entity.GetComponent<SpriteRendererComponent>().Color;
				else if (entity.HasComponent<CircleRendererComponent>())
					c = entity.GetComponent<CircleRendererComponent>().Color;
				lua_pushnumber(L, c.r); lua_pushnumber(L, c.g);
				lua_pushnumber(L, c.b); lua_pushnumber(L, c.a);
				return 4;
			}
		}
		lua_pushnumber(L, 1); lua_pushnumber(L, 1); lua_pushnumber(L, 1); lua_pushnumber(L, 1);
		return 4;
	}

	static int Lua_SetAlpha(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float a = (float)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity) return 0;
		if (entity.HasComponent<SpriteRendererComponent>())
			entity.GetComponent<SpriteRendererComponent>().Color.a = a;
		else if (entity.HasComponent<CircleRendererComponent>())
			entity.GetComponent<CircleRendererComponent>().Color.a = a;
		return 0;
	}

	static int Lua_SetTexture(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		const char* pathStr = lua_tostring(L, 2);
		if (!pathStr) return 0;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return 0;

		std::filesystem::path fullPath = LuaScriptEngine::GetAssetPath() / pathStr;
		if (!std::filesystem::exists(fullPath))
			fullPath = pathStr;

		if (std::filesystem::exists(fullPath))
		{
			auto& src = entity.GetComponent<SpriteRendererComponent>();
			src.Texture = Texture2D::Create(fullPath.string(), src.FilterMode);
		}
		return 0;
	}

	// =========================================================================
	// NEW BINDINGS - Animator / Animation
	// =========================================================================

	static int Lua_PlayAnimation(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		const char* clipName = lua_tostring(L, 2);
		if (!clipName) return 0;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<AnimatorComponent>()) return 0;

		entity.GetComponent<AnimatorComponent>().Play(clipName);
		return 0;
	}

	static int Lua_StopAnimation(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<AnimatorComponent>()) return 0;

		entity.GetComponent<AnimatorComponent>().Stop();
		return 0;
	}

	static int Lua_PauseAnimation(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<AnimatorComponent>()) return 0;

		entity.GetComponent<AnimatorComponent>().Pause();
		return 0;
	}

	static int Lua_SetAnimationFrame(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		int frameIndex = (int)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<AnimatorComponent>()) return 0;

		entity.GetComponent<AnimatorComponent>().CurrentFrameIndex = frameIndex;
		return 0;
	}

	static int Lua_IsAnimationPlaying(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushboolean(L, 0); return 1; }
		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<AnimatorComponent>()) { lua_pushboolean(L, 0); return 1; }

		bool playing = entity.GetComponent<AnimatorComponent>().IsPlaying;
		lua_pushboolean(L, playing ? 1 : 0);
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Entity management
	// =========================================================================

	static int Lua_CreateEntity(lua_State* L)
	{
		const char* name = lua_isstring(L, 1) ? lua_tostring(L, 1) : "Entity";
		float x = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 0.0f;
		float y = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, -1); return 1; }

		Entity entity = scene->CreateEntity(name ? name : "Entity");
		if (entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation.x = x;
			tc.Translation.y = y;
		}
		lua_pushnumber(L, (uint32_t)(entt::entity)entity);
		return 1;
	}

	static int Lua_DestroyEntity(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		// Deferred - we process after the script update loop to avoid iterator invalidation
		LuaScriptEngine::s_PendingDestroys.push_back(entityID);
		return 0;
	}

	static int Lua_DestroyEntityDelayed(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float delay = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 0.0f;
		LuaScriptEngine::s_DelayedDestroys.push_back({ entityID, delay });
		return 0;
	}

	static int Lua_CloneEntity(lua_State* L)
	{
		uint32_t sourceID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, -1); return 1; }

		Entity source{ (entt::entity)sourceID, scene };
		if (!source) { lua_pushnumber(L, -1); return 1; }

		// DuplicateEntity copies all components and returns the new entity
		Entity newEntity = scene->DuplicateEntity(source);
		lua_pushnumber(L, newEntity ? (uint32_t)(entt::entity)newEntity : (uint32_t)entt::null);
		return 1;
	}

	static int Lua_InstantiatePrefab(lua_State* L)
	{
		const char* pathStr = lua_tostring(L, 1);
		float x = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 0.0f;
		float y = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;

		if (!pathStr) { lua_pushnumber(L, -1); return 1; }

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, -1); return 1; }

		std::filesystem::path fullPath = LuaScriptEngine::GetAssetPath() / pathStr;
		if (!VFS::Exists(fullPath))
			fullPath = pathStr;

		if (!VFS::Exists(fullPath))
		{
			WF_CORE_ERROR("InstantiatePrefab: Prefab file '{0}' not found", pathStr);
			lua_pushnumber(L, -1);
			return 1;
		}

		Entity entity = SceneSerializer::DeserializePrefabToEntity(scene, fullPath.string(), x, y);
		if (!entity)
		{
			lua_pushnumber(L, -1);
			return 1;
		}

		// Create the Box2D body right away so physics works (and the physics
		// write-back loop doesn't hit a null RuntimeBody) on this same frame.
		scene->CreateRuntimePhysicsBody(entity);

		// Initialize and call OnCreate for any scripts on the prefab entity so
		// they start running immediately (ScriptTableKeys would otherwise be
		// empty and OnRuntimeUpdate would silently skip this entity).
		LuaScriptEngine::InitScriptsForEntity(scene, entity);

		lua_pushnumber(L, (uint32_t)(entt::entity)entity);
		return 1;
	}

	static int Lua_GetEntityName(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<TagComponent>())
			{
				lua_pushstring(L, entity.GetComponent<TagComponent>().Tag.c_str());
				return 1;
			}
		}
		lua_pushstring(L, "");
		return 1;
	}

	static int Lua_FindAllEntitiesByName(lua_State* L)
	{
		const char* name = lua_tostring(L, 1);
		lua_newtable(L);
		if (!name) return 1;

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 1;

		int idx = 1;
		auto view = scene->GetRegistry().view<TagComponent>();
		for (auto entityID : view)
		{
			const auto& tag = view.get<TagComponent>(entityID);
			if (tag.Tag == name)
			{
				lua_pushnumber(L, (uint32_t)entityID);
				lua_rawseti(L, -2, idx++);
			}
		}
		return 1;
	}

	static int Lua_GetAllEntities(lua_State* L)
	{
		lua_newtable(L);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 1;

		int idx = 1;
		auto view = scene->GetRegistry().view<TagComponent>();
		for (auto entityID : view)
		{
			lua_pushnumber(L, (uint32_t)entityID);
			lua_rawseti(L, -2, idx++);
		}
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Timer system
	// =========================================================================

	static int Lua_SetTimer(lua_State* L)
	{
		float delay = (float)lua_tonumber(L, 1);
		if (!lua_isfunction(L, 2)) { lua_pushnumber(L, -1); return 1; }

		int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops the function
		uint32_t id = LuaScriptEngine::s_NextTimerID++;

		LuaTimerEntry entry;
		entry.Remaining    = delay;
		entry.Delay        = delay;
		entry.CallbackRef  = callbackRef;
		entry.Active       = true;
		entry.ID           = id;
		LuaScriptEngine::s_Timers.push_back(entry);

		lua_pushnumber(L, id);
		return 1;
	}

	static int Lua_CancelTimer(lua_State* L)
	{
		uint32_t id = (uint32_t)lua_tonumber(L, 1);
		for (auto& t : LuaScriptEngine::s_Timers)
		{
			if (t.ID == id && t.Active)
			{
				t.Active = false;
				luaL_unref(LuaScriptEngine::s_LuaState, LUA_REGISTRYINDEX, t.CallbackRef);
				t.CallbackRef = LUA_NOREF;
				break;
			}
		}
		return 0;
	}

	// =========================================================================
	// NEW BINDINGS - Angular physics
	// =========================================================================

	static int Lua_GetAngularVelocity(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<Rigidbody2DComponent>())
			{
				b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
				if (body) { lua_pushnumber(L, body->GetAngularVelocity()); return 1; }
			}
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	static int Lua_SetAngularVelocity(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float omega = (float)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
			if (body) { body->SetAwake(true); body->SetAngularVelocity(omega); }
		}
		return 0;
	}

	static int Lua_ApplyTorque(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float torque = (float)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
			if (body) { body->SetAwake(true); body->ApplyTorque(torque, true); }
		}
		return 0;
	}

	static int Lua_ApplyAngularImpulse(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		float impulse = (float)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity entity{ (entt::entity)entityID, scene };
		if (entity && entity.HasComponent<Rigidbody2DComponent>())
		{
			b2Body* body = (b2Body*)entity.GetComponent<Rigidbody2DComponent>().RuntimeBody;
			if (body) { body->SetAwake(true); body->ApplyAngularImpulse(impulse, true); }
		}
		return 0;
	}

	// =========================================================================
	// NEW BINDINGS - Shape overlap queries
	// =========================================================================

	static int Lua_OverlapCircle(lua_State* L)
	{
		float cx = (float)lua_tonumber(L, 1);
		float cy = (float)lua_tonumber(L, 2);
		float radius = (float)lua_tonumber(L, 3);
		uint32_t excludeID = lua_isnumber(L, 4) ? (uint32_t)lua_tonumber(L, 4) : (uint32_t)entt::null;

		lua_newtable(L);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene || !scene->GetPhysicsWorld()) return 1;

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
				if (it == bodyMap->end() || it->second == excludeID) return true;

				// Confirm circle overlap using Box2D TestPoint or distance check
				b2Vec2 pos = body->GetPosition();
				float dx = pos.x - cx, dy = pos.y - cy;
				if (dx * dx + dy * dy <= radius * radius)
				{
					uint32_t id = it->second;
					if (std::find(hits.begin(), hits.end(), id) == hits.end())
						hits.push_back(id);
				}
				return true;
			}
		} cb;

		cb.excludeID = excludeID;
		cb.cx = cx; cb.cy = cy; cb.radius = radius;
		cb.bodyMap = &scene->GetBodyEntityMap();

		b2AABB aabb;
		aabb.lowerBound = { cx - radius, cy - radius };
		aabb.upperBound = { cx + radius, cy + radius };
		scene->GetPhysicsWorld()->QueryAABB(&cb, aabb);

		for (int i = 0; i < (int)cb.hits.size(); i++)
		{
			lua_pushnumber(L, cb.hits[i]);
			lua_rawseti(L, -2, i + 1);
		}
		return 1;
	}

	static int Lua_OverlapBox(lua_State* L)
	{
		float cx    = (float)lua_tonumber(L, 1);
		float cy    = (float)lua_tonumber(L, 2);
		float halfW = (float)lua_tonumber(L, 3);
		float halfH = (float)lua_tonumber(L, 4);
		uint32_t excludeID = lua_isnumber(L, 5) ? (uint32_t)lua_tonumber(L, 5) : (uint32_t)entt::null;

		lua_newtable(L);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene || !scene->GetPhysicsWorld()) return 1;

		struct BoxOverlapCB : public b2QueryCallback
		{
			std::vector<uint32_t> hits;
			uint32_t excludeID;
			std::unordered_map<b2Body*, uint32_t>* bodyMap;

			bool ReportFixture(b2Fixture* fixture) override
			{
				b2Body* body = fixture->GetBody();
				auto it = bodyMap->find(body);
				if (it == bodyMap->end() || it->second == excludeID) return true;
				uint32_t id = it->second;
				if (std::find(hits.begin(), hits.end(), id) == hits.end())
					hits.push_back(id);
				return true;
			}
		} cb;

		cb.excludeID = excludeID;
		cb.bodyMap   = &scene->GetBodyEntityMap();

		b2AABB aabb;
		aabb.lowerBound = { cx - halfW, cy - halfH };
		aabb.upperBound = { cx + halfW, cy + halfH };
		scene->GetPhysicsWorld()->QueryAABB(&cb, aabb);

		for (int i = 0; i < (int)cb.hits.size(); i++)
		{
			lua_pushnumber(L, cb.hits[i]);
			lua_rawseti(L, -2, i + 1);
		}
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Entity hierarchy
	// =========================================================================

	static int Lua_GetParent(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (scene)
		{
			Entity entity{ (entt::entity)entityID, scene };
			if (entity && entity.HasComponent<RelationshipComponent>())
			{
				UUID parent = entity.GetComponent<RelationshipComponent>().Parent;
				if (parent != 0)
				{
					Entity parentEntity = scene->GetEntityByUUID(parent);
					if (parentEntity)
					{
						lua_pushnumber(L, (uint32_t)(entt::entity)parentEntity);
						return 1;
					}
				}
			}
		}
		lua_pushnumber(L, -1);
		return 1;
	}

	static int Lua_GetChildren(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		lua_newtable(L);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 1;

		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<RelationshipComponent>()) return 1;

		const auto& children = entity.GetComponent<RelationshipComponent>().Children;
		for (int i = 0; i < (int)children.size(); i++)
		{
			Entity child = scene->GetEntityByUUID(children[i]);
			if (child)
			{
				lua_pushnumber(L, (uint32_t)(entt::entity)child);
				lua_rawseti(L, -2, i + 1);
			}
		}
		return 1;
	}

	static int Lua_SetParent(lua_State* L)
	{
		uint32_t childID  = (uint32_t)lua_tonumber(L, 1);
		uint32_t parentID = (uint32_t)lua_tonumber(L, 2);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity child{ (entt::entity)childID, scene };
		Entity parent{ (entt::entity)parentID, scene };
		if (child && parent)
			scene->ParentEntity(child, parent);
		return 0;
	}

	static int Lua_Unparent(lua_State* L)
	{
		uint32_t childID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		Entity child{ (entt::entity)childID, scene };
		if (child) scene->UnparentEntity(child);
		return 0;
	}

	// =========================================================================
	// NEW BINDINGS - Active state
	// =========================================================================

	static int Lua_SetActive(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		bool active = lua_toboolean(L, 2) != 0;
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) return 0;
		entt::entity e = (entt::entity)entityID;
		if (!scene->GetRegistry().valid(e)) return 0;

		if (active)
		{
			if (scene->GetRegistry().all_of<DisabledComponent>(e))
				scene->GetRegistry().remove<DisabledComponent>(e);
		}
		else
		{
			if (!scene->GetRegistry().all_of<DisabledComponent>(e))
				scene->GetRegistry().emplace<DisabledComponent>(e);
		}
		return 0;
	}

	static int Lua_IsActive(lua_State* L)
	{
		uint32_t entityID = (uint32_t)lua_tonumber(L, 1);
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushboolean(L, 1); return 1; }
		entt::entity e = (entt::entity)entityID;
		bool disabled = scene->GetRegistry().valid(e) && scene->GetRegistry().all_of<DisabledComponent>(e);
		lua_pushboolean(L, disabled ? 0 : 1);
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Viewport & camera
	// =========================================================================

	static int Lua_GetViewportSize(lua_State* L)
	{
		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
		lua_pushnumber(L, scene->GetViewportWidth());
		lua_pushnumber(L, scene->GetViewportHeight());
		return 2;
	}

	static int Lua_ScreenToWorld(lua_State* L)
	{
		float sx = (float)lua_tonumber(L, 1);
		float sy = (float)lua_tonumber(L, 2);

		Scene* scene = LuaScriptEngine::GetSceneContext();
		if (!scene) { lua_pushnumber(L, sx); lua_pushnumber(L, sy); return 2; }

		Entity camEntity = scene->GetPrimaryCameraEntity();
		if (!camEntity || !camEntity.HasComponent<CameraComponent>() || !camEntity.HasComponent<TransformComponent>())
		{
			lua_pushnumber(L, sx); lua_pushnumber(L, sy); return 2;
		}

		const auto& camComp = camEntity.GetComponent<CameraComponent>();
		const auto& tc      = camEntity.GetComponent<TransformComponent>();
		float orthoSize  = camComp.Camera.GetOrthographicSize();
		float aspectRatio = camComp.Camera.GetAspectRatio();
		float vw = (float)scene->GetViewportWidth();
		float vh = (float)scene->GetViewportHeight();
		if (vw == 0 || vh == 0) { lua_pushnumber(L, sx); lua_pushnumber(L, sy); return 2; }

		float wx = tc.Translation.x + (sx / vw - 0.5f) * 2.0f * orthoSize * aspectRatio;
		float wy = tc.Translation.y + (0.5f - sy / vh) * 2.0f * orthoSize; // Y flipped
		lua_pushnumber(L, wx);
		lua_pushnumber(L, wy);
		return 2;
	}

	// =========================================================================
	// NEW BINDINGS - GetDeltaTime
	// =========================================================================

	static int Lua_GetDeltaTime(lua_State* L)
	{
		lua_pushnumber(L, LuaScriptEngine::s_CurrentDeltaTime);
		return 1;
	}

	// =========================================================================
	// NEW BINDINGS - Audio Engine
	// =========================================================================

	static int Lua_PlaySound(lua_State* L)
	{
		const char* path = lua_tostring(L, 1);
		float vol = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 1.0f;
		bool loop = lua_isboolean(L, 3) ? (lua_toboolean(L, 3) != 0) : false;

		if (!path) { lua_pushnumber(L, 0); return 1; }
		uint32_t id = AudioEngine::PlaySound(path, vol, loop);
		lua_pushnumber(L, id);
		return 1;
	}

	static int Lua_StopSound(lua_State* L)
	{
		const char* path = lua_tostring(L, 1);
		if (path)
			AudioEngine::StopSound(path);
		else
			AudioEngine::StopAllSounds();
		return 0;
	}

	static int Lua_SetSoundVolume(lua_State* L)
	{
		const char* path = lua_tostring(L, 1);
		float vol = (float)lua_tonumber(L, 2);
		if (path)
			AudioEngine::SetSoundVolume(path, vol);
		return 0;
	}

	static int Lua_SetMasterVolume(lua_State* L)
	{
		float vol = (float)lua_tonumber(L, 1);
		AudioEngine::SetMasterVolume(vol);
		return 0;
	}

	// =========================================================================
	// REGISTER ALL GLOBALS
	// =========================================================================

	static void RegisterGlobals(lua_State* L)
	{
		// ---- Key table ----
		lua_newtable(L);
#define SET_KEY(name) lua_pushinteger(L, Key::name); lua_setfield(L, -2, #name);
		SET_KEY(Space);
		SET_KEY(Apostrophe); SET_KEY(Comma); SET_KEY(Minus); SET_KEY(Period); SET_KEY(Slash);
		SET_KEY(D0); SET_KEY(D1); SET_KEY(D2); SET_KEY(D3); SET_KEY(D4);
		SET_KEY(D5); SET_KEY(D6); SET_KEY(D7); SET_KEY(D8); SET_KEY(D9);
		SET_KEY(Semicolon); SET_KEY(Equal);
		SET_KEY(A); SET_KEY(B); SET_KEY(C); SET_KEY(D); SET_KEY(E);
		SET_KEY(F); SET_KEY(G); SET_KEY(H); SET_KEY(I); SET_KEY(J);
		SET_KEY(K); SET_KEY(L); SET_KEY(M); SET_KEY(N); SET_KEY(O);
		SET_KEY(P); SET_KEY(Q); SET_KEY(R); SET_KEY(S); SET_KEY(T);
		SET_KEY(U); SET_KEY(V); SET_KEY(W); SET_KEY(X); SET_KEY(Y); SET_KEY(Z);
		SET_KEY(LeftBracket); SET_KEY(Backslash); SET_KEY(RightBracket); SET_KEY(GraveAccent);
		SET_KEY(Escape); SET_KEY(Enter); SET_KEY(Tab); SET_KEY(Backspace);
		SET_KEY(Insert); SET_KEY(Delete);
		SET_KEY(Right); SET_KEY(Left); SET_KEY(Down); SET_KEY(Up);
		SET_KEY(PageUp); SET_KEY(PageDown); SET_KEY(Home); SET_KEY(End);
		SET_KEY(CapsLock); SET_KEY(ScrollLock); SET_KEY(NumLock);
		SET_KEY(PrintScreen); SET_KEY(Pause);
		SET_KEY(F1);  SET_KEY(F2);  SET_KEY(F3);  SET_KEY(F4);
		SET_KEY(F5);  SET_KEY(F6);  SET_KEY(F7);  SET_KEY(F8);
		SET_KEY(F9);  SET_KEY(F10); SET_KEY(F11); SET_KEY(F12);
		SET_KEY(LeftShift);   SET_KEY(LeftControl);  SET_KEY(LeftAlt);  SET_KEY(LeftSuper);
		SET_KEY(RightShift);  SET_KEY(RightControl); SET_KEY(RightAlt); SET_KEY(RightSuper);
		SET_KEY(Menu);
#undef SET_KEY
		lua_setglobal(L, "Key");

		// ---- Mouse table ----
		lua_newtable(L);
#define SET_MOUSE(name) lua_pushinteger(L, Mouse::name); lua_setfield(L, -2, #name);
		SET_MOUSE(Button0); SET_MOUSE(Button1); SET_MOUSE(Button2); SET_MOUSE(Button3);
		SET_MOUSE(Button4); SET_MOUSE(Button5); SET_MOUSE(Button6); SET_MOUSE(Button7);
		SET_MOUSE(ButtonLast); SET_MOUSE(ButtonLeft); SET_MOUSE(ButtonRight); SET_MOUSE(ButtonMiddle);
#undef SET_MOUSE
		lua_setglobal(L, "Mouse");

		// ---- Input table (namespaced) ----
		lua_newtable(L);
		lua_pushcfunction(L, Lua_IsKeyPressed);         lua_setfield(L, -2, "IsKeyPressed");
		lua_pushcfunction(L, Lua_IsMouseButtonPressed); lua_setfield(L, -2, "IsMouseButtonPressed");
		lua_pushcfunction(L, Lua_GetMousePosition);     lua_setfield(L, -2, "GetMousePosition");
		lua_setglobal(L, "Input");

		// ---- Flat globals ----
		lua_pushcfunction(L, Lua_IsKeyPressed);         lua_setglobal(L, "IsKeyPressed");
		lua_pushcfunction(L, Lua_IsMouseButtonPressed); lua_setglobal(L, "IsMouseButtonPressed");
		lua_pushcfunction(L, Lua_GetMousePosition);     lua_setglobal(L, "GetMousePosition");

		lua_pushcfunction(L, Lua_Translate);            lua_setglobal(L, "Translate");
		lua_pushcfunction(L, Lua_SetPosition);          lua_setglobal(L, "SetPosition");
		lua_pushcfunction(L, Lua_GetPosition);          lua_setglobal(L, "GetPosition");
		lua_pushcfunction(L, Lua_SetLinearVelocity);    lua_setglobal(L, "SetLinearVelocity");
		lua_pushcfunction(L, Lua_GetLinearVelocity);    lua_setglobal(L, "GetLinearVelocity");
		lua_pushcfunction(L, Lua_ApplyLinearImpulse);   lua_setglobal(L, "ApplyLinearImpulse");
		lua_pushcfunction(L, Lua_ApplyForce);           lua_setglobal(L, "ApplyForce");
		lua_pushcfunction(L, Lua_Raycast);              lua_setglobal(L, "Raycast");

		lua_pushcfunction(L, Lua_GetRotation);          lua_setglobal(L, "GetRotation");
		lua_pushcfunction(L, Lua_SetRotation);          lua_setglobal(L, "SetRotation");
		lua_pushcfunction(L, Lua_GetScale);             lua_setglobal(L, "GetScale");
		lua_pushcfunction(L, Lua_SetScale);             lua_setglobal(L, "SetScale");

		lua_pushcfunction(L, Lua_FindEntityByName);     lua_setglobal(L, "FindEntityByName");

		lua_pushcfunction(L, Lua_LogInfo);              lua_setglobal(L, "LogInfo");
		lua_pushcfunction(L, Lua_LogWarn);              lua_setglobal(L, "LogWarn");
		lua_pushcfunction(L, Lua_LogError);             lua_setglobal(L, "LogError");

		lua_pushcfunction(L, Lua_ChangeScene);          lua_setglobal(L, "ChangeScene");
		lua_pushcfunction(L, Lua_GetCurrentSceneIndex); lua_setglobal(L, "GetCurrentSceneIndex");
		lua_pushcfunction(L, Lua_SetCurrentSceneIndex); lua_setglobal(L, "SetCurrentSceneIndex");

		// ---- New: Just-pressed input ----
		lua_pushcfunction(L, Lua_IsKeyJustPressed);     lua_setglobal(L, "IsKeyJustPressed");
		lua_pushcfunction(L, Lua_IsKeyJustReleased);    lua_setglobal(L, "IsKeyJustReleased");
		lua_pushcfunction(L, Lua_IsMouseJustPressed);   lua_setglobal(L, "IsMouseJustPressed");
		lua_pushcfunction(L, Lua_IsMouseJustReleased);  lua_setglobal(L, "IsMouseJustReleased");
		lua_pushcfunction(L, Lua_GetAxis);              lua_setglobal(L, "GetAxis");
		lua_getglobal(L, "Input");
		lua_pushcfunction(L, Lua_IsKeyJustPressed);     lua_setfield(L, -2, "IsKeyJustPressed");
		lua_pushcfunction(L, Lua_IsKeyJustReleased);    lua_setfield(L, -2, "IsKeyJustReleased");
		lua_pushcfunction(L, Lua_IsMouseJustPressed);   lua_setfield(L, -2, "IsMouseJustPressed");
		lua_pushcfunction(L, Lua_IsMouseJustReleased);  lua_setfield(L, -2, "IsMouseJustReleased");
		lua_pushcfunction(L, Lua_GetAxis);              lua_setfield(L, -2, "GetAxis");
		lua_pop(L, 1);

		// ---- New: Color / Visual ----
		lua_pushcfunction(L, Lua_SetColor);             lua_setglobal(L, "SetColor");
		lua_pushcfunction(L, Lua_GetColor);             lua_setglobal(L, "GetColor");
		lua_pushcfunction(L, Lua_SetAlpha);             lua_setglobal(L, "SetAlpha");
		lua_pushcfunction(L, Lua_SetTexture);           lua_setglobal(L, "SetTexture");
		lua_pushcfunction(L, Lua_PlayAnimation);        lua_setglobal(L, "PlayAnimation");
		lua_pushcfunction(L, Lua_StopAnimation);        lua_setglobal(L, "StopAnimation");
		lua_pushcfunction(L, Lua_PauseAnimation);       lua_setglobal(L, "PauseAnimation");
		lua_pushcfunction(L, Lua_SetAnimationFrame);   lua_setglobal(L, "SetAnimationFrame");
		lua_pushcfunction(L, Lua_IsAnimationPlaying);   lua_setglobal(L, "IsAnimationPlaying");

		// Table alias Animator.Play etc.
		lua_newtable(L);
		lua_pushcfunction(L, Lua_PlayAnimation);        lua_setfield(L, -2, "Play");
		lua_pushcfunction(L, Lua_StopAnimation);        lua_setfield(L, -2, "Stop");
		lua_pushcfunction(L, Lua_PauseAnimation);       lua_setfield(L, -2, "Pause");
		lua_pushcfunction(L, Lua_SetAnimationFrame);   lua_setfield(L, -2, "SetFrame");
		lua_pushcfunction(L, Lua_IsAnimationPlaying);   lua_setfield(L, -2, "IsPlaying");
		lua_setglobal(L, "Animator");

		// ---- New: Entity management ----
		lua_pushcfunction(L, Lua_CreateEntity);         lua_setglobal(L, "CreateEntity");
		lua_pushcfunction(L, Lua_DestroyEntity);        lua_setglobal(L, "DestroyEntity");
		lua_pushcfunction(L, Lua_DestroyEntityDelayed); lua_setglobal(L, "DestroyEntityDelayed");
		lua_pushcfunction(L, Lua_CloneEntity);          lua_setglobal(L, "CloneEntity");
		lua_pushcfunction(L, Lua_InstantiatePrefab);     lua_setglobal(L, "InstantiatePrefab");
		lua_pushcfunction(L, Lua_GetEntityName);        lua_setglobal(L, "GetEntityName");
		lua_pushcfunction(L, Lua_FindAllEntitiesByName);lua_setglobal(L, "FindAllEntitiesByName");
		lua_pushcfunction(L, Lua_GetAllEntities);       lua_setglobal(L, "GetAllEntities");

		// ---- New: Timer ----
		lua_pushcfunction(L, Lua_SetTimer);             lua_setglobal(L, "SetTimer");
		lua_pushcfunction(L, Lua_CancelTimer);          lua_setglobal(L, "CancelTimer");

		// ---- New: Angular physics ----
		lua_pushcfunction(L, Lua_GetAngularVelocity);   lua_setglobal(L, "GetAngularVelocity");
		lua_pushcfunction(L, Lua_SetAngularVelocity);   lua_setglobal(L, "SetAngularVelocity");
		lua_pushcfunction(L, Lua_ApplyTorque);          lua_setglobal(L, "ApplyTorque");
		lua_pushcfunction(L, Lua_ApplyAngularImpulse);  lua_setglobal(L, "ApplyAngularImpulse");

		// ---- New: Shape queries ----
		lua_pushcfunction(L, Lua_OverlapCircle);        lua_setglobal(L, "OverlapCircle");
		lua_pushcfunction(L, Lua_OverlapBox);           lua_setglobal(L, "OverlapBox");

		// ---- New: Hierarchy ----
		lua_pushcfunction(L, Lua_GetParent);            lua_setglobal(L, "GetParent");
		lua_pushcfunction(L, Lua_GetChildren);          lua_setglobal(L, "GetChildren");
		lua_pushcfunction(L, Lua_SetParent);            lua_setglobal(L, "SetParent");
		lua_pushcfunction(L, Lua_Unparent);             lua_setglobal(L, "Unparent");

		// ---- New: Active state ----
		lua_pushcfunction(L, Lua_SetActive);            lua_setglobal(L, "SetActive");
		lua_pushcfunction(L, Lua_IsActive);             lua_setglobal(L, "IsActive");

		// ---- New: Viewport / Camera ----
		lua_pushcfunction(L, Lua_GetViewportSize);      lua_setglobal(L, "GetViewportSize");
		lua_pushcfunction(L, Lua_ScreenToWorld);        lua_setglobal(L, "ScreenToWorld");

		// ---- New: DeltaTime ----
		lua_pushcfunction(L, Lua_GetDeltaTime);         lua_setglobal(L, "GetDeltaTime");

		// ---- New: Audio Engine ----
		lua_pushcfunction(L, Lua_PlaySound);            lua_setglobal(L, "PlaySound");
		lua_pushcfunction(L, Lua_StopSound);            lua_setglobal(L, "StopSound");
		lua_pushcfunction(L, Lua_SetSoundVolume);       lua_setglobal(L, "SetSoundVolume");
		lua_pushcfunction(L, Lua_SetMasterVolume);      lua_setglobal(L, "SetMasterVolume");

		// ---- Pure-Lua Math / Vec2 helpers ----
		static const char* s_MathLibLua = R"lua(
Math = {}
Math.Lerp    = function(a, b, t) return a + (b - a) * t end
Math.Clamp   = function(v, lo, hi) if v < lo then return lo end if v > hi then return hi end return v end
Math.Atan2   = function(y, x) return math.atan(y, x) end
Math.Sign    = function(v) if v > 0 then return 1 elseif v < 0 then return -1 else return 0 end end
Math.Abs     = math.abs
Math.Sqrt    = math.sqrt
Math.Floor   = math.floor
Math.Ceil    = math.ceil
Math.Round   = function(v) return math.floor(v + 0.5) end
Math.Pi      = math.pi
Math.Deg     = math.deg
Math.Rad     = math.rad
Math.Max     = math.max
Math.Min     = math.min
Math.Random  = math.random
Math.Sin     = math.sin
Math.Cos     = math.cos

Vec2 = {}
Vec2.New      = function(x, y) return { x = x or 0, y = y or 0 } end
Vec2.Length   = function(x, y) return math.sqrt(x * x + y * y) end
Vec2.LengthSq = function(x, y) return x * x + y * y end
Vec2.Normalize = function(x, y)
    local len = math.sqrt(x * x + y * y)
    if len == 0 then return 0, 0 end
    return x / len, y / len
end
Vec2.Dot      = function(ax, ay, bx, by) return ax * bx + ay * by end
Vec2.Distance = function(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return math.sqrt(dx * dx + dy * dy)
end
Vec2.DistanceSq = function(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return dx * dx + dy * dy
end
Vec2.Lerp     = function(ax, ay, bx, by, t)
    return ax + (bx - ax) * t, ay + (by - ay) * t
end
Vec2.Angle    = function(x, y) return math.atan(y, x) end

-- Persistent global data store (survives ChangeScene)
if Global == nil then Global = {} end
)lua";
		if (luaL_loadbuffer(L, s_MathLibLua, strlen(s_MathLibLua), "@WaffleLib") != LUA_OK ||
			lua_pcall(L, 0, 0, 0) != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			WF_CORE_ERROR("LuaScriptEngine: Failed to load Math/Vec2 library: {0}", err ? err : "unknown");
			lua_pop(L, 1);
		}
	}

	static std::string MakeTableKey(uint32_t entityID, const std::filesystem::path& scriptPath)
	{
		return "wf_entity_" + std::to_string(entityID) + "_" + scriptPath.stem().string();
	}

	static bool LoadScriptIntoEnv(lua_State* L, const std::filesystem::path& fullPath, const std::string& tableKey)
	{
		if (!VFS::Exists(fullPath))
		{
			WF_CORE_ERROR("LuaScriptEngine: Cannot open or read '{0}'", fullPath.string());
			return false;
		}

		std::string source = VFS::ReadFileAsString(fullPath);

		std::string wrapper = "return function(_ENV)\n" + source + "\nend";
		std::string chunkName = "@" + fullPath.filename().string();

		if (luaL_loadbufferx(L, wrapper.c_str(), wrapper.size(), chunkName.c_str(), "t") != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			WF_CORE_ERROR("LuaScriptEngine: Compile error in '{0}': {1}", fullPath.string(), err ? err : "unknown");
			lua_pop(L, 1);
			return false;
		}

		if (lua_pcall(L, 0, 1, 0) != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			WF_CORE_ERROR("LuaScriptEngine: Wrapper exec error '{0}': {1}", fullPath.string(), err ? err : "unknown");
			lua_pop(L, 1);
			return false;
		}

		int innerFnRef = luaL_ref(L, LUA_REGISTRYINDEX);

		lua_newtable(L);                                              // env
		lua_newtable(L);                                              // metatable
		lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);         // real _G
		lua_setfield(L, -2, "__index");                               // mt.__index = _G
		lua_setmetatable(L, -2);                                      // setmetatable(env, mt)

		int envRef = luaL_ref(L, LUA_REGISTRYINDEX);

		lua_rawgeti(L, LUA_REGISTRYINDEX, innerFnRef);
		lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);

		if (lua_pcall(L, 1, 0, 0) != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			WF_CORE_ERROR("LuaScriptEngine: Script error in '{0}': {1}", fullPath.string(), err ? err : "unknown");
			lua_pop(L, 1);
			luaL_unref(L, LUA_REGISTRYINDEX, envRef);
			luaL_unref(L, LUA_REGISTRYINDEX, innerFnRef);
			return false;
		}

		lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
		lua_setglobal(L, tableKey.c_str());

		luaL_unref(L, LUA_REGISTRYINDEX, envRef);
		luaL_unref(L, LUA_REGISTRYINDEX, innerFnRef);

		return true;
	}

	static void CallEnvFunction(lua_State* L, const std::string& tableKey, const char* funcName, int nArgs, const std::function<void()>& pushArgs)
	{
		lua_getglobal(L, tableKey.c_str());
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			return;
		}

		lua_getfield(L, -1, funcName);
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 2);
			return;
		}

		int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_pop(L, 1);

		lua_rawgeti(L, LUA_REGISTRYINDEX, fnRef);
		pushArgs();

		if (lua_pcall(L, nArgs, 0, 0) != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			WF_CORE_ERROR("LuaScriptEngine: Error in {0}::{1}: {2}", tableKey, funcName, err ? err : "unknown error");
			lua_pop(L, 1);
		}

		luaL_unref(L, LUA_REGISTRYINDEX, fnRef);
	}

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	void LuaScriptEngine::Init()
	{
		if (s_LuaState)
			return;

		s_LuaState = luaL_newstate();
		luaL_openlibs(s_LuaState);
		RegisterGlobals(s_LuaState);

		WF_CORE_INFO("LuaScriptEngine: Initialised.");
	}

	void LuaScriptEngine::Shutdown()
	{
		if (s_LuaState)
		{
			lua_close(s_LuaState);
			s_LuaState = nullptr;
			WF_CORE_INFO("LuaScriptEngine: Shut down.");
		}
	}

	class LuaContactListener : public b2ContactListener
	{
	public:
		void FireCollision(const char* funcName, b2Contact* contact)
		{
			b2Body* bodyA = contact->GetFixtureA()->GetBody();
			b2Body* bodyB = contact->GetFixtureB()->GetBody();

			Scene* scene = LuaScriptEngine::GetSceneContext();
			lua_State* L = LuaScriptEngine::GetLuaState();
			if (!scene || !L) return;

			auto& bodyMap = scene->GetBodyEntityMap();
			auto itA = bodyMap.find(bodyA);
			auto itB = bodyMap.find(bodyB);
			if (itA == bodyMap.end() || itB == bodyMap.end()) return;

			uint32_t idA = itA->second;
			uint32_t idB = itB->second;

			// Fire on A's scripts with B as the other
			FireOnEntity(L, scene, idA, funcName, idA, idB);
			// Fire on B's scripts with A as the other
			FireOnEntity(L, scene, idB, funcName, idB, idA);
		}

		void BeginContact(b2Contact* contact) override
		{
			// Check if either fixture is a sensor (trigger)
			bool isSensor = contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor();
			FireCollision(isSensor ? "OnTriggerBegin" : "OnCollisionBegin", contact);
		}
		void EndContact(b2Contact* contact) override
		{
			bool isSensor = contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor();
			FireCollision(isSensor ? "OnTriggerEnd" : "OnCollisionEnd", contact);
		}

	private:
		void FireOnEntity(lua_State* L, Scene* scene, uint32_t entityID,
			const char* funcName, uint32_t selfID, uint32_t otherID)
		{
			entt::entity enttID = (entt::entity)entityID;
			if (!scene->GetRegistry().valid(enttID)) return;
			if (!scene->GetRegistry().all_of<ScriptComponent>(enttID)) return;

			auto& sc = scene->GetRegistry().get<ScriptComponent>(enttID);
			for (const auto& tableKey : sc.ScriptTableKeys)
			{
				lua_getglobal(L, tableKey.c_str());
				if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

				lua_getfield(L, -1, funcName);
				if (!lua_isfunction(L, -1)) { lua_pop(L, 2); continue; }

				lua_pushnumber(L, selfID);
				lua_pushnumber(L, otherID);

				if (lua_pcall(L, 2, 0, 0) != LUA_OK)
				{
					const char* err = lua_tostring(L, -1);
					WF_CORE_ERROR("LuaScriptEngine: {0} error on entity {1}: {2}",
						funcName, entityID, err ? err : "unknown");
					lua_pop(L, 1);
				}
				lua_pop(L, 1); // pop the env table
			}
		}
	};

	LuaContactListener* LuaScriptEngine::s_ContactListener = nullptr;

	void LuaScriptEngine::OnRuntimeStart(Scene* scene)
	{
		Init();
		s_SceneContext = scene;

		if (!scene || !s_LuaState)
			return;

		auto view = scene->m_Registry.view<ScriptComponent>();
		for (auto entityID : view)
		{
			Entity entity{ entityID, scene };
			auto& sc = entity.GetComponent<ScriptComponent>();

			std::vector<std::string> scriptsToLoad = sc.ScriptPaths;
			if (scriptsToLoad.empty() && !sc.ClassName.empty())
				scriptsToLoad.push_back(sc.ClassName);

			sc.ScriptTableKeys.clear();

			for (const auto& scriptPath : scriptsToLoad)
			{
				if (scriptPath.empty())
					continue;

				std::filesystem::path fullPath = ResolveScriptPath(scriptPath);
				if (!VFS::Exists(fullPath))
				{
					WF_CORE_WARN("LuaScriptEngine: Script not found: '{0}'", scriptPath);
					continue;
				}

				std::string tableKey = MakeTableKey((uint32_t)entityID, fullPath);

				if (!LoadScriptIntoEnv(s_LuaState, fullPath, tableKey))
					continue;

				ScrapePublicFields(s_LuaState, tableKey, scriptPath, sc);

				WF_CORE_INFO("LuaScriptEngine: Loaded '{0}' -> table '{1}' (entity {2})",
					fullPath.string(), tableKey, (uint32_t)entityID);

				sc.ScriptTableKeys.push_back(tableKey);

				uint32_t id = (uint32_t)entityID;
				CallEnvFunction(s_LuaState, tableKey, "OnCreate", 1, [&]() {
					lua_pushnumber(s_LuaState, id);
					});
			}
		}

		if (scene->GetPhysicsWorld())
		{
			s_ContactListener = new LuaContactListener();
			scene->GetPhysicsWorld()->SetContactListener(s_ContactListener);
		}
	}

	void LuaScriptEngine::OnRuntimeStop(Scene* scene)
	{
		if (!scene || !s_LuaState)
		{
			s_SceneContext = nullptr;
			return;
		}

		auto view = scene->m_Registry.view<ScriptComponent>();
		for (auto entityID : view)
		{
			Entity entity{ entityID, scene };
			auto& sc = entity.GetComponent<ScriptComponent>();

			uint32_t id = (uint32_t)entityID;

			for (const auto& tableKey : sc.ScriptTableKeys)
			{
				CallEnvFunction(s_LuaState, tableKey, "OnDestroy", 1, [&]() {
					lua_pushnumber(s_LuaState, id);
					});

				lua_pushnil(s_LuaState);
				lua_setglobal(s_LuaState, tableKey.c_str());
			}

			sc.ScriptTableKeys.clear();
		}

		if (scene && scene->GetPhysicsWorld())
			scene->GetPhysicsWorld()->SetContactListener(nullptr);

		delete s_ContactListener;
		s_ContactListener = nullptr;

		// Clean up timers and release their Lua refs
		for (auto& t : s_Timers)
		{
			if (t.Active && t.CallbackRef != LUA_NOREF)
				luaL_unref(s_LuaState, LUA_REGISTRYINDEX, t.CallbackRef);
		}
		s_Timers.clear();
		s_PendingDestroys.clear();
		s_DelayedDestroys.clear();

		// NOTE: We intentionally do NOT clear s_Global or the Lua "Global" table.
		// It is meant to persist across ChangeScene calls.

		s_SceneContext = nullptr;
	}

	// -------------------------------------------------------------------------
	// InitScriptsForEntity - load + OnCreate for a single entity.
	// Called for prefabs that are instantiated at runtime via Lua so their
	// ScriptTableKeys are populated and OnUpdate/OnCreate actually fire.
	// -------------------------------------------------------------------------
	void LuaScriptEngine::InitScriptsForEntity(Scene* scene, Entity entity)
	{
		if (!scene || !s_LuaState || !entity) return;

		auto entityID = (entt::entity)(uint32_t)entity;
		if (!scene->m_Registry.all_of<ScriptComponent>(entityID)) return;

		auto& sc = scene->m_Registry.get<ScriptComponent>(entityID);

		std::vector<std::string> scriptsToLoad = sc.ScriptPaths;
		if (scriptsToLoad.empty() && !sc.ClassName.empty())
			scriptsToLoad.push_back(sc.ClassName);

		sc.ScriptTableKeys.clear();

		for (const auto& scriptPath : scriptsToLoad)
		{
			if (scriptPath.empty()) continue;

			std::filesystem::path fullPath = ResolveScriptPath(scriptPath);
			if (!VFS::Exists(fullPath))
			{
				WF_CORE_WARN("LuaScriptEngine::InitScriptsForEntity: Script not found: '{0}'", scriptPath);
				continue;
			}

			std::string tableKey = MakeTableKey((uint32_t)entityID, fullPath);

			if (!LoadScriptIntoEnv(s_LuaState, fullPath, tableKey))
				continue;

			ScrapePublicFields(s_LuaState, tableKey, scriptPath, sc);

			WF_CORE_INFO("LuaScriptEngine: Loaded '{0}' -> table '{1}' (entity {2}, runtime-spawned)",
				fullPath.string(), tableKey, (uint32_t)entityID);

			sc.ScriptTableKeys.push_back(tableKey);

			uint32_t id = (uint32_t)entityID;
			CallEnvFunction(s_LuaState, tableKey, "OnCreate", 1, [&]() {
				lua_pushnumber(s_LuaState, id);
				});
		}
	}

	void LuaScriptEngine::OnRuntimeUpdate(Scene* scene, Timestep ts)
	{
		if (!scene || !s_LuaState)
			return;

		// --- Frame setup ---
		s_CurrentDeltaTime = (float)ts;
		UpdateInputStates(); // snapshot prev/curr key and mouse states

		// --- Script update (skip disabled entities) ---
		auto view = scene->m_Registry.view<ScriptComponent>();
		for (auto entityID : view)
		{
			// Skip disabled entities
			if (scene->m_Registry.all_of<DisabledComponent>(entityID)) continue;

			Entity entity{ entityID, scene };
			auto& sc = entity.GetComponent<ScriptComponent>();

			uint32_t id = (uint32_t)entityID;
			float tsf = (float)ts;

			for (const auto& tableKey : sc.ScriptTableKeys)
			{
				CallEnvFunction(s_LuaState, tableKey, "OnUpdate", 2, [&]() {
					lua_pushnumber(s_LuaState, id);
					lua_pushnumber(s_LuaState, tsf);
					});

				// Sync Public table values back into sc.Fields for live inspector display
				for (auto& [scriptPath, fieldList] : sc.Fields)
				{
					if (fieldList.empty()) continue;

					lua_getglobal(s_LuaState, tableKey.c_str());
					if (!lua_istable(s_LuaState, -1)) { lua_pop(s_LuaState, 1); continue; }
					lua_getfield(s_LuaState, -1, "Public");
					if (!lua_istable(s_LuaState, -1)) { lua_pop(s_LuaState, 2); continue; }

					for (auto& field : fieldList)
					{
						lua_getfield(s_LuaState, -1, field.Name.c_str());

						switch (field.Type)
						{
						case LuaFieldType::Float:
							if (lua_isnumber(s_LuaState, -1))
								field.FloatVal = (float)lua_tonumber(s_LuaState, -1);
							break;
						case LuaFieldType::Int:
							if (lua_isinteger(s_LuaState, -1))
								field.IntVal = (int)lua_tointeger(s_LuaState, -1);
							break;
						case LuaFieldType::Bool:
							if (lua_isboolean(s_LuaState, -1))
								field.BoolVal = lua_toboolean(s_LuaState, -1) != 0;
							break;
						case LuaFieldType::String:
							if (lua_isstring(s_LuaState, -1))
								field.StringVal = lua_tostring(s_LuaState, -1);
							break;
						}

						lua_pop(s_LuaState, 1); // pop field value
					}

					lua_pop(s_LuaState, 2); // pop Public + env
				}
			}
		}

		// --- Tick timers ---
		for (auto& t : s_Timers)
		{
			if (!t.Active) continue;
			t.Remaining -= (float)ts;
			if (t.Remaining <= 0.0f)
			{
				t.Active = false;
				// Fire callback
				lua_rawgeti(s_LuaState, LUA_REGISTRYINDEX, t.CallbackRef);
				if (lua_isfunction(s_LuaState, -1))
				{
					if (lua_pcall(s_LuaState, 0, 0, 0) != LUA_OK)
					{
						const char* err = lua_tostring(s_LuaState, -1);
						WF_CORE_ERROR("LuaScriptEngine: Timer callback error: {0}", err ? err : "unknown");
						lua_pop(s_LuaState, 1);
					}
				}
				else lua_pop(s_LuaState, 1);
				luaL_unref(s_LuaState, LUA_REGISTRYINDEX, t.CallbackRef);
				t.CallbackRef = LUA_NOREF;
			}
		}
		// Compact finished timers
		s_Timers.erase(std::remove_if(s_Timers.begin(), s_Timers.end(),
			[](const LuaTimerEntry& t) { return !t.Active; }), s_Timers.end());

		// --- Tick delayed destroys ---
		for (auto& d : s_DelayedDestroys)
			d.Remaining -= (float)ts;
		for (auto& d : s_DelayedDestroys)
		{
			if (d.Remaining <= 0.0f)
				s_PendingDestroys.push_back(d.EntityID);
		}
		s_DelayedDestroys.erase(
			std::remove_if(s_DelayedDestroys.begin(), s_DelayedDestroys.end(),
				[](const LuaDelayedDestroy& d) { return d.Remaining <= 0.0f; }),
			s_DelayedDestroys.end());

		// --- Process deferred destroys ---
		for (uint32_t id : s_PendingDestroys)
		{
			entt::entity e = (entt::entity)id;
			if (scene->m_Registry.valid(e))
			{
				Entity entity{ e, scene };
				scene->DestroyEntity(entity);
			}
		}
		s_PendingDestroys.clear();
	}
}