// IsoCamLock - pins Skyrim's third-person camera to a fixed world yaw and pitch.
// Built against CommonLibSSE-NG (alandtse fork) for SE/AE/1.7.x runtimes.

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <numbers>
#include <string>
#include <unordered_set>
#include <vector>

namespace logger = SKSE::log;

// ---------------------------------------------------------------------------
// Settings (Data/SKSE/Plugins/IsoCamLock.ini)
// ---------------------------------------------------------------------------
struct Settings
{
	bool         enabled = true;
	bool         lockYaw = false;      // free mouse rotation by default
	bool         lockPitch = true;
	float        yawDegrees = 45.0f;    // world heading the camera faces. 0 = north, 90 = east
	float        pitchDegrees = 30.0f;  // how far the camera looks down
	int          pitchMode = 0;         // 0 = write player pitch, 1 = write camera freeRotation.y
	bool         invertPitch = false;   // flip sign if the camera looks up instead of down
	std::uint32_t toggleKey = 0x44;     // DirectInput scan code, default F10
	bool         debugLog = false;      // log camera values once per second
	bool         pauseOnTargetLock = false;  // let TDM hold the camera while target lock is active
	std::uint32_t ceilingKey = 0x43;    // DirectInput scan code, default F9
	float        ceilingClearance = 200.0f;  // hide objects whose lowest point is this far above the player's feet
	bool         ceilingsHiddenAtStart = false;
	std::uint32_t pitchUpKey = 0x49;    // Page Up: tilt toward top-down
	std::uint32_t pitchDownKey = 0x51;  // Page Down: tilt toward the horizon
	float        pitchStep = 5.0f;
	std::uint32_t reloadKey = 0x42;     // F8: reload the ini
	bool         fixInteriorRooms = true;  // draw the room the player is in, not the one the camera is in
	float        carryWeight = 0.0f;   // [Player] 0 = leave the game's value alone
	float        speedMult = 0.0f;     // [Player] 0 = leave alone; 100 = vanilla
	std::vector<std::pair<std::string, std::string>> gameSettings;  // [GameSettings] name = value
};

static Settings g_settings;
static bool     g_active = true;
static bool     g_hideCeilings = false;

