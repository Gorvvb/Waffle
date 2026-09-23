@echo off
setlocal enabledelayedexpansion

echo =========================================================================
echo               Waffle Engine 1.0 Release Packaging Script
echo =========================================================================

pushd %~dp0\..\
set ROOT_DIR=%CD%
set DIST_NAME=WaffleEngine-1.0.0-Windows-x64
set DIST_DIR=%ROOT_DIR%\dist\%DIST_NAME%
set ZIP_FILE=%ROOT_DIR%\dist\%DIST_NAME%.zip

echo.
echo [1/7] Cleaning previous distribution folder...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
if exist "%ZIP_FILE%" del /q "%ZIP_FILE%"
mkdir "%DIST_DIR%"

echo.
echo [2/7] Building Waffle solution in Release configuration (x64)...
call "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Waffle.slnx /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MSBuild Release build failed!
    popd
    PAUSE
    exit /b %ERRORLEVEL%
)

echo.
echo [3/7] Creating component subdirectories...
mkdir "%DIST_DIR%\WaffleHub"
mkdir "%DIST_DIR%\Waffle-Editor"
mkdir "%DIST_DIR%\Waffle-Runtime"

echo.
echo [4/7] Deploying Release executables...
set MISSING=0
if not exist "bin\Release-windows-x86_64\WaffleHub\WaffleHub.exe" set MISSING=1
if not exist "bin\Release-windows-x86_64\Waffle-Editor\Waffle-Editor.exe" set MISSING=1
if not exist "bin\Release-windows-x86_64\Waffle-Runtime\Waffle-Runtime.exe" set MISSING=1
if "%MISSING%"=="1" (
    echo [ERROR] Release executables not found. Build the solution in Release configuration first:
    echo         MSBuild Waffle.slnx /p:Configuration=Release /p:Platform=x64
    popd
    PAUSE
    exit /b 1
)
copy /Y "bin\Release-windows-x86_64\WaffleHub\WaffleHub.exe" "%DIST_DIR%\WaffleHub\WaffleHub.exe" >nul
copy /Y "bin\Release-windows-x86_64\Waffle-Editor\Waffle-Editor.exe" "%DIST_DIR%\Waffle-Editor\Waffle-Editor.exe" >nul
copy /Y "Waffle-Editor\imgui.ini" "%DIST_DIR%\Waffle-Editor\imgui.ini" >nul
copy /Y "bin\Release-windows-x86_64\Waffle-Runtime\Waffle-Runtime.exe" "%DIST_DIR%\Waffle-Runtime\Waffle-Runtime.exe" >nul

echo.
echo [4b] Deploying VC++ runtime DLLs so the tools run on clean machines...
for %%D in (WaffleHub Waffle-Editor Waffle-Runtime) do (
    if exist "%SystemRoot%\System32\vcruntime140.dll"  copy /Y "%SystemRoot%\System32\vcruntime140.dll"  "%DIST_DIR%\%%D\vcruntime140.dll"  >nul
    if exist "%SystemRoot%\System32\vcruntime140_1.dll" copy /Y "%SystemRoot%\System32\vcruntime140_1.dll" "%DIST_DIR%\%%D\vcruntime140_1.dll" >nul
    if exist "%SystemRoot%\System32\msvcp140.dll"      copy /Y "%SystemRoot%\System32\msvcp140.dll"      "%DIST_DIR%\%%D\msvcp140.dll"      >nul
)

