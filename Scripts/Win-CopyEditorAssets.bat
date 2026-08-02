@echo off
pushd %~dp0\..\

echo Copying Waffle assets, projects, and resources to binary directories...

if not "%1"=="" (
    call :CopyAssets "%1"
) else (
    for %%D in (
        "bin\Debug-windows-x86_64\Waffle-Editor"
        "bin\Release-windows-x86_64\Waffle-Editor"
        "bin\Dist-windows-x86_64\Waffle-Editor"
        "bin\Debug-windows-x86_64\Waffle-Runtime"
        "bin\Release-windows-x86_64\Waffle-Runtime"
        "bin\Dist-windows-x86_64\Waffle-Runtime"
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

if exist "Waffle-Editor\Assets" (
    xcopy /E /I /Y "Waffle-Editor\Assets" "%DIR%\Assets" >nul
)
if exist "Assets" (
    xcopy /E /I /Y "Assets" "%DIR%\Assets" >nul
)

if exist "Waffle-Editor\Projects" (
    xcopy /E /I /Y "Waffle-Editor\Projects" "%DIR%\Projects" >nul
)
if exist "Projects" (
    xcopy /E /I /Y "Projects" "%DIR%\Projects" >nul
)

if exist "Waffle-Editor\Resources" (
    xcopy /E /I /Y "Waffle-Editor\Resources" "%DIR%\Resources" >nul
)

if exist "Waffle-Editor\imgui.ini" (
    copy /Y "Waffle-Editor\imgui.ini" "%DIR%\imgui.ini" >nul
)
goto :eof
