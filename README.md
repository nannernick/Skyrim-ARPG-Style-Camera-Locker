# Skyrim ARPG Style Camera Locker + Install Guide

SKSE plugin that pins Skyrim's third-person camera to a fixed angle for isometric/ARPG play, plus the mod list to turn Skyrim into a Path of Exile/Diablo-style ARPG.

## Install guide
Tested on Skyrim **1.7.104**. Every `.dll` mod needs a build for your game version; ESP-only mods work on any.

1. **Base:** [SKSE](https://skse.silverlock.org/), [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444), [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604), [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000), Pandora Behaviour Engine
2. **Camera:** [3PCO](https://www.nexusmods.com/skyrimspecialedition/mods/18515), [True Directional Movement](https://www.nexusmods.com/skyrimspecialedition/mods/51614), [Some Collision Camera](https://www.nexusmods.com/skyrimspecialedition/mods/57864), [Motion Sensitive Fix](https://www.nexusmods.com/skyrimspecialedition/mods/35833), then this plugin ([**Releases**](https://github.com/nannernick/Skyrim-aRPG-Style-Camera-Lock/releases/latest))
3. **Combat:** [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347)
4. **Skills and spells:** Ordinator, Apocalypse, [Summermyst](https://www.nexusmods.com/skyrimspecialedition/mods/6285)
5. **Packs:** Genesis
6. **Loot:** [DiabloRim](https://www.nexusmods.com/skyrimspecialedition/mods/190634), QuickLoot LE, Grid Inventory, [Socketing](https://www.nexusmods.com/skyrimspecialedition/mods/195740)
7. **Run Pandora** (TDM, Precision) and launch through SKSE.
8. **Set up 3PCO and TDM** (below). The [Isometric Scrolls Skyrim guide](https://www.nexusmods.com/skyrimspecialedition/mods/89358) has screenshots of these menus (outdated, written for 1.6.640).
9. **Console** (`~`), once per character: `player.setav carryweight 10000` and `player.setav speedmult 130`, then open and close your inventory.

### 3PCO (MCM)
- **General:** FOV 90, Camera speed 0, Min zoom 600, Max zoom 600
- **Sheathed, Melee, Ranged, Sneak:** every offset and zoom 0; Auto 3rd/1st Person off; crosshair off
- **Ranged:** Enabled Ranged Cam off, Magic is Ranged off
- **Sneak:** the four toggles at the top off

### True Directional Movement (MCM)
- Follow the TISS guide steps 1–20. In short: Directional movement on, Auto Adjust Camera off, Target Lock camera Pitch/Yaw Adjust Speed at the minimum (edit `config.json` min/step to `0.0000001`, drag the slider fully left, tap **D** once).
- *Reset Camera with Target Lock Button* on (lock key with no target snaps the camera forward), *Reset Camera Pitch* off. Set the lock key (e.g. Mouse4).

If the game crashes on load, check `My Games\Skyrim Special Edition\SKSE\skse64.log` for a plugin that failed to load.

## Camera Locker
**F10** toggles the lock, **F9** hides ceilings indoors, **Page Up / Page Down** change the pitch (saved automatically), **F8** reloads the ini.

**Settings (`IsoCamLock.ini`)**
- `fPitch`: degrees looking down (default 30)
- `bLockYaw` / `bLockPitch`: lock each axis (yaw is free by default)
- `fYaw`: compass direction in degrees, if yaw is locked (default 45)
- `bFixInteriorRooms`: indoors, draw the room you're in even when the camera is above the ceiling (default 1)
- `fCeilingClearance`: how high above your feet counts as ceiling (default 200). Doesn't work in caves whose floor and ceiling are one mesh.
- `iToggleKey` / `iCeilingKey`: scan codes, 0x44 = F10, 0x43 = F9
- `[Player]` `fCarryWeight` / `fSpeedMult`: applied on every load (no weight limit, faster movement)
- `[GameSettings]`: any engine setting, applied every launch (armor no longer slows you)
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
GPL-3.0-or-later (links CommonLibSSE-NG). Interior room fix adapted from [Skyrim Camera Disocclusion System](https://github.com/machinegod420/SkyrimCameraDisocclusion).
