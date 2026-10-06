"""Waffle engine test runner.

Usage:
    python Scripts/RunTests.py [--no-build]

Runs the full test suite:
  1. Builds the solution (skip with --no-build).
  2. Runs the C++ unit tests (bin/.../WaffleTests.exe).
  3. Runs the in-engine test project (Tests/TestProject) in the player and asserts
     on the log: every managed test marker, lifecycle coverage, zero validation
     errors, zero malformed entities, clean script compilation.

Exits non-zero if anything fails. Designed to be CI-friendly.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LOG_NAME = "Waffle.log"
TIMEOUT_SECONDS = 120

MSBUILD_CANDIDATES = [
    r"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
    r"C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe",
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
]


def find_msbuild():
    for candidate in MSBUILD_CANDIDATES:
        if Path(candidate).is_file():
            return candidate
    return None


def build():
    msbuild = find_msbuild()
    if not msbuild:
        print("BUILD FAIL: MSBuild not found")
        return False
    print("== Building solution (Debug x64) ==")
    result = subprocess.run(
        [msbuild, str(REPO / "Waffle.slnx"), "-p:Configuration=Debug",
         "-p:Platform=x64", "-m", "-v:m", "-nologo"],
        cwd=REPO, capture_output=True, text=True)
    errors = [line for line in (result.stdout + result.stderr).splitlines()
              if " error " in line]
    for line in errors[:10]:
        print(line)
    if result.returncode != 0 or errors:
        print("BUILD FAIL")
        return False
    print("== Build OK ==")
    return True


def run_unit_tests():
    exe = REPO / "bin" / "Debug-windows-x86_64" / "WaffleTests" / "WaffleTests.exe"
    if not exe.is_file():
        print("UNIT FAIL: WaffleTests.exe not found (build first)")
        return False
    print("== C++ unit tests ==")
    result = subprocess.run([str(exe)], cwd=str(exe.parent), capture_output=True, text=True,
                            timeout=TIMEOUT_SECONDS)
    output = result.stdout + result.stderr
    print(output)
    ok = result.returncode == 0
    if not ok:
        print(f"UNIT FAIL: exit code {result.returncode}")
    return ok


def run_engine_tests():
    ok = True
    for backend in ("Vulkan", "OpenGL"):
        print(f"-- Backend: {backend} --")
        if not run_engine_tests_for_backend(exe := REPO / "bin" / "Debug-windows-x86_64" / "WafflePlayer" / "WafflePlayer.exe", backend):
            ok = False
    return ok


def run_engine_tests_for_backend(exe, backend):
    if not exe.is_file():
        print("ENGINE FAIL: WafflePlayer.exe not found (build first)")
        return False

    run_dir = Path(os.environ.get("TEMP", REPO)) / f"waffle_engine_tests_{backend}"
    if run_dir.exists():
        shutil.rmtree(run_dir, ignore_errors=True)
    shutil.copytree(REPO / "Tests" / "TestProject", run_dir)
    log_path = run_dir / LOG_NAME
    if log_path.exists():
        log_path.unlink()

    env = dict(os.environ)
    env["WAFFLE_BACKEND"] = backend

    process = subprocess.Popen([str(exe)], cwd=str(run_dir), env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.time() + TIMEOUT_SECONDS
    while time.time() < deadline:
        if process.poll() is not None:
            break
        time.sleep(0.5)
    else:
        pass
    if process.poll() is None:
        print("ENGINE FAIL: player did not quit within the timeout - killing")
        subprocess.run(["taskkill", "/IM", "WafflePlayer.exe", "/F"],
                       capture_output=True)
        return False

    if not log_path.exists():
        print("ENGINE FAIL: no Waffle.log written")
        return False
    log = log_path.read_text(encoding="utf-8", errors="replace")

    failures = []

    def expect(cond, message):
        if not cond:
            failures.append(message)

    expect("ENGINE TESTS RESULT: ALL PASS" in log, "managed suite did not report ALL PASS")
    expect("TEST FAIL" not in log, "TEST FAIL marker present")
    expect("TEST LIFECYCLE: OnDestroy ran" in log, "lifecycle OnDestroy marker missing")
    expect("skipping malformed" not in log, "malformed entities while loading")
    if backend == "Vulkan":
        expect("Validation Error" not in log, "Vulkan validation errors")
        expect("non-acquired" not in log, "non-acquired swapchain image use")
    expect("script compilation failed" not in log, "script compilation failed")
    # The chosen backend must actually be the one that initialized.
    expect(f"backend: {backend}" in log.lower() or backend.lower() in log.lower(),
           f"log does not mention backend {backend}")

    import re
    summary = re.search(r"ENGINE TESTS: (\d+) passed, (\d+) failed", log)
    expect(summary is not None, "managed summary line missing")
    if summary:
        passed, failed = int(summary.group(1)), int(summary.group(2))
        expect(failed == 0, f"{failed} managed test failures")
        expect(passed >= 10, f"only {passed} managed tests ran - suite shrank?")

    # A few key feature markers (if the suite grows, these keep flagship features covered).
    for marker in ["TEST PASS: ai: timed transition fires",
                   "TEST PASS: coroutine: nested + WaitForSeconds",
                   "TEST PASS: particles: burst raises alive count",
                   "TEST PASS: physics: velocity roundtrip",
                   "TEST PASS: ui: text set/get"]:
        expect(marker in log, f"feature marker missing: {marker}")

    if failures:
        print(f"ENGINE FAIL ({backend}):")
        for failure in failures:
            print(f"  - {failure}")
        return False
    print(f"== In-engine OK ({backend}): {passed} managed tests passed, log clean ==")
    return True


def main():
    parser = argparse.ArgumentParser(description="Run the Waffle test suite")
    parser.add_argument("--no-build", action="store_true", help="skip the build step")
    args = parser.parse_args()

    print("=== Waffle test suite ===")
    ok = True
    if not args.no_build:
        ok = build()
    if ok:
        ok = run_unit_tests()
    if ok:
        ok = run_engine_tests()

    print("=== RESULT:", "ALL TESTS PASS" if ok else "FAILURES DETECTED", "===")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
