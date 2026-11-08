// IsoCamLock - pins Skyrim's third-person camera to a fixed world yaw and pitch.
// Built against CommonLibSSE-NG (alandtse fork) for SE/AE/1.7.x runtimes.

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <numbers>
#include <string>

namespace logger = SKSE::log;

// ---------------------------------------------------------------------------
// Settings (Data/SKSE/Plugins/IsoCamLock.ini)
// ---------------------------------------------------------------------------
struct Settings
{
	bool         enabled = true;
	bool         lockYaw = true;
	bool         lockPitch = true;
	float        yawDegrees = 45.0f;    // world heading the camera faces. 0 = north, 90 = east
	float        pitchDegrees = 55.0f;  // how far the camera looks down
	int          pitchMode = 0;         // 0 = write player pitch, 1 = write camera freeRotation.y
	bool         invertPitch = false;   // flip sign if the camera looks up instead of down
	std::uint32_t toggleKey = 0x44;     // DirectInput scan code, default F10
	bool         debugLog = false;      // log camera values once per second
};

static Settings g_settings;
static bool     g_active = true;

static std::string Trim(std::string s)
{
	auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

static void LoadSettings()
{
	std::ifstream file("Data/SKSE/Plugins/IsoCamLock.ini");
	if (!file.is_open()) {
		logger::warn("IsoCamLock.ini not found, using defaults");
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
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

		try {
			if (key == "bEnabled") g_settings.enabled = std::stoi(value) != 0;
			else if (key == "bLockYaw") g_settings.lockYaw = std::stoi(value) != 0;
			else if (key == "bLockPitch") g_settings.lockPitch = std::stoi(value) != 0;
			else if (key == "fYaw") g_settings.yawDegrees = std::stof(value);
			else if (key == "fPitch") g_settings.pitchDegrees = std::stof(value);
			else if (key == "iPitchMode") g_settings.pitchMode = std::stoi(value);
			else if (key == "bInvertPitch") g_settings.invertPitch = std::stoi(value) != 0;
			else if (key == "iToggleKey") g_settings.toggleKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
			else if (key == "bDebugLog") g_settings.debugLog = std::stoi(value) != 0;
		} catch (...) {
			logger::warn("Bad value for {}: {}", key, value);
		}
	}

	g_active = g_settings.enabled;
	logger::info("Settings: yaw={} pitch={} lockYaw={} lockPitch={} pitchMode={} invert={} toggleKey=0x{:X}",
		g_settings.yawDegrees, g_settings.pitchDegrees, g_settings.lockYaw, g_settings.lockPitch,
		g_settings.pitchMode, g_settings.invertPitch, g_settings.toggleKey);
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

static void ApplyLock(RE::ThirdPersonState* a_state)
{
	if (!g_active || !a_state) {
		return;
	}
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
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
	logger::info("freeRotEnabled={} freeRot=({:.3f}, {:.3f}) playerAngle=(x {:.3f}, z {:.3f}) curYaw={:.3f} tgtYaw={:.3f}",
		a_state->freeRotationEnabled, a_state->freeRotation.x, a_state->freeRotation.y,
		player->data.angle.x, player->data.angle.z, a_state->currentYaw, a_state->targetYaw);
}

// Hook ThirdPersonState::Update (vtable slot 0x3).
struct ThirdPersonUpdateHook
{
	static void thunk(RE::ThirdPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
	{
		ApplyLock(a_this);         // set before the game builds this frame's camera
		func(a_this, a_nextState);
		ApplyLock(a_this);         // undo anything mouse input or other mods changed this frame
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
			if (button->GetIDCode() == g_settings.toggleKey) {
				g_active = !g_active;
				RE::DebugNotification(g_active ? "IsoCamLock: ON" : "IsoCamLock: OFF");
				logger::info("Toggled {}", g_active ? "on" : "off");
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}
};

static void OnMessage(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SKSE::MessagingInterface::kInputLoaded) {
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(InputSink::GetSingleton());
			logger::info("Input sink registered");
		}
	}
}

static void SetupLog()
{
	auto dir = logger::log_directory();
	if (!dir) {
		return;
	}
	*dir /= "IsoCamLock.log";
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(dir->string(), true);
	auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] [%l] %v");
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	logger::info("IsoCamLock loading");

	LoadSettings();
	ThirdPersonUpdateHook::Install();

	if (auto* messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	logger::info("IsoCamLock loaded");
	return true;
}
