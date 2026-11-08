# IsoCamLock

SKSE plugin that pins the third-person camera to a fixed world yaw and pitch every frame.
Fixes mouse drift and the camera snapping behind the player after doors and fast travel.

## One-time setup

1. **Visual Studio 2022 Community**, with the "Desktop development with C++" workload.
2. **Git** for Windows.
3. **CMake** 3.24+ (VS 2022 includes one; a separate install on PATH also works).
4. **vcpkg**:
   ```
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   ```
   Then add a user environment variable `VCPKG_ROOT` = `C:\vcpkg` and reopen any terminals.

## Get the library

Open **x64 Native Tools Command Prompt for VS 2022** in the IsoCamLock folder:
```
git clone --recursive https://github.com/alandtse/CommonLibSSE-NG extern/CommonLibSSE-NG
```

## Build

Same prompt, same folder:
```
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release
```
The first configure downloads and builds dependencies and can take 10–20 minutes.

Output lands in `dist\SKSE\Plugins\` (IsoCamLock.dll + IsoCamLock.ini).

## Install

Zip the `dist` folder's contents so the zip contains `SKSE\Plugins\...`, then install it in Vortex like any manual download.

## Check it loaded

`Documents\My Games\Skyrim Special Edition\SKSE\IsoCamLock.log` should say "Hooked ThirdPersonState::Update". F10 toggles the lock in game.

## Tuning

Edit `IsoCamLock.ini` in the mod's folder (Vortex → Open in File Manager), redeploy, restart the game.