static std::string Trim(std::string s)
{
	auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

static constexpr const char* kIniPath = "Data/SKSE/Plugins/IsoCamLock.ini";

// Rewrite one "key = value" line in the ini, keeping everything else (comments included).
static void SaveSetting(const std::string& a_key, const std::string& a_value)
{
	std::ifstream in(kIniPath);
	if (!in.is_open()) {
		logger::warn("Can't open ini to save {}", a_key);
		return;
	}
	std::vector<std::string> lines;
	std::string line;
	bool found = false;
	while (std::getline(in, line)) {
		const auto trimmed = Trim(line);
		const auto eq = trimmed.find('=');
		if (!found && eq != std::string::npos && Trim(trimmed.substr(0, eq)) == a_key) {
			line = a_key + " = " + a_value;
			found = true;
		}
		lines.push_back(line);
	}
	in.close();
	if (!found) {
		lines.push_back(a_key + " = " + a_value);
	}
	std::ofstream out(kIniPath, std::ios::trunc);
	for (const auto& l : lines) {
		out << l << '\n';
	}
}

static void LoadSettings()
{
	std::ifstream file(kIniPath);
	if (!file.is_open()) {
		logger::warn("IsoCamLock.ini not found, using defaults");
		return;
	}

	g_settings.gameSettings.clear();
	std::string section;
	std::string line;
	while (std::getline(file, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#') {
			continue;
		}
		if (line[0] == '[') {
			section = line;
			continue;
		}
		const auto eq = line.find('=');
		if (eq == std::string::npos) {
			continue;
		}
		auto key = Trim(line.substr(0, eq));
		auto value = Trim(line.substr(eq + 1));
		if (const auto semi = value.find(';'); semi != std::string::npos) {
			value = Trim(value.substr(0, semi));
		}

		if (section == "[GameSettings]") {
			g_settings.gameSettings.emplace_back(key, value);
			continue;
		}

		try {
			if (key == "bFixInteriorRooms") g_settings.fixInteriorRooms = std::stoi(value) != 0;
			else if (key == "fCarryWeight") g_settings.carryWeight = std::stof(value);
			else if (key == "fSpeedMult") g_settings.speedMult = std::stof(value);
			else if (key == "bEnabled") g_settings.enabled = std::stoi(value) != 0;
			else if (key == "bLockYaw") g_settings.lockYaw = std::stoi(value) != 0;
			else if (key == "bLockPitch") g_settings.lockPitch = std::stoi(value) != 0;
			else if (key == "fYaw") g_settings.yawDegrees = std::stof(value);
			else if (key == "fPitch") g_settings.pitchDegrees = std::stof(value);
			else if (key == "iPitchMode") g_settings.pitchMode = std::stoi(value);
			else if (key == "bInvertPitch") g_settings.invertPitch = std::stoi(value) != 0;
			else if (key == "iToggleKey") g_settings.toggleKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "bDebugLog") g_settings.debugLog = std::stoi(value) != 0;
			else if (key == "bPauseOnTargetLock") g_settings.pauseOnTargetLock = std::stoi(value) != 0;
			else if (key == "iCeilingKey") g_settings.ceilingKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "fCeilingClearance") g_settings.ceilingClearance = std::stof(value);
			else if (key == "iPitchUpKey") g_settings.pitchUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "iPitchDownKey") g_settings.pitchDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "fPitchStep") g_settings.pitchStep = std::stof(value);
			else if (key == "iReloadKey") g_settings.reloadKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "bCeilingsHiddenAtStart") g_settings.ceilingsHiddenAtStart = std::stoi(value) != 0;
		} catch (...) {
			logger::warn("Bad value for {}: {}", key, value);
		}
	}

	g_active = g_settings.enabled;
	logger::info("Settings: yaw={} pitch={} lockYaw={} lockPitch={} pitchMode={} invert={} pauseOnTargetLock={} toggleKey=0x{:X}",
		g_settings.yawDegrees, g_settings.pitchDegrees, g_settings.lockYaw, g_settings.lockPitch,
		g_settings.pitchMode, g_settings.invertPitch, g_settings.pauseOnTargetLock, g_settings.toggleKey);
}


// ---------------------------------------------------------------------------
// Game settings and player values from the ini
// ---------------------------------------------------------------------------
// [GameSettings] entries are engine settings (the ones `setgs` changes). The engine resets them
// on every launch, so they are re-applied once the game data has loaded.
static void ApplyGameSettings()
{
	auto* collection = RE::GameSettingCollection::GetSingleton();
	if (!collection) {
		return;
	}
	for (const auto& [name, value] : g_settings.gameSettings) {
		auto* setting = collection->GetSetting(name.c_str());
		if (!setting) {
			logger::warn("Game setting {} not found", name);
			continue;
		}
		try {
			switch (setting->GetType()) {
			case RE::Setting::Type::kFloat:
				setting->data.f = std::stof(value);
				break;
			case RE::Setting::Type::kInteger:
				setting->data.i = std::stoi(value);
				break;
			case RE::Setting::Type::kUnsignedInteger:
				setting->data.u = static_cast<std::uint32_t>(std::stoul(value));
				break;
			case RE::Setting::Type::kBool:
				setting->data.b = std::stoi(value) != 0;
				break;
			default:
				logger::warn("Game setting {} has a type this plugin can't set", name);
				continue;
			}
			logger::info("Game setting {} = {}", name, value);
		} catch (...) {
			logger::warn("Bad value for game setting {}: {}", name, value);
		}
	}
}

// [Player] values are character stats (the ones `player.setav` changes). They live in the save,
// so they are applied whenever a save loads or a new game starts.
static void ApplyPlayerValues()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	auto* av = player->AsActorValueOwner();
	if (!av) {
		return;
	}
	if (g_settings.speedMult > 0.0f) {
		av->SetBaseActorValue(RE::ActorValue::kSpeedMult, g_settings.speedMult);
	}
	if (g_settings.carryWeight > 0.0f) {
		// Also refreshes movement speed, which otherwise only updates on the next weight change.
		av->SetBaseActorValue(RE::ActorValue::kCarryWeight, g_settings.carryWeight);
	}
	logger::info("Player values applied: speedMult={} carryWeight={}", g_settings.speedMult, g_settings.carryWeight);
}

