# Skyrim ARPG Style Camera Mod

SKSE plugin that turns Skyrim's camera into an ARPG camera. It locks the camera at a fixed top-down angle, and indoors it splits away the walls and roofs between the camera and you. Tested on Skyrim **1.7.104**.

## Suggested modpack
- **Base:** [SKSE](https://skse.silverlock.org/), [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444), [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604), [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000), Pandora Behaviour Engine
- **Camera:** this mod, [3PCO](https://www.nexusmods.com/skyrimspecialedition/mods/18515), [True Directional Movement](https://www.nexusmods.com/skyrimspecialedition/mods/51614), [Some Collision Camera](https://www.nexusmods.com/skyrimspecialedition/mods/57864), [Motion Sensitive Fix](https://www.nexusmods.com/skyrimspecialedition/mods/35833)
- **Combat:** [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347)
- **Skills and spells:** Ordinator, Apocalypse, [Summermyst](https://www.nexusmods.com/skyrimspecialedition/mods/6285)
- **Loot:** [Grid Inventory - DiabloRim](https://www.nexusmods.com/skyrimspecialedition/mods/190634), [Socketing](https://www.nexusmods.com/skyrimspecialedition/mods/195740)
- **Optional:** [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352), for the in-game settings menu

Run Pandora, then launch with `skse64_loader.exe`.

## Camera mod settings required
**3PCO:** FOV 90, camera speed 0, min/max zoom 600. Every offset and zoom 0 in Sheathed, Melee, Ranged and Sneak. Auto 3rd/1st person, crosshair, Ranged Cam and Magic is Ranged all off. The four Sneak toggles off.

**True Directional Movement:** directional movement on, Auto Adjust Camera off, target lock pitch/yaw adjust speed at minimum. Reset Camera with Target Lock on, Reset Camera Pitch off. Lock on before using magic or bows.

## Camera locker
Locks the camera's pitch (and optionally yaw) every frame. Indoors, it keeps your room drawn when the camera sits above the ceiling.
- **F10:** on/off
- **Page Up / Page Down:** more top-down / flatter
- Pitch 30 = Path of Exile 2 / Diablo II, 50 = Path of Exile

![Path of Exile ring measured at 1920×1080](images/poe1-measurement.jpg)

Pitch = arcsin(height ÷ width) of a ground circle (an AoE ring) near the center of the screen.

## Camera splitter
Indoors, everything above a cut line is removed by the GPU. Walls end cleanly and roofs vanish, while floors, furniture and characters stay. The background goes black.
- **F9:** *Auto* (split only when walls block the view) or *Always* (Diablo look)
- The cut stays at one height per floor. Jumps don't move it, and changes ease in.
- The cut rises behind you, so far walls stay full height.
- Idea from machinegod's *Skyrim Camera Disocclusion System*. This is a separate implementation.

## Settings
- **In game:** with SKSE Menu Framework, open its menu (F1) and go to **Skyrim ARPG Style Camera**. Changes apply live. Click **Save to INI** to keep them.
- **INI:** `Data/SKSE/Plugins/SkyrimARPGCamera.ini`. Every setting is commented. **F8** reloads it in game. `[Cells]` sets a cut height, or `off`, for specific interiors.
