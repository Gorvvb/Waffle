@echo off
pushd %~dp0\..\

echo Copying Waffle assets, projects, and resources to binary directories...

if not "%1"=="" (
    call :CopyAssets "%1"
) else (
    for %%D in (
        "bin\Debug-windows-x86_64\WaffleEditor"
        "bin\Release-windows-x86_64\WaffleEditor"
        "bin\Dist-windows-x86_64\WaffleEditor"
        "bin\Debug-windows-x86_64\WafflePlayer"
        "bin\Release-windows-x86_64\WafflePlayer"
        "bin\Dist-windows-x86_64\WafflePlayer"
        "bin\Debug-windows-x86_64\WaffleHub"
        "bin\Release-windows-x86_64\WaffleHub"
        "bin\Dist-windows-x86_64\WaffleHub"
    ) do (
        if exist %%D (
            call :CopyAssets %%D
        )
    )
)

echo Asset copy process completed.
popd
goto :eof

:CopyAssets
set DIR=%~1
echo Deploying to %DIR%...
if not exist "%DIR%" mkdir "%DIR%"

if exist "WaffleEditor\Assets" (
    xcopy /E /I /Y "WaffleEditor\Assets" "%DIR%\Assets" >nul
)
if exist "Assets" (
    xcopy /E /I /Y "Assets" "%DIR%\Assets" >nul
)

if exist "WaffleEditor\Projects" (
    xcopy /E /I /Y "WaffleEditor\Projects" "%DIR%\Projects" >nul
)
if exist "Projects" (
    xcopy /E /I /Y "Projects" "%DIR%\Projects" >nul
)

if exist "WaffleEditor\Resources" (
    xcopy /E /I /Y "WaffleEditor\Resources" "%DIR%\Resources" >nul
)

if exist "WaffleEditor\imgui.ini" (
    copy /Y "WaffleEditor\imgui.ini" "%DIR%\imgui.ini" >nul
)
goto :eof