// ---------------------------------------------------------------------------
// Camera pinning
// ---------------------------------------------------------------------------
static float DegToRad(float deg) { return deg * (std::numbers::pi_v<float> / 180.0f); }

static float NormalizeAngle(float a)
{
	constexpr float pi = std::numbers::pi_v<float>;
	constexpr float twoPi = 2.0f * pi;
	a = std::fmod(a + pi, twoPi);
	if (a < 0.0f) {
		a += twoPi;
	}
	return a - pi;
}

// True Directional Movement sets this graph variable on the player while target lock is on.
static bool IsTargetLocked(RE::PlayerCharacter* a_player)
{
	static const RE::BSFixedString var{ "TDM_TargetLock" };
	bool locked = false;
	return a_player && a_player->GetGraphVariableBool(var, locked) && locked;
}

static void ApplyLock(RE::ThirdPersonState* a_state)
{
	if (!g_active || !a_state) {
		return;
	}
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	// While locked on, TDM keeps the camera still (TISS settings). Writing pitch here fights it.
	if (g_settings.pauseOnTargetLock && IsTargetLocked(player)) {
		return;
	}

	if (g_settings.lockYaw) {
		// Camera world yaw = player heading + freeRotation.x when free rotation is on.
		// Solve for the offset that keeps the camera at a fixed world heading.
		a_state->freeRotationEnabled = true;
		const float desired = DegToRad(g_settings.yawDegrees);
		a_state->freeRotation.x = NormalizeAngle(desired - player->data.angle.z);
	}

	if (g_settings.lockPitch) {
		float pitch = DegToRad(g_settings.pitchDegrees);
		if (g_settings.invertPitch) {
			pitch = -pitch;
		}
		if (g_settings.pitchMode == 1) {
			a_state->freeRotation.y = pitch;
		} else {
			player->data.angle.x = pitch;
			// TDM's target lock adds its own camera tilt here (seen hitting -60 deg, which points the
			// camera at the ground). Camera tilt = player pitch - freeRotation.y, so keep it at zero.
			a_state->freeRotation.y = 0.0f;
		}
	}
}

static void DebugLog(RE::ThirdPersonState* a_state)
{
	if (!g_settings.debugLog || !a_state) {
		return;
	}
	static auto last = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	if (now - last < std::chrono::seconds(1)) {
		return;
	}
	last = now;

	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	logger::info("targetLock={} freeRotEnabled={} freeRot=({:.3f}, {:.3f}) playerAngle=(x {:.3f}, z {:.3f}) curYaw={:.3f} tgtYaw={:.3f}",
		IsTargetLocked(player), a_state->freeRotationEnabled, a_state->freeRotation.x, a_state->freeRotation.y,
		player->data.angle.x, player->data.angle.z, a_state->currentYaw, a_state->targetYaw);
}


// ---------------------------------------------------------------------------
// Ceiling hiding (interiors only)
// ---------------------------------------------------------------------------
// Hides static objects that sit entirely above the player's head, so an overhead camera can see
// into houses and dungeons. Pieces that reach the floor (walls, most cave meshes) are left alone.
static std::vector<RE::NiPointer<RE::NiAVObject>> g_hiddenNodes;
static bool g_ceilingRescanRequested = true;

static void RestoreCeilings()
{
	for (auto& node : g_hiddenNodes) {
		if (node) {
			node->SetAppCulled(false);
		}
	}
	g_hiddenNodes.clear();
}

