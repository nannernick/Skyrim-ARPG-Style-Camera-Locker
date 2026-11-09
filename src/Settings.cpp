#include "Settings.h"

namespace
{
	constexpr const char* kIniPath = "Data\\SKSE\\Plugins\\SkyrimARPGCamera.ini";

	using IniMap = std::unordered_map<std::string, std::string>;

	std::string Trim(const std::string& a_s)
	{
		const auto first = a_s.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = a_s.find_last_not_of(" \t\r\n");
		return a_s.substr(first, last - first + 1);
	}

	std::string Lower(std::string a_s)
	{
		std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_s;
	}

	// Flattens the INI into "Section.Key" -> value. [GameSettings] is kept in order separately.
	IniMap Parse(std::vector<std::pair<std::string, std::string>>& a_gameSettings)
	{
		IniMap        out;
		std::ifstream in(kIniPath);
		if (!in.is_open()) {
			logs::warn("settings: {} not found, using defaults", kIniPath);
			return out;
		}
		std::string section;
		std::string line;
		while (std::getline(in, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			if (line.front() == '[' && line.back() == ']') {
				section = Trim(line.substr(1, line.size() - 2));
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(line.substr(0, eq));
			auto       value = Trim(line.substr(eq + 1));
			if (const auto semi = value.find(';'); semi != std::string::npos) {
				value = Trim(value.substr(0, semi));
			}
			if (section == "GameSettings") {
				a_gameSettings.emplace_back(key, value);
			} else {
				out[section + "." + key] = value;
			}
		}
		return out;
	}

	float GetFloat(const IniMap& a_ini, const char* a_key, float a_default, float a_min, float a_max)
	{
		const auto it = a_ini.find(a_key);
		if (it == a_ini.end()) {
			return a_default;
		}
		char*       end = nullptr;
		const float v = std::strtof(it->second.c_str(), &end);
		if (end == it->second.c_str()) {
			logs::warn("settings: {} = '{}' is not a number, using {}", a_key, it->second, a_default);
			return a_default;
		}
		return std::clamp(v, a_min, a_max);
	}

	bool GetBool(const IniMap& a_ini, const char* a_key, bool a_default)
	{
		return GetFloat(a_ini, a_key, a_default ? 1.0f : 0.0f, 0.0f, 1.0f) > 0.5f;
	}

	int GetInt(const IniMap& a_ini, const char* a_key, int a_default, int a_min, int a_max)
	{
		return static_cast<int>(std::lround(GetFloat(a_ini, a_key, static_cast<float>(a_default), static_cast<float>(a_min), static_cast<float>(a_max))));
	}

	// Scan codes may be written in hex (0x44) or decimal (68).
	std::uint32_t GetKey(const IniMap& a_ini, const char* a_key, std::uint32_t a_default)
	{
		const auto it = a_ini.find(a_key);
		if (it == a_ini.end()) {
			return a_default;
		}
		char*               end = nullptr;
		const unsigned long v = std::strtoul(it->second.c_str(), &end, 0);
		if (end == it->second.c_str() || v > 0xFF) {
			logs::warn("settings: {} = '{}' is not a key code, using {:#x}", a_key, it->second, a_default);
			return a_default;
		}
		return static_cast<std::uint32_t>(v);
	}
}

namespace Settings
{
	void Load()
	{
		std::vector<std::pair<std::string, std::string>> gameSettings;
		const auto                                       ini = Parse(gameSettings);
		GameSettings = std::move(gameSettings);

		CameraEnabled = GetBool(ini, "Camera.Enabled", CameraEnabled);
		LockYaw = GetBool(ini, "Camera.LockYaw", LockYaw);
		LockPitch = GetBool(ini, "Camera.LockPitch", LockPitch);
		Yaw = GetFloat(ini, "Camera.Yaw", Yaw, -360.0f, 360.0f);
		Pitch = GetFloat(ini, "Camera.Pitch", Pitch, 5.0f, 85.0f);
		PitchMode = GetInt(ini, "Camera.PitchMode", PitchMode, 0, 1);
		InvertPitch = GetBool(ini, "Camera.InvertPitch", InvertPitch);
		PauseOnTargetLock = GetBool(ini, "Camera.PauseOnTargetLock", PauseOnTargetLock);
		FixInteriorRooms = GetBool(ini, "Camera.FixInteriorRooms", FixInteriorRooms);
		PitchStep = GetFloat(ini, "Camera.PitchStep", PitchStep, 0.5f, 45.0f);
		CameraToggleKey = GetKey(ini, "Camera.ToggleKey", CameraToggleKey);
		PitchUpKey = GetKey(ini, "Camera.PitchUpKey", PitchUpKey);
		PitchDownKey = GetKey(ini, "Camera.PitchDownKey", PitchDownKey);
		ReloadKey = GetKey(ini, "Camera.ReloadKey", ReloadKey);
		CameraDebugLog = GetBool(ini, "Camera.DebugLog", CameraDebugLog);

		CarryWeight = GetFloat(ini, "Player.CarryWeight", CarryWeight, 0.0f, 1000000.0f);
		SpeedMult = GetFloat(ini, "Player.SpeedMult", SpeedMult, 0.0f, 1000.0f);

		Enabled = GetBool(ini, "General.Enabled", Enabled);
		InteriorOnly = GetBool(ini, "General.InteriorOnly", InteriorOnly);
		Mode = GetInt(ini, "General.Mode", Mode, 0, 1);

		CutHeight = GetFloat(ini, "Cut.Height", CutHeight, 0.0f, 2000.0f);
		CutRadius = GetFloat(ini, "Cut.Radius", CutRadius, 0.0f, 100000.0f);
		CutAllGeometry = GetBool(ini, "Cut.AllGeometry", CutAllGeometry);
		CutSlope = GetFloat(ini, "Cut.Slope", CutSlope, 0.0f, 20.0f);
		ReanchorDistance = GetFloat(ini, "Cut.ReanchorDistance", ReanchorDistance, 0.0f, 100000.0f);
		ReanchorDelay = GetFloat(ini, "Cut.ReanchorDelay", ReanchorDelay, 0.0f, 30.0f);
		HeightSmoothing = GetFloat(ini, "Cut.HeightSmoothing", HeightSmoothing, 0.0f, 30.0f);
		MaxHeightSpeed = GetFloat(ini, "Cut.MaxHeightSpeed", MaxHeightSpeed, 1.0f, 100000.0f);
		AutoLift = GetBool(ini, "Cut.AutoLift", AutoLift);
		StyleKey = GetKey(ini, "Cut.StyleKey", StyleKey);
		ClearDelay = GetFloat(ini, "Cut.ClearDelay", ClearDelay, 0.0f, 30.0f);
		LiftMax = GetFloat(ini, "Cut.LiftMax", LiftMax, 0.0f, 100000.0f);

		CellOverrides.clear();
		for (const auto& [key, value] : ini) {
			if (key.rfind("Cells.", 0) != 0 || key.size() <= 6) {
				continue;
			}
			CellOverride entry;
			if (Lower(value) == "off") {
				entry.off = true;
			} else {
				char* end = nullptr;
				entry.height = std::strtof(value.c_str(), &end);
				if (end == value.c_str()) {
					logs::warn("settings: [Cells] {} = '{}' is not a number or 'off', ignored", key.substr(6), value);
					continue;
				}
			}
			CellOverrides[Lower(key.substr(6))] = entry;
		}

		HeadHeight = GetFloat(ini, "Sweep.HeadHeight", HeadHeight, 0.0f, 500.0f);
		CorridorRadius = GetFloat(ini, "Sweep.CorridorRadius", CorridorRadius, 0.0f, 5000.0f);
		SweepInterval = GetFloat(ini, "Sweep.Interval", SweepInterval, 0.0f, 1.0f);
		MinCameraDistance = GetFloat(ini, "Sweep.MinCameraDistance", MinCameraDistance, 0.0f, 1000.0f);
		MinHeight = GetFloat(ini, "Hide.MinHeight", MinHeight, -500.0f, 2000.0f);
		CeilingHeight = GetFloat(ini, "Hide.CeilingHeight", CeilingHeight, -500.0f, 5000.0f);
		BehindMargin = GetFloat(ini, "Hide.BehindMargin", BehindMargin, -2000.0f, 5000.0f);
		MaxShapeRadius = GetFloat(ini, "Hide.MaxShapeRadius", MaxShapeRadius, 0.0f, 100000.0f);
		Linger = GetFloat(ini, "Hide.Linger", Linger, 0.0f, 10.0f);
		Hysteresis = GetFloat(ini, "Hide.Hysteresis", Hysteresis, 0.0f, 1000.0f);

		BlackFog = GetBool(ini, "Background.BlackFog", BlackFog);
		HideSky = GetBool(ini, "Background.HideSky", HideSky);
		DebugLog = GetBool(ini, "Debug.Log", DebugLog);

		logs::info("settings: camera enabled={} pitch={:.0f} lockPitch={} lockYaw={} yaw={:.0f} fixRooms={} keys toggle={:#x} pitch+={:#x} pitch-={:#x} reload={:#x}",
			CameraEnabled, Pitch, LockPitch, LockYaw, Yaw, FixInteriorRooms, CameraToggleKey, PitchUpKey, PitchDownKey, ReloadKey);
		logs::info("settings: player carry={:.0f} speed={:.0f} gameSettings={}", CarryWeight, SpeedMult, GameSettings.size());
		logs::info("settings: cutaway enabled={} interiorOnly={} mode={} height={:.0f} slope={:.2f} allGeometry={} autoLift={} styleKey={:#x}",
			Enabled, InteriorOnly, Mode, CutHeight, CutSlope, CutAllGeometry, AutoLift, StyleKey);
		logs::info("settings: reanchor={:.0f}/{:.2f}s smoothing={:.2f}s maxSpeed={:.0f} clearDelay={:.2f} liftMax={:.0f} cells={} blackFog={} hideSky={} log={}",
			ReanchorDistance, ReanchorDelay, HeightSmoothing, MaxHeightSpeed, ClearDelay, LiftMax, CellOverrides.size(), BlackFog, HideSky, DebugLog);
	}

	bool Save()
	{
		std::ofstream out(kIniPath, std::ios::trunc);
		if (!out.is_open()) {
			logs::error("settings: can't write {}", kIniPath);
			return false;
		}
		const auto b = [](bool a_v) { return a_v ? 1 : 0; };

		out << "; Skyrim ARPG Style Camera Mod - settings. Editable here or in game (SKSE Menu Framework > Skyrim ARPG Style Camera).\n"
			<< "; Distances are Skyrim units (a human is roughly 128 tall). Key codes are DirectX scan codes.\n\n";

		out << "[Camera]\n"
			<< "; Camera lock on at game start (ToggleKey flips it in game).\n"
			<< std::format("Enabled = {}\n", b(CameraEnabled))
			<< "; Pitch = degrees looking down (30 = Path of Exile 2). Yaw only applies with LockYaw.\n"
			<< std::format("LockPitch = {}\nPitch = {}\nLockYaw = {}\nYaw = {}\n", b(LockPitch), Pitch, b(LockYaw), Yaw)
			<< "; Leave at 0 unless the camera looks up instead of down.\n"
			<< std::format("PitchMode = {}\nInvertPitch = {}\n", PitchMode, b(InvertPitch))
			<< "; Let True Directional Movement hold the camera while target lock is on.\n"
			<< std::format("PauseOnTargetLock = {}\n", b(PauseOnTargetLock))
			<< "; Indoors, draw the room you're in even when the camera is above the ceiling.\n"
			<< std::format("FixInteriorRooms = {}\n", b(FixInteriorRooms))
			<< "; Keys: 0x44 = F10, 0x49 = Page Up, 0x51 = Page Down, 0x42 = F8. 0 = no key.\n"
			<< std::format("ToggleKey = {:#x}\nPitchUpKey = {:#x}\nPitchDownKey = {:#x}\nPitchStep = {}\nReloadKey = {:#x}\n",
				   CameraToggleKey, PitchUpKey, PitchDownKey, PitchStep, ReloadKey)
			<< std::format("DebugLog = {}\n\n", b(CameraDebugLog));

		out << "[Player]\n"
			<< "; Applied whenever a save loads. 0 = leave the game's value alone. SpeedMult 100 = vanilla.\n"
			<< std::format("CarryWeight = {}\nSpeedMult = {}\n\n", CarryWeight, SpeedMult);

		out << "[GameSettings]\n"
			<< "; Engine settings (the names `setgs` uses), re-applied every launch.\n";
		for (const auto& [name, value] : GameSettings) {
			out << name << " = " << value << '\n';
		}
		out << '\n';

		out << "[General]\n"
			<< "; Cutaway master switch, interiors only, and method (1 = cut, 0 = legacy per-mesh hiding).\n"
			<< std::format("Enabled = {}\nInteriorOnly = {}\nMode = {}\n\n", b(Enabled), b(InteriorOnly), Mode);

		out << "[Cut]\n"
			<< "; Cut height above the floor at your position, and how fast it rises behind you\n"
			<< "; (1 = 45 deg, 0 = flat; clamped automatically to keep the floor on screen).\n"
			<< std::format("Height = {}\nSlope = {}\n", CutHeight, CutSlope)
			<< "; 1 = every mesh uses the cut projection so depth stays consistent.\n"
			<< std::format("AllGeometry = {}\nRadius = {}\n", b(CutAllGeometry), CutRadius)
			<< "; Height is anchored per area; it re-anchors only after standing this far\n"
			<< "; above/below it for ReanchorDelay seconds on the ground (jumps never count).\n"
			<< std::format("ReanchorDistance = {}\nReanchorDelay = {}\n", ReanchorDistance, ReanchorDelay)
			<< "; Every height change eases in over ~HeightSmoothing seconds, capped at MaxHeightSpeed.\n"
			<< std::format("HeightSmoothing = {}\nMaxHeightSpeed = {}\n", HeightSmoothing, MaxHeightSpeed)
			<< "; Style: 1 = cut only when something blocks the view, 0 = always cut (Diablo look).\n"
			<< "; StyleKey switches in game (0x43 = F9).\n"
			<< std::format("AutoLift = {}\nStyleKey = {:#x}\nClearDelay = {}\nLiftMax = {}\n\n", b(AutoLift), StyleKey, ClearDelay, LiftMax);

		out << "[Cells]\n"
			<< "; Per-interior overrides by editor ID (see the log's [cell] lines): a height, or off.\n";
		for (const auto& [name, entry] : CellOverrides) {
			if (entry.off) {
				out << name << " = off\n";
			} else {
				out << std::format("{} = {}\n", name, entry.height);
			}
		}
		out << '\n';

		out << "[Background]\n"
			<< "; Black interior fog and no sky dome while inside (what shows through the cut).\n"
			<< std::format("BlackFog = {}\nHideSky = {}\n\n", b(BlackFog), b(HideSky));

		out << "[Sweep]\n; Mode 0 (legacy) only.\n"
			<< std::format("HeadHeight = {}\nCorridorRadius = {}\nInterval = {}\nMinCameraDistance = {}\n\n",
				   HeadHeight, CorridorRadius, SweepInterval, MinCameraDistance)
			<< "[Hide]\n; Mode 0 (legacy) only.\n"
			<< std::format("MinHeight = {}\nCeilingHeight = {}\nBehindMargin = {}\nMaxShapeRadius = {}\nLinger = {}\nHysteresis = {}\n\n",
				   MinHeight, CeilingHeight, BehindMargin, MaxShapeRadius, Linger, Hysteresis);

		out << "[Debug]\n"
			<< std::format("Log = {}\n", b(DebugLog));

		logs::info("settings: saved to {}", kIniPath);
		return out.good();
	}

	const CellOverride* FindCellOverride(const char* a_editorID)
	{
		if (!a_editorID || !a_editorID[0]) {
			return nullptr;
		}
		const auto it = CellOverrides.find(Lower(a_editorID));
		return it != CellOverrides.end() ? &it->second : nullptr;
	}
}
