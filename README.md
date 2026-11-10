# Skyrim ARPG Style Camera Mod

**This mod is deprecated.** Its features have been merged into [ARPG POV](https://github.com/nannernick/ARPG-POV-Skyrim-mod), which combines the camera system, interior cutaway, click-to-move, loot labels, and silhouette outlines into a single plugin. Install that instead.

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

---

## Credits

Camera system by machinegod.