// Lowest world Z of the reference's object bounds (OBND), transformed by its 3D node.
static bool LowestPointZ(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_node, float& a_out)
{
	const auto mn = a_ref->GetBoundMin();
	const auto mx = a_ref->GetBoundMax();
	if (mn.x == mx.x && mn.y == mx.y && mn.z == mx.z) {
		return false;  // no bounds data
	}
	float lowest = std::numeric_limits<float>::max();
	for (int i = 0; i < 8; ++i) {
		const RE::NiPoint3 corner{ (i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z };
		const auto w = a_node->world * corner;
		lowest = std::min(lowest, w.z);
	}
	a_out = lowest;
	return true;
}

static void RescanCeilings(RE::PlayerCharacter* a_player)
{
	auto* cell = a_player->GetParentCell();
	if (!g_hideCeilings || !cell || !cell->IsInteriorCell()) {
		RestoreCeilings();
		return;
	}

	const float threshold = a_player->GetPositionZ() + g_settings.ceilingClearance;

	std::unordered_set<RE::NiAVObject*> previous;
	for (auto& node : g_hiddenNodes) {
		previous.insert(node.get());
	}

	std::vector<RE::NiPointer<RE::NiAVObject>> next;
	cell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
		if (!a_ref || a_ref->IsDisabled()) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		auto* base = a_ref->GetBaseObject();
		if (!base || !(base->Is(RE::FormType::Static) || base->Is(RE::FormType::MovableStatic))) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		auto* node = a_ref->Get3D();
		if (!node) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		float lowest = 0.0f;
		if (!LowestPointZ(a_ref, node, lowest) || lowest <= threshold) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		// Skip anything the game itself already culled, so we never un-hide it later.
		if (!previous.contains(node) && node->GetAppCulled()) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		next.emplace_back(node);
		return RE::BSContainer::ForEachResult::kContinue;
	});

	std::unordered_set<RE::NiAVObject*> nextSet;
	for (auto& node : next) {
		nextSet.insert(node.get());
		node->SetAppCulled(true);
	}
	for (auto& node : g_hiddenNodes) {
		if (node && !nextSet.contains(node.get())) {
			node->SetAppCulled(false);  // player moved up a floor, etc.
		}
	}
	g_hiddenNodes = std::move(next);
}

static void UpdateCeilings()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	static auto last = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	if (!g_ceilingRescanRequested && now - last < std::chrono::milliseconds(250)) {
		return;
	}
	last = now;
	g_ceilingRescanRequested = false;
	RescanCeilings(player);
}


// ---------------------------------------------------------------------------
// Interior room culling
// ---------------------------------------------------------------------------
// Interiors are split into rooms joined by portals, and the engine only draws the rooms it can
// reach from the room the CAMERA is in. An overhead camera is often above the ceiling or in a
// neighbouring room, so the player's own room vanishes. When the engine asks a room "does it
// contain this point?" and the answer is no, ask again at the player's eye and at points walking
// from there back toward the camera. Approach from Skyrim Camera Disocclusion System.
static std::atomic<float> g_eyeX{ 0.0f }, g_eyeY{ 0.0f }, g_eyeZ{ 0.0f };
static std::atomic<bool>  g_eyeValid{ false };

static void UpdateEye()
{
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		const auto pos = player->GetPosition();
		g_eyeX.store(pos.x, std::memory_order_relaxed);
		g_eyeY.store(pos.y, std::memory_order_relaxed);
		g_eyeZ.store(pos.z + 120.0f, std::memory_order_relaxed);
		g_eyeValid.store(true, std::memory_order_release);
	}
}

struct RoomContainsPointHook
{
	static bool thunk(RE::BSMultiBoundRoom* a_this, const RE::NiPoint3& a_point)
	{
		if (func(a_this, a_point)) {
			return true;
		}
		if (!g_settings.fixInteriorRooms || !g_eyeValid.load(std::memory_order_acquire)) {
			return false;
		}
		const RE::NiPoint3 eye{ g_eyeX.load(std::memory_order_relaxed), g_eyeY.load(std::memory_order_relaxed),
			g_eyeZ.load(std::memory_order_relaxed) };
		for (const float t : { 1.0f, 0.75f, 0.5f, 0.25f }) {
			const RE::NiPoint3 p{ a_point.x + (eye.x - a_point.x) * t, a_point.y + (eye.y - a_point.y) * t,
				a_point.z + (eye.z - a_point.z) * t };
			if (func(a_this, p)) {
				return true;
			}
		}
		return false;
	}
	static inline REL::Relocation<decltype(thunk)> func;