echo.
echo [5/7] Deploying Vulkan shaderc_shared.dll for standalone execution...
set SHADERC_COPIED=0
if defined VULKAN_SDK (
    if exist "%VULKAN_SDK%\Bin\shaderc_shared.dll" (
        copy /Y "%VULKAN_SDK%\Bin\shaderc_shared.dll" "%DIST_DIR%\Waffle-Editor\shaderc_shared.dll" >nul
        copy /Y "%VULKAN_SDK%\Bin\shaderc_shared.dll" "%DIST_DIR%\Waffle-Runtime\shaderc_shared.dll" >nul
        copy /Y "%VULKAN_SDK%\Bin\shaderc_shared.dll" "%DIST_DIR%\WaffleHub\shaderc_shared.dll" >nul
        echo [OK] shaderc_shared.dll copied from %%VULKAN_SDK%%
        set SHADERC_COPIED=1
    )
)
if "!SHADERC_COPIED!"=="0" (
    for /d %%V in ("C:\VulkanSDK\*") do (
        if exist "%%V\Bin\shaderc_shared.dll" (
            copy /Y "%%V\Bin\shaderc_shared.dll" "%DIST_DIR%\Waffle-Editor\shaderc_shared.dll" >nul
            copy /Y "%%V\Bin\shaderc_shared.dll" "%DIST_DIR%\Waffle-Runtime\shaderc_shared.dll" >nul
            copy /Y "%%V\Bin\shaderc_shared.dll" "%DIST_DIR%\WaffleHub\shaderc_shared.dll" >nul
            echo [OK] shaderc_shared.dll copied from %%V
        )
    )
)

echo.
echo [6/7] Deploying Assets, Resources, Projects and Waffle Hub Shortcut...
if exist "Waffle-Editor\Assets" (
    xcopy /E /I /Y "Waffle-Editor\Assets" "%DIST_DIR%\Waffle-Editor\Assets" >nul
    xcopy /E /I /Y "Waffle-Editor\Assets" "%DIST_DIR%\Waffle-Runtime\Assets" >nul
    xcopy /E /I /Y "Waffle-Editor\Assets" "%DIST_DIR%\WaffleHub\Assets" >nul
)
if exist "Waffle-Editor\Resources" (
    xcopy /E /I /Y "Waffle-Editor\Resources" "%DIST_DIR%\WaffleHub\Resources" >nul
    xcopy /E /I /Y "Waffle-Editor\Resources" "%DIST_DIR%\Waffle-Editor\Resources" >nul
)
rem The shader cache is regenerated on first launch.
if exist "%DIST_DIR%\Waffle-Editor\Assets\cache" rmdir /s /q "%DIST_DIR%\Waffle-Editor\Assets\cache"
if exist "%DIST_DIR%\Waffle-Runtime\Assets\cache" rmdir /s /q "%DIST_DIR%\Waffle-Runtime\Assets\cache"
rem Local projects (and their manifests) are NOT shipped - the Hub
rem creates a fresh Projects folder on first use.

rem Ship the preconfigured Hub window layout.
if exist "WaffleHub\imgui.ini" (
    copy /Y "WaffleHub\imgui.ini" "%DIST_DIR%\WaffleHub\imgui.ini" >nul
)

rem Launcher batch: starts the Hub with its own folder as working
rem directory, then the console window closes by itself.
(
    echo @echo off
    echo pushd "%%~dp0WaffleHub"
    echo start "" "WaffleHub.exe"
) > "%DIST_DIR%\Run Waffle.bat"

(
    echo =========================================================================
    echo                      Waffle Engine 1.0.0 Release
    echo =========================================================================
    echo.
    echo Quick Start:
    echo 1. Double-click "Run Waffle.bat" in this folder to launch the Hub.
    echo 2. Open an existing project or create a new 2D project.
    echo 3. Waffle Editor will open automatically.
    echo 4. Press Play to test, or click File -^> Export Project to compile a
    echo    standalone game (.exe + .wpack).
    echo.
    echo Minimum Requirements:
    echo - Windows 10/11 x64
    echo - OpenGL 4.5+ or Vulkan 1.2+ Compatible GPU
    echo.
    echo Enjoy building games with Waffle Engine!
) > "%DIST_DIR%\README.txt"

echo.
echo [7/7] Zipping distribution bundle into %ZIP_FILE%...
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"

echo.
echo =========================================================================
echo   SUCCESS! Waffle Engine 1.0 Release package generated at:
echo   %ZIP_FILE%
echo =========================================================================

popd
PAUSE
