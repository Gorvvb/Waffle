using System.Collections;
using System.Numerics;
using Waffle.Scripting.Internal;

namespace Waffle;

/// <summary>Yield instruction: resume the coroutine after this many seconds have elapsed.</summary>
public sealed class WaitForSeconds
{
    internal readonly float Seconds;
    public WaitForSeconds(float seconds) => Seconds = seconds;
}

/// <summary>Yield instruction: resume the coroutine after this many frames.</summary>
public sealed class WaitForFrames
{
    internal readonly int Frames;
    public WaitForFrames(int frames) => Frames = frames;
}

/// <summary>Yield instruction: resume once the condition returns true (checked every frame).</summary>
public sealed class WaitUntil
{
    internal readonly Func<bool> Condition;
    public WaitUntil(Func<bool> condition) => Condition = condition;
}

/// <summary>Yield instruction: resume once the condition returns false (checked every frame).</summary>
public sealed class WaitWhile
{
    internal readonly Func<bool> Condition;
    public WaitWhile(Func<bool> condition) => Condition = condition;
}

/// <summary>Handle to a running coroutine; Stop() cancels it, IsRunning polls it.</summary>
public struct CoroutineHandle
{
    internal uint Id;
    internal bool Valid;

    public readonly bool IsRunning => Valid && CoroutineSystem.IsRunning(Id);
    public void Stop() => CoroutineSystem.Stop(Id);
}

// Coroutine runner: routines advance from the engine's frame pump, after scripts and AI tick.
internal static class CoroutineSystem
{
    private sealed class Routine
    {
        public uint Id;
        public uint OwnerEntity;
        public Stack<IEnumerator> Frames = new(); // nested yield return SomeEnumerator() pushes here
        public float WaitSeconds;
        public int WaitFrames;
        public Func<bool>? WaitCondition;         // WaitUntil (true resumes) / WaitWhile (false resumes)
        public bool WaitWhile;
    }

    private static readonly List<Routine> s_Active = new();
    private static uint s_NextId = 1;

    internal static unsafe CoroutineHandle Start(Entity owner, IEnumerator routine)
    {
        if (NativeApi.EditorGizmoPass() != 0)
        {
            ScriptRuntime.LogWarn("Editor gizmo preview: coroutines have no effect outside play mode");
            return default;
        }

        var entry = new Routine { Id = s_NextId++, OwnerEntity = owner.Id };
        entry.Frames.Push(routine);
        s_Active.Add(entry);
        return new CoroutineHandle { Id = entry.Id, Valid = true };
    }

    internal static bool IsRunning(uint id)
    {
        for (int i = 0; i < s_Active.Count; i++)
            if (s_Active[i].Id == id)
                return true;
        return false;
    }

    internal static void Stop(uint id)
    {
        for (int i = 0; i < s_Active.Count; i++)
        {
            if (s_Active[i].Id == id)
            {
                s_Active.RemoveAt(i);
                return;
            }
        }
    }

    internal static void StopAllForEntity(uint entityId)
    {
        for (int i = s_Active.Count - 1; i >= 0; i--)
        {
            if (s_Active[i].OwnerEntity == entityId)
                s_Active.RemoveAt(i);
        }
    }

    internal static void Pump(float dt)
    {
        // Snapshot the count: a coroutine body may start or stop coroutines mid-iteration.
        int count = s_Active.Count;
        for (int i = 0; i < count && i < s_Active.Count; i++)
        {
            Routine routine = s_Active[i];

            if (routine.WaitSeconds > 0f)
            {
                routine.WaitSeconds -= dt;
                if (routine.WaitSeconds > 0f)
                    continue;
                routine.WaitSeconds = 0f;
            }
            if (routine.WaitFrames > 0)
            {
                routine.WaitFrames--;
                continue;
            }
            if (routine.WaitCondition is not null)
            {
                bool keepWaiting = false;
                try { keepWaiting = routine.WaitWhile ? routine.WaitCondition() : !routine.WaitCondition(); }
                catch (Exception e)
                {
                    ScriptRuntime.LogWarn($"Coroutine wait threw: {e.Message}");
                    keepWaiting = false; // resume rather than hang on a throwing condition
                }
                if (keepWaiting)
                    continue;
                routine.WaitCondition = null;
            }

            if (!Advance(routine))
                s_Active.RemoveAt(i--);
        }
    }

    // Runs the iterator one step; true while the routine still has work, false when it completed.
    private static bool Advance(Routine routine)
    {
        while (routine.Frames.Count > 0)
        {
            IEnumerator current = routine.Frames.Peek();
            bool moved;
            try { moved = current.MoveNext(); }
            catch (Exception e)
            {
                ScriptRuntime.LogWarn($"Coroutine threw: {e.Message}");
                return false;
            }

            if (!moved)
            {
                routine.Frames.Pop();
                continue;
            }

            switch (current.Current)
            {
                case WaitForSeconds wait:
                    routine.WaitSeconds = wait.Seconds;
                    return true;
                case WaitForFrames frames:
                    routine.WaitFrames = frames.Frames;
                    return true;
                case WaitUntil until:
                    routine.WaitCondition = until.Condition;
                    routine.WaitWhile = false;
                    return true;
                case WaitWhile waitWhile:
                    routine.WaitCondition = waitWhile.Condition;
                    routine.WaitWhile = true;
                    return true;
                case IEnumerator nested:
                    routine.Frames.Push(nested);
                    return true;
                default:
                    return true; // yield return null / anything else: resume next frame
            }
        }
        return false;
    }

    internal static void Clear() => s_Active.Clear();
}
