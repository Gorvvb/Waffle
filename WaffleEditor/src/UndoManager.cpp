#define NOMINMAX
#include "UndoManager.h"

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Core/Log.h"

#include <yaml-cpp/yaml.h>

namespace Waffle {

	bool UndoManager::s_Enabled = true;
	std::vector<UndoManager::Entry> UndoManager::s_Undo;
	std::vector<UndoManager::Entry> UndoManager::s_Redo;
	UndoManager::Entry UndoManager::s_Staged;
	bool UndoManager::s_StagedValid = false;

	// History depth: old entries fall off the back; plenty for an editing session.
	constexpr size_t kMaxUndoEntries = 128;

	void UndoManager::Reset()
	{
		s_Undo.clear();
		s_Redo.clear();
		s_StagedValid = false;
	}

	void UndoManager::CaptureBefore(Entity entity)
	{
		if (!entity || !s_Enabled)
			return;
		s_Staged.Target = (uint64_t)entity.GetUUID();
		s_Staged.Before = SceneSerializer::SerializeEntityToString(entity);
		s_Staged.After.clear();
		s_StagedValid = true;
	}

	void UndoManager::Commit(Entity entity)
	{
		CommitSnapshot(entity ? SceneSerializer::SerializeEntityToString(entity) : std::string());
	}

	void UndoManager::CommitSnapshot(const std::string& afterYaml)
	{
		if (!s_StagedValid)
			return;
		s_StagedValid = false;
		if (s_Staged.Before == afterYaml)
			return; // nothing actually changed

		s_Staged.After = afterYaml;
		s_Undo.push_back(s_Staged);
		if (s_Undo.size() > kMaxUndoEntries)
			s_Undo.erase(s_Undo.begin());
		s_Redo.clear();
	}

	void UndoManager::NotifyEntityCreated(Entity entity)
	{
		if (!entity)
			return;
		CaptureBefore(entity);
		s_Staged.Before.clear(); // created: did not exist before
		Commit(entity);
	}

	// Parent-first snapshots of the entity and all Relationship descendants (depth-capped).
	static void CollectTree(Scene* scene, Entity entity, std::vector<std::string>& out, int depth)
	{
		if (!entity || depth > 64)
			return;
		out.push_back(SceneSerializer::SerializeEntityToString(entity));
		if (entity.HasComponent<RelationshipComponent>())
		{
			for (uint64_t childUUID : entity.GetComponent<RelationshipComponent>().Children)
				if (Entity child = scene->GetEntityByUUID(childUUID))
					CollectTree(scene, child, out, depth + 1);
		}
	}

	void UndoManager::NotifyCreatedTree(Entity root)
	{
		if (!root || !s_Enabled)
			return;

		Entry entry;
		entry.Tree = true;
		CollectTree(root.GetScene(), root, entry.TreeSnapshots, 0);
		if (entry.TreeSnapshots.empty())
			return;

		s_Undo.push_back(entry);
		if (s_Undo.size() > kMaxUndoEntries)
			s_Undo.erase(s_Undo.begin());
		s_Redo.clear();
	}

	// One side of an entry: empty yaml = the entity should not exist, else restore the snapshot.
	void UndoManager::Apply(Scene* scene, const Entry& entry, bool undoing)
	{
		if (entry.Tree)
		{
			if (undoing)
			{
				// Destroy deepest-first so parents still see their children when they go.
				for (auto it = entry.TreeSnapshots.rbegin(); it != entry.TreeSnapshots.rend(); ++it)
				{
					YAML::Node node = YAML::Load(*it);
					uint64_t uuid = node["Entity"] ? node["Entity"].as<uint64_t>(0) : 0;
					if (uuid)
						if (Entity existing = scene->GetEntityByUUID(uuid))
							scene->DestroyEntity(existing);
				}
			}
			else
			{
				// Restore parent-first so hierarchy re-links find their parents.
				for (const auto& snapshot : entry.TreeSnapshots)
					SceneSerializer::RestoreEntityFromSnapshot(scene, snapshot);
			}
			return;
		}

		const std::string& yaml = undoing ? entry.Before : entry.After;
		if (yaml.empty())
		{
			if (Entity existing = scene->GetEntityByUUID(entry.Target))
				scene->DestroyEntity(existing);
			return;
		}
		Entity restored = SceneSerializer::RestoreEntityFromSnapshot(scene, yaml);
		if (!restored)
			WF_CORE_WARN("Undo: failed to restore entity {0}", entry.Target);
	}

	void UndoManager::Undo(Scene* scene)
	{
		if (s_Undo.empty() || !scene)
			return;
		Entry entry = s_Undo.back();
		s_Undo.pop_back();
		Apply(scene, entry, true);
		s_Redo.push_back(entry);
	}

	void UndoManager::Redo(Scene* scene)
	{
		if (s_Redo.empty() || !scene)
			return;
		Entry entry = s_Redo.back();
		s_Redo.pop_back();
		Apply(scene, entry, false);
		s_Undo.push_back(entry);
	}

}
