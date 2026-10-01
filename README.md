
![Waffle Logo](https://raw.githubusercontent.com/Gorvvb/Waffle/main/Waffle-Editor/Resources/Icons/logo.png)
# Waffle

Waffle is a 2D game engine for Windows, built around a C#-scripted ECS and a docking editor. Gameplay scripts are plain `.cs` files embedded with the real .NET runtime (CoreCLR) - no Mono, and no project files to maintain: drop a script next to your assets and the engine compiles it.

## Status

Version 1.0.0

## Requirements

- Windows 10/11 x64
- Git
- Visual Studio 2026
- .NET 10 SDK (for C# scripting)
- Vulkan SDK (for the Vulkan backend)

## Features

- 2D batch rendering with post-processing
- OpenGL and Vulkan rendering backends
- Animation and spritesheet systems with a spritesheet and animation editor
- Tilemaps with palette painting and merged colliders
- In-engine UI system (buttons, text, images, progress bars)
- Entity Component System (ECS) with C# scripting on an embedded .NET runtime
- Box2D physics with triggers and render interpolation
- Audio, input, and controller support
- Multithreaded job system and event queue
- Integrated editor with scene hierarchy, content browser, and asset tools
- One-click export of standalone games (ships a self-contained .NET runtime - players need nothing installed)

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

## License

[Apache 2.0 + Commons Clause](LICENSE)