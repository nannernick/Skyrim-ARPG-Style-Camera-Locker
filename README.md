# Skyrim ARPG Style Camera Mod

> **This mod is deprecated.** Its features have been merged into [ARPG POV](https://github.com/nannernick/ARPG-POV-Skyrim-mod), which combines the camera system, interior cutaway, click-to-move, loot labels, and silhouette outlines into a single plugin. Install that instead.
>
> The source code for this mod remains here for reference.

---

## Migration

1. **Uninstall this mod** — remove `SkyrimARPGCamera.dll` and `SkyrimARPGCamera.ini` from `Data/SKSE/Plugins/`
2. **Install [ARPG POV](https://github.com/nannernick/ARPG-POV-Skyrim-mod)**
3. If you had custom values in `SkyrimARPGCamera.ini`, copy them into `ARPGPOV.ini`:
   - `[GameSettings]`, `[Player]`, and `[Cells]` sections carry over directly
   - `[General]` (cutaway settings) is now `[Cutaway]`

---

## What this mod did

Locked the third-person camera to an isometric pitch and yaw to give Skyrim an ARPG-style overhead view. Included interior cutaway so roofs hide when you're inside a room.

All of that is now in ARPG POV, alongside point-and-click movement, loot labels, and silhouette outlines.

### Angle math

| Game | Pitch |
|---|---|
| Diablo II | 30 (2:1 floor tiles: arcsin(1 ÷ 2) = 30°) |
| Path of Exile | 50 (ring 210 × 160 px at 1080p: arcsin(160 ÷ 210) = 49.6°) |
| Path of Exile 2 | 30 (ring ~700 × 350 px: arcsin(350 ÷ 700) = 30°) |
| Diablo IV | not measured yet |

![Path of Exile ring measured at 1920×1080](images/poe1-measurement.jpg)

**Pitch = arcsin(height ÷ width)**

To measure any game, screenshot a circle on the ground (an AoE ring) near the center of the screen, then measure its width and height in pixels.

---

## Credits

Camera system by machinegod. Source: [Skyrim Camera Disocclusion System](https://github.com/machinegod/Skyrim-Camera-Disocclusion-System).
