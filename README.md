# Skyrim ARPG Style Camera Locker

SKSE plugin that pins Skyrim's third-person camera to a fixed angle for isometric/ARPG play. Stops mouse drift and the camera resetting after doors.

**Requires:** SKSE and [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444) for your game version.

**Made for ARPG-style setups**, specifically:
- 3PCO – 3rd Person Camera Overhaul: https://www.nexusmods.com/skyrimspecialedition/mods/18515
- True Directional Movement: https://www.nexusmods.com/skyrimspecialedition/mods/51614
- The Isometric Scrolls Skyrim guide, for their settings (outdated, written for 1.6.640): https://www.nexusmods.com/skyrimspecialedition/mods/89358

## Install
Download the latest release from [**Releases**](https://github.com/nannernick/Skyrim-aRPG-Style-Camera-Lock/releases/latest), drag the zip into Vortex/MO2, and launch through SKSE. **F10** toggles the lock.

## Settings (`IsoCamLock.ini`)
- `fYaw`: compass direction in degrees (default 45)
- `fPitch`: degrees looking down (default 55)
- `bLockYaw` / `bLockPitch`: lock each axis (set 0 to free it)
- `iPitchMode`: try 1 if pitch won't hold
- `bInvertPitch`: set 1 if the camera faces the sky
- `iToggleKey`: scan code, 0x44 = F10
- `bDebugLog`: log camera values to `My Games\Skyrim Special Edition\SKSE\IsoCamLock.log`

## Angles from other ARPGs
Hover a value to see the math.

| Game | `fPitch` |
|---|---|
| Diablo II | [30](#angles-from-other-arpgs "2:1 floor tiles → arcsin(1 ÷ 2) = 30°") |
| Path of Exile | [—](#angles-from-other-arpgs "Not measured yet") |
| Path of Exile 2 | [—](#angles-from-other-arpgs "Not measured yet") |
| Diablo IV | [—](#angles-from-other-arpgs "Not measured yet") |

To measure any game: screenshot a circle on the ground (an AoE ring) near the center of the screen. `fPitch` = arcsin(height ÷ width). Also lower 3PCO's FOV (~40–55) and raise zoom, since ARPGs use a narrow lens from far away.

Lock on with TDM before using magic or bows.

## License
GPL-3.0-or-later (links CommonLibSSE-NG).
