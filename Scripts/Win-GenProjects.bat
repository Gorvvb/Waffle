@echo off
pushd %~dp0\..\
call vendor\premake\bin\premake5.exe vs2026
if %ERRORLEVEL% NEQ 0 (
    echo VS2026 action not supported by this Premake binary, generating for VS2022...
    call vendor\premake\bin\premake5.exe vs2022
)
popd

rem Build the C# scripting assemblies (contract + nothing else) so the
rem editor/runtime can use them right after project generation.
where python >nul 2>nul && python BuildScripting.py

popd
PAUSE