	static void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_BSMultiBoundRoom[0] };
		func = vtbl.write_vfunc(0x3F, thunk);
		logger::info("Hooked BSMultiBoundRoom::QPointWithin");
	}
};

// Hook ThirdPersonState::Update (vtable slot 0x3).
struct ThirdPersonUpdateHook
{
	static void thunk(RE::ThirdPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
	{
		UpdateEye();
		ApplyLock(a_this);         // set before the game builds this frame's camera
		func(a_this, a_nextState);
		ApplyLock(a_this);         // undo anything mouse input or other mods changed this frame
		UpdateCeilings();
		DebugLog(a_this);
	}
	static inline REL::Relocation<decltype(thunk)> func;

	static void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_ThirdPersonState[0] };
		func = vtbl.write_vfunc(0x3, thunk);
		logger::info("Hooked ThirdPersonState::Update");
	}
};

// ---------------------------------------------------------------------------
// Toggle hotkey
// ---------------------------------------------------------------------------
class InputSink : public RE::BSTEventSink<RE::InputEvent*>
{
public:
	static InputSink* GetSingleton()
	{
		static InputSink instance;
		return &instance;
	}

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		for (auto* e = *a_event; e; e = e->next) {
			if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
				continue;
			}
			auto* button = e->AsButtonEvent();
			if (!button || !button->IsDown() || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
				continue;
			}
			const auto code = button->GetIDCode();
			if (code == g_settings.pitchUpKey || code == g_settings.pitchDownKey) {
				const float step = code == g_settings.pitchUpKey ? g_settings.pitchStep : -g_settings.pitchStep;
				g_settings.pitchDegrees = std::clamp(g_settings.pitchDegrees + step, 5.0f, 85.0f);
				const auto text = std::format("{:.0f}", g_settings.pitchDegrees);
				SaveSetting("fPitch", text);
				static std::string msg;  // keep the text alive in case the HUD queues the pointer
				msg = "Camera pitch: " + text + " deg";
				RE::SendHUDMessage::ShowHUDMessage(msg.c_str());
				logger::info("Pitch set to {}", text);
			}
			if (code == g_settings.reloadKey) {
				LoadSettings();
				ApplyGameSettings();
				ApplyPlayerValues();
				g_ceilingRescanRequested = true;
				RE::SendHUDMessage::ShowHUDMessage("IsoCamLock: settings reloaded");
			}
			if (button->GetIDCode() == g_settings.ceilingKey) {
				g_hideCeilings = !g_hideCeilings;
				g_ceilingRescanRequested = true;
				if (!g_hideCeilings) {
					RestoreCeilings();
				}
				RE::SendHUDMessage::ShowHUDMessage(g_hideCeilings ? "Ceilings: hidden" : "Ceilings: shown");
				logger::info("Ceilings {}", g_hideCeilings ? "hidden" : "shown");
			}
			if (button->GetIDCode() == g_settings.toggleKey) {
				g_active = !g_active;
				RE::SendHUDMessage::ShowHUDMessage(g_active ? "IsoCamLock: ON" : "IsoCamLock: OFF");
				logger::info("Toggled {}", g_active ? "on" : "off");
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}
};

static void OnMessage(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
		ApplyGameSettings();
	}
	if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame || a_msg->type == SKSE::MessagingInterface::kNewGame) {
		ApplyPlayerValues();
	}
	if (a_msg->type == SKSE::MessagingInterface::kInputLoaded) {
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(InputSink::GetSingleton()));
			logger::info("Input sink registered");
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);  // also creates My Games\Skyrim Special Edition\SKSE\IsoCamLock.log
	logger::info("IsoCamLock loading");

	LoadSettings();
	g_hideCeilings = g_settings.ceilingsHiddenAtStart;
	ThirdPersonUpdateHook::Install();
	RoomContainsPointHook::Install();

	if (auto* messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	logger::info("IsoCamLock loaded");
	return true;
}
