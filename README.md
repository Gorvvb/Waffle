
![Waffle Logo](https://raw.githubusercontent.com/Gorvvb/Waffle/main/WaffleEditor/Resources/Icons/logo.png)
# Waffle

Waffle is a 2D game engine for Windows, built around a C#-scripted ECS and a docking editor. Gameplay scripts are plain `.cs` files embedded with the real .NET runtime (CoreCLR) - no Mono, and no project files to maintain: drop a script next to your assets and the engine compiles it.

## Status

Version 2.0.0

## Requirements

- Windows 10/11 x64
- Git
- Visual Studio 2026
- .NET 10 SDK (for C# scripting)
- Vulkan SDK (for the Vulkan backend)

## Features

- 2D batch rendering with post-processing
- OpenGL and Vulkan rendering backends, switchable at launch from the editor's Stats panel (remembered) or project file; games inherit the editor's choice
- Animation and spritesheet systems with a spritesheet and animation editor
- Tilemaps with palette painting and merged colliders
- In-engine UI system (buttons, text, images, progress bars)
- CPU particle systems with inspector tuning and script-side bursts
- Undo/Redo (Ctrl+Z / Ctrl+Y) for entity edits, gizmo moves and component changes
- Background script compilation (play mode no longer blocks on the compiler)
- Entity Component System (ECS) with C# scripting on an embedded .NET runtime
- Box2D physics with triggers and render interpolation
- Audio, input, and controller support
- Multithreaded job system and event queue
- Integrated editor with scene hierarchy, content browser, and asset tools
- One-click export of standalone games (ships a self-contained .NET runtime - players need nothing installed)

## Testing

The engine ships with a test suite — run it after every change:

```cmd
python Scripts/RunTests.py
```

(Use `--no-build` to skip the build step.) The runner exits non-zero on any failure,
so it can drop straight into CI.

- **C++ unit tests** (`Tests/Unit`, `WaffleTests.exe`): serialization round-trips,
  scene copy, undo snapshot restore, UUID guarantees, runtime config I/O, steering math.
- **In-engine tests** (`Tests/TestProject`): a real project the player boots into -
  17 managed assertions covering the script lifecycle, state machines, coroutines,
  steering, blackboards, particles, physics, UI and transforms, plus log assertions
  for zero Vulkan validation errors, zero malformed entities and clean script compilation.
- The render pipeline is exercised by the test scene (sprites, circles, particles,
  tilemap, UI, bloom + vignette post chain) and asserted validation-error-free -
  **on both backends**: the suite runs the whole in-engine pass twice, once on
  Vulkan and once on OpenGL (`WAFFLE_BACKEND` env var selects the backend, so
  backend regressions can never hide behind the other pipeline).

Adding a feature: add a `WTEST` in `Tests/Unit` for engine-internal logic and/or an
`Expect()` in `EngineTests.cs` for anything script-facing, then run the suite.

## Building

Clone the repository:

```bash
git clone https://github.com/Gorvvb/Waffle.git --recursive
```

Then run the following scripts in order:

```cmd
Scripts/Setup.bat
Scripts/Win-GenProjects.bat
```

`Win-GenProjects.bat` also builds the C# scripting assemblies (`Scripts/BuildScripting.py`; rerun it by hand after changing the engine's `Scripting/src` contract API).

Open the generated project in Visual Studio and build.
`Scripts/PackageRelease.bat` produces the release zip.

## Scripting Quick Look

Gameplay scripts are plain `.cs` files under `Assets/Scripts/`. Lifecycle
methods are magic messages - declare them by name, never `override`.
Public fields show up in the inspector (`[Range]`/`[Tooltip]` supported).

```csharp
using System.Numerics;
using Waffle;

public class Player : WaffleBehaviour
{
    [Range(1f, 20f)]
    public float Speed = 5.0f; // inspector-exposed

    void OnStart()
    {
	    Log.Info("Spawned!");
	}

    void OnUpdate(float dt)
    {
        var rb = Entity.Get<Rigidbody2D>();
        float move = Input.GetAxis("Horizontal");
        rb.Velocity = new Vector2(move * Speed, rb.Velocity.Y);

        if (Input.GetKeyDown(KeyCode.Space))
            rb.AddImpulse(new Vector2(0f, 64f));
    }

    void OnCollisionEnter2D(Entity other)
    {
        if (other.Name == "Death")
            SceneManager.Load(SceneManager.CurrentSceneIndex - 1);
    }
}
```

Lifecycle messages:
`OnStart`,
`OnUpdate` / `OnUpdate(float)`, 
`OnDestroy`,
`OnEnable` / `OnDisable`, 
`OnCollisionEnter2D` / `OnCollisionExit2D`,
`OnTriggerEnter2D` / `OnTriggerExit2D`,
`OnDrawGizmos`, and per-button UI
handlers named by `UIButtonComponent`.

## Coroutines

Sequence gameplay over time with `StartCoroutine` - the routine is owned by
the script's entity and stops automatically when the entity is destroyed or
the scene stops.

```csharp
void OnStart()
{
    StartCoroutine(SpawnWaves());
}

IEnumerator SpawnWaves()
{
    for (int wave = 1; wave <= 5; wave++)
    {
        for (int i = 0; i < wave * 3; i++)
        {
            Scene.Instantiate("Enemies/Grunt.prefab", spawnPoint);
            yield return new WaitForSeconds(0.4f);
        }
        Log.Info($"Wave {wave} cleared incoming...");
        yield return new WaitUntil(() => Scene.FindByName("Grunt") == Entity.None);
        yield return new WaitForSeconds(2f);
    }
}
```

Yield instructions: `WaitForSeconds`, `WaitForFrames`,
`WaitUntil` / `WaitWhile`, `yield return null` (one frame), and
`yield return` another `IEnumerator` for nesting. `StopCoroutine` /
`StopAllCoroutines` cancel early.

## Particle Systems

Add a Particle System component (right-click panel > Particle System) and
tune it live in the inspector: spawn rate, lifetime/speed ranges, emission
direction cone, gravity, start/end color and size, sorting. It renders in
the sorted 2D pass (textured or untextured), previews in the editor while
editing, and the inspector's Burst button fires a one-shot 64-particle
burst. Scripts get full control through the `ParticleSystem` component view:

```csharp
ParticleSystem ps = Entity.Get<ParticleSystem>();
ps.Emitting = false;          // pause spawning (rate is preserved)
ps.Burst(64);                 // one-shot burst (impacts, explosions)
int alive = ps.AliveCount;    // live particle count
```

## Undo / Redo

`Ctrl+Z` / `Ctrl+Y` (also `Ctrl+Shift+Z`) covers entity deletion,
duplication and creation, component add/remove, and gizmo moves - plus the
collider edit mode. History is per editing session and resets on scene
load, play mode and prefab editing.

## AI State Machines

Enemies and NPCs get a code-driven state machine: build states and
transitions in `OnStart`, call `Start()`, and the engine ticks the machine
automatically after all script updates - no per-frame bookkeeping. While
playing, the editor inspector shows the live state (`AI: Chase (1.24s)`).

```csharp
using Waffle;

public class Guard : WaffleBehaviour
{
    private StateMachine ai;

    void OnStart()
    {
        ai = new StateMachine(Entity);

        var patrol = ai.AddState("Patrol",
            update: dt =>
            {
                var rb = Entity.Get<Rigidbody2D>();
                rb.Velocity = new System.Numerics.Vector2(2f, rb.Velocity.Y);
            });
        var chase = ai.AddState("Chase");
        var stun = ai.AddState("Stun");

        ai.AddTransition("Patrol", "Chase").When(() => CanSeePlayer());
        ai.AddTransition("Chase", "Patrol").After(4f).When(() => !CanSeePlayer());
        ai.AddGlobalTriggerTransition("Stun", "Hurt");   // ai.Fire("Hurt") from anywhere
        ai.AddTransition("Stun", "Patrol").After(1.5f);  // recover after 1.5s

        ai.Start("Patrol");
    }

    void OnCollisionEnter2D(Entity other)
    {
        if (other.Name == "Player")
            ai.Fire("Hurt");
    }

    void OnUpdate(float dt)
    {
        if (ai.JustChanged)
            Log.Info($"Guard is now {ai.CurrentState}");
    }
}
```

Transition rules: automatic transitions are evaluated every tick and the
first eligible one wins (the current state's own, then global ones);
`.When(guard)` makes a transition conditional, `.After(seconds)` delays it,
and trigger transitions (`AddTriggerTransition` /
`AddGlobalTriggerTransition`) only fire when you call `ai.Fire("...")` -
useful from collision callbacks. `IsIn(name)`, `TimeInState`,
`PreviousState` and `JustChanged` cover reaction logic. Machines stop when
their entity is destroyed or the scene stops.

While playing, `Window > AI Debugger` lists every running machine
(entity, live state, time-in-state) - click a row to select the entity.

### Blackboard and steering

Every machine carries a `Blackboard` - typed shared memory for its states
(target entity, patrol direction, aggro level):

```csharp
ai.Blackboard.Set("home", new Vector2(3f, 4f));
ai.Blackboard.Set("aggro", 0.8f);
Vector2 home = ai.Blackboard.GetVector2("home");
```

`Steering2D` provides classic movement AI that pairs naturally with states -
every helper returns a desired velocity to assign to `Rigidbody2D.Velocity`
(or blend before applying):

```csharp
void OnUpdate(float dt)
{
    var rb = Entity.Get<Rigidbody2D>();
    Vector2 desired = ai.IsIn("Chase")
        ? Steering2D.Pursue(Entity.Position, playerPos, playerVel, maxSpeed)
        : Steering2D.Wander(rb.Velocity, ref wanderAngle, maxSpeed);
    rb.Velocity = desired;
}
```

Behaviors: `Seek`, `Flee`, `Arrive` (decelerating seek), `Pursue` / `Evade`
(velocity-leading), `Wander` (smooth random heading, keep the angle in a
field and pass it by ref), and `Separate` (neighbor repulsion - feed it
`Physics2D.OverlapCircle` positions).

## License

[Apache 2.0 + Commons Clause](LICENSE)