![Waffle Logo](https://raw.githubusercontent.com/Gorvvb/Waffle/main/Waffle-Editor/Resources/Icons/logo.png)
# Waffle

Waffle is a 2D game engine under active development, targeting Windows.

## Status

The project is in early development. Features are incomplete and the API is subject to change.

## Requirements

- Windows OS
- Git
- Visual Studio 2026

## Features

- 2D game development & batch rendering
- Animation and spritesheet systems
- OpenGL and Vulkan rendering backends
- Multithreaded job system & event queue
- Entity Component System (ECS) & Lua scripting
- Integrated editor with scene hierarchy, content browser, and asset tools

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

## License

[Apache 2.0 + Commons Clause](LICENSE)
