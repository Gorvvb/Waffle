@echo off
echo Cleaning Waffle Shader Caches...
pushd %~dp0\..\
if exist "WaffleEditor\Assets\cache" rmdir /s /q "WaffleEditor\Assets\cache"
if exist "WaffleHub\Projects" (
    for /d %%P in ("WaffleHub\Projects\*") do (
        if exist "%%P\Assets\cache" rmdir /s /q "%%P\Assets\cache"
    )
)
popd
echo Shader caches removed successfully.
PAUSE
