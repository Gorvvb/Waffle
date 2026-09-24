![Waffle Logo](https://raw.githubusercontent.com/Gorvvb/Waffle/main/Waffle-Editor/Resources/Icons/logo.png)
# Waffle

Waffle is a 2D game engine for Windows, built around a Lua-scripted ECS and a docking editor.

## Status

Version 1.0.0

## Requirements

- Windows 10/11 x64
- Git
- Visual Studio 2026
- Vulkan SDK (for the Vulkan backend)

## Features

- 2D batch rendering with post-processing
- OpenGL and Vulkan rendering backends
- Animation and spritesheet systems with a spritesheet and animation editor
- Tilemaps with palette painting and merged colliders
- In-engine UI system (buttons, text, images, progress bars)
- Entity Component System (ECS) and Lua scripting
- Box2D physics with triggers and render interpolation
- Audio, input, and controller support
- Multithreaded job system and event queue
- Integrated editor with scene hierarchy, content browser, and asset tools
- One-click export of standalone games

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

Open the generated project in Visual Studio and build.
`Scripts/PackageRelease.bat` produces the release zip.

## Scripting Quick Look

```lua
Public = { Speed = 5.0 }

function OnUpdate(entity, ts)
    local move = GetAxis("Horizontal")
    local vx, vy = GetLinearVelocity(entity)
    SetLinearVelocity(entity, move * Public.Speed, vy)
end

function OnCollisionBegin(entity, other)
    if GetEntityName(other) == "Death" then
        ChangeScene(GetCurrentSceneIndex() - 1)
    end
end
```

## License

[Apache 2.0 + Commons Clause](LICENSE)
