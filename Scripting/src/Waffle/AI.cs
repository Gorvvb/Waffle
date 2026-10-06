namespace Waffle;

/// <summary>Shared key/value memory for AI: typed setters plus fallback-returning getters, one per StateMachine.</summary>
public sealed class Blackboard
{
    private readonly Dictionary<string, object> _values = new();

    public void Set(string key, float value) => _values[key] = value;
    public void Set(string key, int value) => _values[key] = value;
    public void Set(string key, bool value) => _values[key] = value;
    public void Set(string key, string value) => _values[key] = value;
    public void Set(string key, Entity value) => _values[key] = value;
    public void Set(string key, System.Numerics.Vector2 value) => _values[key] = value;

    public float GetFloat(string key, float fallback = 0f) => _values.TryGetValue(key, out var v) && v is float f ? f : fallback;
    public int GetInt(string key, int fallback = 0) => _values.TryGetValue(key, out var v) && v is int i ? i : fallback;
    public bool GetBool(string key, bool fallback = false) => _values.TryGetValue(key, out var v) && v is bool b ? b : fallback;
    public string GetString(string key, string fallback = "") => _values.TryGetValue(key, out var v) && v is string s ? s : fallback;
    public Entity GetEntity(string key) => _values.TryGetValue(key, out var v) && v is Entity e ? e : Entity.None;
    public System.Numerics.Vector2 GetVector2(string key, System.Numerics.Vector2 fallback = default)
        => _values.TryGetValue(key, out var v) && v is System.Numerics.Vector2 vec ? vec : fallback;

    public bool Has(string key) => _values.ContainsKey(key);
    public bool Remove(string key) => _values.Remove(key);
    public void Clear() => _values.Clear();
}

/// <summary>A state-machine state: assign Enter/Update/Exit after creation; Update receives delta time.</summary>
public sealed class AIState
{
    /// <summary>Runs when the machine switches into this state.</summary>
    public Action? Enter;
    /// <summary>Runs every tick while this is the current state.</summary>
    public Action<float>? Update;
    /// <summary>Runs when the machine switches away from this state.</summary>
    public Action? Exit;

    internal readonly string Name;
    internal AIState(string name) => Name = name;
}

/// <summary>A directed edge of a StateMachine - chain When()/After() on the returned object to make it conditional.</summary>
public sealed class AITransition
{
    internal readonly string? From; // null = global (from any state)
    internal readonly string To;
    internal Func<bool>? Guard;
    internal string? Trigger;
    internal float AfterSeconds;

    internal AITransition(string? from, string to)
    {
        From = from;
        To = to;
    }

    /// <summary>The transition is only taken while the guard returns true (evaluated every tick, or when its trigger fires).</summary>
    public AITransition When(Func<bool> guard)
    {
        Guard = guard;
        return this;
    }

    /// <summary>The transition is not taken until the current state has been active this many seconds.</summary>
    public AITransition After(float seconds)
    {
        AfterSeconds = seconds;
        return this;
    }

    internal bool Ready(float timeInState) => timeInState >= AfterSeconds;
}

/// <summary>Code-driven AI state machine for enemies and NPCs: build states and transitions in OnStart, call Start(), and the engine ticks it automatically after all script updates. Transitions pick the first eligible edge in add order (current state's own, then globals); the live state shows in the editor inspector while playing.</summary>
public sealed class StateMachine
{
    /// <summary>Name of the current state, or empty before Start.</summary>
    public string CurrentState => _current?.Name ?? "";
    /// <summary>Name of the state active before the last transition.</summary>
    public string PreviousState => _previous?.Name ?? "";
    /// <summary>Seconds the current state has been active.</summary>
    public float TimeInState => _timeInState;
    /// <summary>True on the tick a transition happened - for one-shot reactions in OnUpdate.</summary>
    public bool JustChanged { get; private set; }
    /// <summary>False until Start has been called.</summary>
    public bool IsRunning => _current is not null;
    /// <summary>Shared memory for this machine's states (e.g. target entity, patrol direction, aggro level).</summary>
    public Blackboard Blackboard { get; } = new();

    private readonly Entity _owner;
    private readonly Dictionary<string, AIState> _states = new();
    private readonly List<AITransition> _transitions = new();
    private AIState? _current;
    private AIState? _previous;
    private float _timeInState;

    /// <summary>The owner is used for editor debug display and to stop the machine when the entity is destroyed.</summary>
    public StateMachine(Entity owner)
    {
        _owner = owner;
        AIRegistry.Register(owner, this);
    }

    /// <summary>Adds a state; callbacks may be null and assigned later through the returned object.</summary>
    public AIState AddState(string name, Action? enter = null, Action<float>? update = null, Action? exit = null)
    {
        var state = new AIState(name) { Enter = enter, Update = update, Exit = exit };
        _states[name] = state;
        return state;
    }

    /// <summary>Automatic transition: taken while its guard (if any) passes and After (if set) has elapsed.</summary>
    public AITransition AddTransition(string from, string to) => New(from, to, null);

    /// <summary>Explicit transition: taken only when Fire(trigger) is called (guard/After still apply).</summary>
    public AITransition AddTriggerTransition(string from, string to, string trigger) => New(from, to, trigger);

    /// <summary>Automatic transition from ANY state - checked after the current state's own transitions.</summary>
    public AITransition AddGlobalTransition(string to) => New(null, to, null);

    /// <summary>Explicit transition from any state - e.g. Fire("Hit") for stun/knockdown reactions.</summary>
    public AITransition AddGlobalTriggerTransition(string to, string trigger) => New(null, to, trigger);

    private AITransition New(string? from, string to, string? trigger)
    {
        var t = new AITransition(from, to) { Trigger = trigger };
        _transitions.Add(t);
        return t;
    }

