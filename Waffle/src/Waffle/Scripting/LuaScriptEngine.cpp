#include "wfpch.h"
#include "LuaScriptEngine.h"

#include "Waffle/Core/Log.h"
#include "Waffle/Core/Input.h"
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"
#include "Waffle/Scene/Components.h"

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

	// -------------------------------------------------------------------------
	// Path resolution
	// -------------------------------------------------------------------------

	static std::filesystem::path ResolveScriptPath(const std::string& scriptPath)
	{
		// 1. Try the path as-is
		std::filesystem::path fullPath = scriptPath;
		if (std::filesystem::exists(fullPath))
			return fullPath;

		// 2. Try relative to Assets/
		fullPath = std::filesystem::path("Assets") / scriptPath;
		if (std::filesystem::exists(fullPath))
			return fullPath;

		// 3. Append .lua extension if missing
		if (!fullPath.has_extension())
		{
			fullPath += ".lua";
			if (std::filesystem::exists(fullPath))
				return fullPath;
		}

		// 4. Recursive search inside Assets/ for filename match
		if (std::filesystem::exists("Assets"))
		{
			std::string searchFilename = std::filesystem::path(scriptPath).filename().string();
			if (searchFilename.find(".lua") == std::string::npos)
				searchFilename += ".lua";

			for (auto& entry : std::filesystem::recursive_directory_iterator("Assets"))
			{
				if (entry.is_regular_file() && entry.path().filename().string() == searchFilename)
					return entry.path();
			}
		}

		return scriptPath;
	}

	// -------------------------------------------------------------------------
	// Lua C bindings — Input
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
	// Lua C bindings — Transform / Physics
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
					// Dynamic: never stomp position — the solver owns it.
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

	// -------------------------------------------------------------------------
	// Lua C bindings — Raycasting
	// -------------------------------------------------------------------------

	// Raycast(entityID, offsetX, offsetY, dirX, dirY, distance) -> bool
	//
	// Casts a ray from the entity's position + offset in the given direction
	// for the given distance. Returns true if anything other than the entity
	// itself was hit. Useful for ground detection:
	//
	//   local isGrounded = Raycast(entity, 0, -0.5, 0, -1, 0.6)
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
			lua_pushboolean(L, 0);
			return 1;
		}

		Entity entity{ (entt::entity)entityID, scene };
		if (!entity || !entity.HasComponent<TransformComponent>())
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		// Guard: need a physics world
		if (!scene->GetPhysicsWorld())
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const auto& tc = entity.GetComponent<TransformComponent>();
		b2Vec2 origin(tc.Translation.x + offsetX, tc.Translation.y + offsetY);
		b2Vec2 end(origin.x + dirX * dist, origin.y + dirY * dist);

		// Box2D requires the two endpoints to differ
		if (origin.x == end.x && origin.y == end.y)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		// Inline callback — ignores the casting entity's own fixtures
		struct GroundCallback : public b2RayCastCallback
		{
			uint32_t ignoreID;
			bool hit = false;
			std::unordered_map<b2Body*, uint32_t>* bodyMap; // ADD

			float ReportFixture(b2Fixture* fixture,
				const b2Vec2& /*point*/,
				const b2Vec2& /*normal*/,
				float fraction) override
			{
				b2Body* body = fixture->GetBody();
				auto it = bodyMap->find(body);
				if (it != bodyMap->end() && it->second == ignoreID)
					return -1.0f; // skip self

				hit = true;
				return fraction;
			}
		};

		GroundCallback cb;
		cb.ignoreID = entityID;
		cb.bodyMap = &scene->GetBodyEntityMap(); // ADD
		scene->GetPhysicsWorld()->RayCast(&cb, origin, end);

		lua_pushboolean(L, cb.hit ? 1 : 0);
		return 1;
	}

	// -------------------------------------------------------------------------
	// Lua C bindings — Logging
	// -------------------------------------------------------------------------

	static int Lua_LogInfo(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_CORE_INFO("Lua: {0}", msg);
		return 0;
	}

	static int Lua_LogWarn(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_CORE_WARN("Lua: {0}", msg);
		return 0;
	}

	static int Lua_LogError(lua_State* L)
	{
		const char* msg = lua_tostring(L, 1);
		if (msg)
			WF_CORE_ERROR("Lua: {0}", msg);
		return 0;
	}

	// -------------------------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------------------------

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

		lua_pushcfunction(L, Lua_GetRotation); lua_setglobal(L, "GetRotation");
		lua_pushcfunction(L, Lua_SetRotation); lua_setglobal(L, "SetRotation");
		lua_pushcfunction(L, Lua_GetScale);    lua_setglobal(L, "GetScale");
		lua_pushcfunction(L, Lua_SetScale);    lua_setglobal(L, "SetScale");

		lua_pushcfunction(L, Lua_FindEntityByName); lua_setglobal(L, "FindEntityByName");

		lua_pushcfunction(L, Lua_LogInfo);              lua_setglobal(L, "LogInfo");
		lua_pushcfunction(L, Lua_LogWarn);              lua_setglobal(L, "LogWarn");
		lua_pushcfunction(L, Lua_LogError);             lua_setglobal(L, "LogError");
	}

	static std::string MakeTableKey(uint32_t entityID, const std::filesystem::path& scriptPath)
	{
		return "wf_entity_" + std::to_string(entityID) + "_" + scriptPath.stem().string();
	}

	static bool LoadScriptIntoEnv(lua_State* L, const std::filesystem::path& fullPath, const std::string& tableKey)
	{
		std::ifstream file(fullPath, std::ios::binary);
		if (!file.is_open())
		{
			WF_CORE_ERROR("LuaScriptEngine: Cannot open '{0}'", fullPath.string());
			return false;
		}
		std::string source((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();

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
			lua_State* L = LuaScriptEngine::GetLuaState(); // you'll need to expose this
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

		void BeginContact(b2Contact* contact) override { FireCollision("OnCollisionBegin", contact); }
		void EndContact(b2Contact* contact)   override { FireCollision("OnCollisionEnd", contact); }

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
				if (!std::filesystem::exists(fullPath))
				{
					WF_CORE_WARN("LuaScriptEngine: Script not found: '{0}'", scriptPath);
					continue;
				}

				std::string tableKey = MakeTableKey((uint32_t)entityID, fullPath);

				if (!LoadScriptIntoEnv(s_LuaState, fullPath, tableKey))
					continue;

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

		s_SceneContext = nullptr;
	}

	void LuaScriptEngine::OnRuntimeUpdate(Scene* scene, Timestep ts)
	{
		if (!scene || !s_LuaState)
			return;

		auto view = scene->m_Registry.view<ScriptComponent>();
		for (auto entityID : view)
		{
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
			}
		}
	}

}