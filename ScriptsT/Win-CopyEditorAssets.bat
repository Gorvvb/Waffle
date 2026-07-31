@echo off
pushd %~dp0\..\

echo Copying WaffleEditor assets and resources to binary directories...

set TARGETS[0]=bin\Debug-windows-x86_64\Waffle-Editor
set TARGETS[1]=bin\Release-windows-x86_64\Waffle-Editor
set TARGETS[2]=bin\Dist-windows-x86_64\Waffle-Editor

if not "%1"=="" (
    call :CopyAssets "%1"
) else (
    for %%D in (
        "bin\Debug-windows-x86_64\Waffle-Editor"
        "bin\Release-windows-x86_64\Waffle-Editor"
        "bin\Dist-windows-x86_64\Waffle-Editor"
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

if exist "Waffle-Editor\assets" (
    xcopy /E /I /Y "Waffle-Editor\assets" "%DIR%\assets" >nul
)

if exist "Waffle-Editor\Resources" (
    xcopy /E /I /Y "Waffle-Editor\Resources" "%DIR%\Resources" >nul
)

if exist "Waffle-Editor\imgui.ini" (
    copy /Y "Waffle-Editor\imgui.ini" "%DIR%\imgui.ini" >nul
)
goto :eof