    /// <summary>Enters the initial state (call once from OnStart; its Enter runs immediately).</summary>
    public void Start(string initialState) => SwitchTo(initialState);

    /// <summary>Forces a transition regardless of guards or triggers (Exit/Enter run immediately).</summary>
    public void SetState(string name) => SwitchTo(name);

    /// <summary>True while the named state is current.</summary>
    public bool IsIn(string name) => _current is not null && _current.Name == name;

    /// <summary>Fires a trigger: the first matching trigger transition (current state's, then global) is taken immediately.</summary>
    public void Fire(string trigger)
    {
        if (_current is null)
            return;
        for (int i = 0; i < _transitions.Count; i++)
        {
            AITransition t = _transitions[i];
            if (t.Trigger != trigger)
                continue;
            if (t.From is not null && t.From != _current.Name)
                continue;
            if (!t.Ready(_timeInState) || !PassesGuard(t))
                continue;
            SwitchTo(t.To);
            return;
        }
    }

    // One engine tick: advance time, run the state's Update, then take the first eligible automatic transition.
    internal void Tick(float dt)
    {
        JustChanged = false;
        if (_current is null)
            return;

        _timeInState += dt;
        try { _current.Update?.Invoke(dt); }
        catch (Exception e) { AIRegistry.Warn(_owner, e); }

        for (int i = 0; i < _transitions.Count; i++)
        {
            AITransition t = _transitions[i];
            if (t.Trigger is not null)
                continue; // explicit transitions only fire via Fire()
            if (t.From is not null && t.From != _current.Name)
                continue;
            if (!t.Ready(_timeInState) || !PassesGuard(t))
                continue;
            SwitchTo(t.To);
            return;
        }
    }

    private bool PassesGuard(AITransition t)
    {
        if (t.Guard is null)
            return true;
        try { return t.Guard(); }
        catch (Exception e) { AIRegistry.Warn(_owner, e); return false; }
    }

    private void SwitchTo(string name)
    {
        if (!_states.TryGetValue(name, out AIState? next))
        {
            AIRegistry.WarnUnknownState(_owner, name);
            return;
        }
        if (_current == next)
            return;

        try { _current?.Exit?.Invoke(); }
        catch (Exception e) { AIRegistry.Warn(_owner, e); }

        _previous = _current;
        _current = next;
        _timeInState = 0.0f;
        JustChanged = true;

        try { next.Enter?.Invoke(); }
        catch (Exception e) { AIRegistry.Warn(_owner, e); }
    }
}

// Live set of state machines, keyed by owning entity for ticking and editor debug queries.
internal static class AIRegistry
{
    private static readonly Dictionary<uint, List<StateMachine>> s_ByEntity = new();
    // Iteration snapshot: scripts may create machines from inside state callbacks (Enter/Update), which would mutate the dictionary mid-tick.
    private static readonly List<StateMachine> s_Snapshot = new();
    private static bool s_Dirty = true;

    internal static void Register(Entity owner, StateMachine machine)
    {
        if (!s_ByEntity.TryGetValue(owner.Id, out List<StateMachine>? list))
        {
            list = new List<StateMachine>();
            s_ByEntity[owner.Id] = list;
        }
        if (!list.Contains(machine))
            list.Add(machine);
        s_Dirty = true;
    }

    internal static void UnregisterEntity(uint entityId)
    {
        if (s_ByEntity.Remove(entityId))
            s_Dirty = true;
    }

    internal static void Pump(float dt)
    {
        if (s_Dirty)
        {
            s_Snapshot.Clear();
            foreach (List<StateMachine> list in s_ByEntity.Values)
                s_Snapshot.AddRange(list);
            s_Dirty = false;
        }
        for (int i = 0; i < s_Snapshot.Count; i++)
            s_Snapshot[i].Tick(dt);
    }

    internal static void Clear()
    {
        s_ByEntity.Clear();
        s_Snapshot.Clear();
        s_Dirty = true;
    }

    // The first machine registered on the entity wins the display slot.
    internal static bool TryGetDebugState(uint entityId, out string state, out float time)
    {
        if (s_ByEntity.TryGetValue(entityId, out List<StateMachine>? list) && list.Count > 0 && list[0].IsRunning)
        {
            state = list[0].CurrentState;
            time = list[0].TimeInState;
            return state.Length > 0;
        }
        state = "";
        time = 0.0f;
        return false;
    }

    internal static void Warn(Entity owner, Exception e)
        => Scripting.Internal.ScriptRuntime.LogWarn($"AI on '{owner}': {e.Message}");
    internal static void WarnMessage(Entity owner, string message)
        => Scripting.Internal.ScriptRuntime.LogWarn($"AI on '{owner}': {message}");
    internal static void WarnUnknownState(Entity owner, string name)
        => Scripting.Internal.ScriptRuntime.LogWarn($"AI on '{owner}': no state named '{name}'");

    // Fills the editor's AI debugger rows: id @ +0, time-in-state @ +4, NUL-terminated state name @ +8.
    internal static unsafe int BuildReport(byte* rows, int maxRows, int rowStride)
    {
        int n = 0;
        foreach (var kv in s_ByEntity)
        {
            if (n >= maxRows)
                break;
            List<StateMachine> list = kv.Value;
            if (list.Count == 0 || !list[0].IsRunning)
                continue;
            byte* row = rows + n * rowStride;
            *(uint*)row = kv.Key;
            *(float*)(row + 4) = list[0].TimeInState;
            string state = list[0].CurrentState;
            int copy = Math.Min(state.Length, 63);
            byte* dst = row + 8;
            for (int i = 0; i < copy; i++)
                dst[i] = (byte)state[i];
            dst[copy] = 0;
            n++;
        }
        return n;
    }
}
