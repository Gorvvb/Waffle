@echo off
echo Cleaning Waffle Shader Caches...
pushd %~dp0\..\
if exist "Waffle-Editor\Assets\cache" rmdir /s /q "Waffle-Editor\Assets\cache"
if exist "WaffleHub\Projects" (
    for /d %%P in ("WaffleHub\Projects\*") do (
        if exist "%%P\Assets\cache" rmdir /s /q "%%P\Assets\cache"
    )
)
popd
echo Shader caches removed successfully.
PAUSE
