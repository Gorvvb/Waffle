#!/usr/bin/env python3
"""Builds the Waffle C# scripting assemblies - no csproj, no Visual Studio.

Uses the Roslyn C# compiler (csc.dll) that ships inside the .NET SDK directly,
so this works the same on Windows, Linux, and macOS with nothing but a .NET
SDK on PATH:

    dotnet <sdk>/Roslyn/bincore/csc.dll -target:library ...

Outputs to bin/ScriptingRuntime/:
  Waffle.Scripting.dll              - engine contract assembly ("using Waffle;")
  Waffle.Scripting.runtimeconfig.json
  GameScripts/GameScripts.dll       - spike/test scripts (later: editor compiles
                                      project scripts in-process with Roslyn)

Engine developers run this when the contract API changes; game developers
never need it (the editor compiles their scripts itself).
"""

import json
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "bin" / "ScriptingRuntime"

CONTRACT_SOURCES_DIR = REPO / "Scripting" / "src"
TEST_SOURCES_DIR = REPO / "Scripting" / "TestScripts"


def die(message: str) -> None:
    print(f"BuildScripting: ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def find_dotnet() -> str:
    import shutil
    dotnet = shutil.which("dotnet")
    if not dotnet:
        die(".NET SDK not found on PATH (need 'dotnet'). Install from https://dotnet.microsoft.com")
    return dotnet


def find_sdk(dotnet: str) -> tuple[Path, str]:
    """Returns (sdk_root, version) for the highest installed SDK."""
    result = subprocess.run([dotnet, "--list-sdks"], capture_output=True, text=True, check=True)
    best: tuple[int, ...] | None = None
    chosen: tuple[Path, str] | None = None
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line or "[" not in line:
            continue
        version, _, location = line.partition(" [")
        try:
            key = tuple(int(part) for part in version.split("."))
        except ValueError:
            continue
        if best is None or key > best:
            best = key
            chosen = (Path(location.strip("[]")), version)
    if chosen is None:
        die("dotnet --list-sdks returned nothing usable")
    return chosen


def find_runtime_dir(dotnet: str) -> Path:
    """Highest-versioned Microsoft.NETCore.App shared runtime next to the SDK."""
    dotnet_root = find_sdk(dotnet)[0].parent  # <dotnet root>/sdk -> <dotnet root>
    shared = dotnet_root / "shared" / "Microsoft.NETCore.App"
    candidates = [d for d in shared.iterdir() if d.is_dir()] if shared.is_dir() else []
    if not candidates:
        die(f"no Microsoft.NETCore.App runtime found under {shared}")
    return max(candidates, key=lambda d: [int(p) if p.isdigit() else 0 for p in d.name.split(".")])


def compile_library(dotnet: str, refs: list[Path], out_dll: Path, sources: list[Path],
                    extra_refs: list[Path] = ()) -> None:
    sdk_base, sdk_version = find_sdk(dotnet)
    csc = sdk_base / sdk_version / "Roslyn" / "bincore" / "csc.dll"
    if not csc.is_file():
        die(f"Roslyn compiler not found at {csc}")

    out_dll.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        dotnet, str(csc),
        "-nologo",
        "-nostdlib+",
        "-target:library",
        f"-out:{out_dll}",
        f"-pdb:{out_dll.with_suffix('.pdb')}",
        "-debug:portable",
        "-unsafe+",
        "-nullable+",
        "-langversion:latest",
        "-define:TRACE",
    ]
    cmd += [f"-r:{ref}" for ref in list(refs) + list(extra_refs)]
    cmd += [str(src) for src in sources]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        die(f"csc failed for {out_dll.name}")


def write_runtimeconfig() -> None:
    # rollForward lets this resolve any installed 10.x runtime - no pinning.
    config = {
        "runtimeOptions": {
            "tfm": "net10.0",
            "rollForward": "LatestMinor",
            "framework": {
                "name": "Microsoft.NETCore.App",
                "version": "10.0.0",
            },
        }
    }
    (OUT / "Waffle.Scripting.runtimeconfig.json").write_text(
        json.dumps(config, indent=2), encoding="utf-8")


def is_managed_dll(path: Path) -> bool:
    """True when the PE has a non-zero CLR header (data directory 14)."""
    try:
        with path.open("rb") as file:
            data = file.read(0x400)
        if len(data) < 0x40 or data[:2] != b"MZ":
            return False
        pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe_offset:pe_offset + 4] != b"PE\x00\x00":
            return False
        optional_offset = pe_offset + 24
        magic = struct.unpack_from("<H", data, optional_offset)[0]
        if magic == 0x20B:      # PE32+
            dir_offset = optional_offset + 112 + 14 * 8
        elif magic == 0x10B:    # PE32
            dir_offset = optional_offset + 96 + 14 * 8
        else:
            return False
        if dir_offset + 8 > len(data):
            return False
        return struct.unpack_from("<I", data, dir_offset)[0] != 0
    except OSError:
        return False


def find_framework_refs(runtime_dir: Path) -> list[Path]:
    """Compile-time references: System.Private.CoreLib plus every managed
    assembly in the shared runtime (native dlls like coreclr/hostpolicy are
    filtered out). Same set the runtime itself loads as TPA."""
    corelib = runtime_dir / "System.Private.CoreLib.dll"
    if not corelib.is_file():
        die(f"System.Private.CoreLib.dll not found in {runtime_dir}")
    managed = sorted(
        p for p in runtime_dir.glob("*.dll")
        if p.name != "System.Private.CoreLib.dll" and is_managed_dll(p))
    return [corelib] + managed


def main() -> None:
    dotnet = find_dotnet()
    runtime_dir = find_runtime_dir(dotnet)
    framework_refs = find_framework_refs(runtime_dir)

    OUT.mkdir(parents=True, exist_ok=True)

    contract_sources = sorted(CONTRACT_SOURCES_DIR.rglob("*.cs"))
    if not contract_sources:
        die(f"no sources under {CONTRACT_SOURCES_DIR}")

    contract_dll = OUT / "Waffle.Scripting.dll"
    compile_library(dotnet, framework_refs, contract_dll, contract_sources)
    print(f"Built {contract_dll}")

    write_runtimeconfig()
    print(f"Wrote {OUT / 'Waffle.Scripting.runtimeconfig.json'}")

    test_sources = sorted(TEST_SOURCES_DIR.glob("*.cs"))
    if test_sources:
        test_dll = OUT / "GameScripts" / "GameScripts.dll"
        compile_library(dotnet, framework_refs, test_dll, test_sources,
                        extra_refs=[contract_dll])
        print(f"Built {test_dll}")

    print("BuildScripting: done.")


if __name__ == "__main__":
    main()
