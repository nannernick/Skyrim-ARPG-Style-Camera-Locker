#pragma once

// All settings live in Data/SKSE/Plugins/ARPGCamera.ini. The in-game menu edits
// these values directly (they apply live) and Save() writes them back.
namespace Settings
{
	// [Camera] - camera lock (formerly IsoCamLock)
	inline bool          CameraEnabled = true;        // camera lock on at game start
	inline bool          LockYaw = false;             // free mouse rotation by default
	inline bool          LockPitch = true;
	inline float         Yaw = 45.0f;                 // world heading when LockYaw: 0 = north, 90 = east
	inline float         Pitch = 30.0f;               // degrees looking down
	inline int           PitchMode = 0;               // 0 = write player pitch, 1 = write camera freeRotation.y
	inline bool          InvertPitch = false;
	inline bool          PauseOnTargetLock = false;   // let TDM hold the camera while target lock is active
	inline bool          FixInteriorRooms = true;     // draw the player's room even if the camera is outside it
	inline float         PitchStep = 5.0f;
	inline std::uint32_t CameraToggleKey = 0x44;      // F10
	inline std::uint32_t PitchUpKey = 0x49;           // Page Up
	inline std::uint32_t PitchDownKey = 0x51;         // Page Down
	inline std::uint32_t ReloadKey = 0x42;            // F8
	inline bool          CameraDebugLog = false;

	// [Player] - applied on save load / new game. 0 = leave the game's value alone.
	inline float CarryWeight = 0.0f;
	inline float SpeedMult = 0.0f;

	// [GameSettings] - engine settings (setgs names), re-applied every launch.
	inline std::vector<std::pair<std::string, std::string>> GameSettings;

	// [General] - cutaway
	inline bool Enabled = true;
	inline bool InteriorOnly = true;
	inline int  Mode = 1;                             // 1 = horizontal cut (clip plane), 0 = hide whole meshes (legacy)

	// [Cut] - Mode 1
	inline float         CutHeight = 220.0f;          // cut height above the floor at the player
	inline float         CutRadius = 2500.0f;         // selective cut only
	inline bool          CutAllGeometry = true;       // every main-camera mesh uses the cut projection (consistent depth)
	inline float         CutSlope = 1.0f;             // how fast the cut rises behind you (1 = 45 deg, 0 = flat)
	inline float         ReanchorDistance = 150.0f;   // cut height only moves if you stand this far above/below it...
	inline float         ReanchorDelay = 1.0f;        // ...for this many seconds, on the ground
	inline float         HeightSmoothing = 0.6f;      // seconds every height change eases in over (0 = instant)
	inline float         MaxHeightSpeed = 800.0f;     // units/second cap on any height change
	inline bool          AutoLift = true;             // style: true = cut only when the view is blocked, false = always cut
	inline std::uint32_t StyleKey = 0x43;             // F9: switch style in game
	inline float         ClearDelay = 0.75f;          // auto style: seconds of clear view before the cut rises
	inline float         LiftMax = 600.0f;            // auto style: how far it rises; at the top the cut switches off

	// [Cells] - per-interior overrides, keyed by lowercase editor ID
	struct CellOverride
	{
		bool  off{ false };
		float height{ 0.0f };
	};
	inline std::unordered_map<std::string, CellOverride> CellOverrides;
	const CellOverride* FindCellOverride(const char* a_editorID);

	// [Sweep] / [Hide] - Mode 0 (legacy per-mesh hiding)
	inline float HeadHeight = 120.0f;
	inline float CorridorRadius = 400.0f;
	inline float SweepInterval = 0.05f;
	inline float MinCameraDistance = 50.0f;
	inline float MinHeight = 200.0f;
	inline float CeilingHeight = 350.0f;
	inline float BehindMargin = 100.0f;
	inline float MaxShapeRadius = 700.0f;
	inline float Linger = 1.0f;
	inline float Hysteresis = 75.0f;

	// [Background]
	inline bool BlackFog = true;                      // black interior fog while inside
	inline bool HideSky = true;                       // hide the sky dome while inside

	// [Debug]
	inline bool DebugLog = false;

	void Load();
	bool Save();  // writes every setting back to the INI (comments are regenerated)
}
