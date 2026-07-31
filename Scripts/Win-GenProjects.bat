@echo off
pushd %~dp0\..\
call vendor\premake\bin\premake5.exe vs2026
if %ERRORLEVEL% NEQ 0 (
    echo VS2026 action not supported by this Premake binary, generating for VS2022...
    call vendor\premake\bin\premake5.exe vs2022
)
popd
PAUSE