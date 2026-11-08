# IsoCamLock

SKSE plugin that pins Skyrim's third-person camera to a fixed angle for isometric/ARPG play. Stops mouse drift and the camera resetting after doors.

**Requires:** SKSE and Address Library for your game version. Don't combine it with the TISS 3PCO camera-lock scripts.

## Install
Download **IsoCamLock-mod** from the latest green run in **Actions**, drag it into Vortex/MO2, and launch through SKSE. **F10** toggles the lock.

## Settings (`IsoCamLock.ini`)
- `fYaw`: compass direction in degrees (default 45)
- `fPitch`: degrees looking down (default 55)
- `bLockYaw` / `bLockPitch`: lock each axis (set 0 to free it)
- `iPitchMode`: try 1 if pitch won't hold
- `bInvertPitch`: set 1 if the camera faces the sky
- `iToggleKey`: scan code, 0x44 = F10
- `bDebugLog`: log camera values to `My Games\Skyrim Special Edition\SKSE\IsoCamLock.log`

Lock on with TDM before using magic or bows.

## License
GPL-3.0-or-later (links CommonLibSSE-NG).
