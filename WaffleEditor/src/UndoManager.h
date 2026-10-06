#pragma once

#include <string>
#include <vector>

namespace Waffle {

class Scene;
class Entity;

// Snapshot-based undo/redo for entity-level edits (delete, duplicate, create, component
// add/remove, gizmo moves). Each entry stores one entity's full YAML before and after a
// mutation; an empty side means "did not exist" (create/delete). Editor-only: it lives in
// WaffleEditor and is always applied to the EDIT scene while not playing.
class UndoManager
{
public:
	// Scene changed (load/new/play/prefab) - drop all history.
	static void Reset();

	// Stages the entity's current state; follow with Commit() once the mutation ran.
	static void CaptureBefore(Entity entity);
	// Completes the staged entry with the entity's fresh state (invalid entity = "no longer exists").
	static void Commit(Entity entity);
	// Completes the staged entry with an explicit after-snapshot (empty = "no longer exists").
	static void CommitSnapshot(const std::string& afterYaml);

	// Records a freshly created entity (undo removes it again).
	static void NotifyEntityCreated(Entity entity);

	// Records a freshly created entity SUBTREE (prefab instantiate, UI canvas + element):
	// undo destroys the whole tree, redo restores every snapshot (parent-first order).
	static void NotifyCreatedTree(Entity root);

	// Editor sets this per frame; while disabled (play mode) nothing is recorded.
	static void SetEnabled(bool enabled) { s_Enabled = enabled; }

	static void Undo(Scene* scene);
	static void Redo(Scene* scene);
	static bool CanUndo() { return !s_Undo.empty(); }
	static bool CanRedo() { return !s_Redo.empty(); }

private:
	struct Entry
	{
		bool Tree = false;                 // created-subtree entry (see NotifyCreatedTree)
		uint64_t Target = 0;
		std::string Before;
		std::string After;
		// Tree entries: parent-first entity snapshots of the created subtree.
		std::vector<std::string> TreeSnapshots;
	};
	static void Apply(Scene* scene, const Entry& entry, bool undoing);

	static bool s_Enabled;
	static std::vector<Entry> s_Undo;
	static std::vector<Entry> s_Redo;
	static Entry s_Staged;
	static bool s_StagedValid;
};

}
