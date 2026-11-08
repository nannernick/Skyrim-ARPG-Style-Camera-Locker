# Skyrim ARPG Style Camera Locker + Install Guide

SKSE plugin that pins Skyrim's third-person camera to a fixed angle for isometric/ARPG play, plus the mod list to turn Skyrim into a Path of Exile/Diablo-style ARPG.

## Install guide
Tested on Skyrim **1.7.104**. Every `.dll` mod needs a build for your game version; ESP-only mods work on any.

1. **Base:** [SKSE](https://skse.silverlock.org/), [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444), [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604), [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000), Pandora Behaviour Engine
2. **Camera:** [3PCO](https://www.nexusmods.com/skyrimspecialedition/mods/18515), [True Directional Movement](https://www.nexusmods.com/skyrimspecialedition/mods/51614), [Some Collision Camera](https://www.nexusmods.com/skyrimspecialedition/mods/57864), [Motion Sensitive Fix](https://www.nexusmods.com/skyrimspecialedition/mods/35833), then this plugin ([**Releases**](https://github.com/nannernick/Skyrim-aRPG-Style-Camera-Lock/releases/latest))
3. **Combat:** [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347), [TK Dodge RE](https://www.nexusmods.com/skyrimspecialedition/mods/56956)
4. **Skills and spells:** Ordinator, Apocalypse, [Summermyst](https://www.nexusmods.com/skyrimspecialedition/mods/6285)
5. **Packs:** Genesis
6. **Loot:** [DiabloRim](https://www.nexusmods.com/skyrimspecialedition/mods/190634), [Loot Spillage NG](https://www.nexusmods.com/skyrimspecialedition/mods/108394), Grid Inventory, [Socketing](https://www.nexusmods.com/skyrimspecialedition/mods/195740)
7. **Run Pandora** (TDM, TK Dodge, Precision) and launch through SKSE.
8. **Set up 3PCO and TDM** with the [Isometric Scrolls Skyrim guide](https://www.nexusmods.com/skyrimspecialedition/mods/89358) (outdated, written for 1.6.640). In TDM, turn off *Reset Camera with Target Lock Button*.
9. **Tweaks:** difficulty Novice/Apprentice for faster kills; console `player.setav speedmult 130` for faster movement.

If the game crashes on load, check `My Games\Skyrim Special Edition\SKSE\skse64.log` for a plugin that failed to load.

## Camera Locker
**F10** toggles the lock, **F9** hides ceilings indoors.

**Settings (`IsoCamLock.ini`)**
- `fPitch`: degrees looking down (default 30)
- `bLockYaw` / `bLockPitch`: lock each axis (yaw is free by default)
- `fYaw`: compass direction in degrees, if yaw is locked (default 45)
- `fCeilingClearance`: how high above your feet counts as ceiling (default 200). Doesn't work in caves whose floor and ceiling are one mesh.
- `iToggleKey` / `iCeilingKey`: scan codes, 0x44 = F10, 0x43 = F9
- `bDebugLog`: log camera values to `My Games\Skyrim Special Edition\SKSE\IsoCamLock.log`

## Angles from other ARPGs
Hover a value to see the math.

| Game | `fPitch` |
|---|---|
| Diablo II | [30](#angles-from-other-arpgs "2:1 floor tiles → arcsin(1 ÷ 2) = 30°") |
| Path of Exile | [50](#angles-from-other-arpgs "At 1920×1080: ring radii 210 × 160 px → arcsin(160 ÷ 210) = 49.6°") |
| Path of Exile 2 | [30](#angles-from-other-arpgs "Ground ring ~700 × 350 px → arcsin(350 ÷ 700) = 30°") |
| Diablo IV | [—](#angles-from-other-arpgs "Not measured yet") |

![Path of Exile ring measured at 1920×1080](images/poe1-measurement.jpg)

`fPitch = arcsin(height ÷ width)`

To measure any game: screenshot a circle on the ground (an AoE ring) near the center of the screen and measure its width and height. Also lower 3PCO's FOV (~40–55) and raise zoom, since ARPGs use a narrow lens from far away.

Lock on with TDM before using magic or bows.

## License
GPL-3.0-or-later (links CommonLibSSE-NG).
