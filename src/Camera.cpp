#include "Camera.h"
#include "Settings.h"

namespace Camera
{
	namespace
	{
		bool g_active = true;

		float DegToRad(float a_deg) { return a_deg * (std::numbers::pi_v<float> / 180.0f); }

		float NormalizeAngle(float a_angle)
		{
			constexpr float pi = std::numbers::pi_v<float>;
			constexpr float twoPi = 2.0f * pi;
			a_angle = std::fmod(a_angle + pi, twoPi);
			if (a_angle < 0.0f) {
				a_angle += twoPi;
			}
			return a_angle - pi;
		}

		// True Directional Movement sets this graph variable on the player while target lock is on.
		bool IsTargetLocked(RE::PlayerCharacter* a_player)
		{
			static const RE::BSFixedString var{ "TDM_TargetLock" };
			bool                           locked = false;
			return a_player && a_player->GetGraphVariableBool(var, locked) && locked;
		}

		void ApplyLock(RE::ThirdPersonState* a_state)
		{
			if (!g_active || !a_state) {
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			// While locked on, TDM keeps the camera still (TISS settings). Writing pitch here fights it.
			if (Settings::PauseOnTargetLock && IsTargetLocked(player)) {
				return;
			}

			if (Settings::LockYaw) {
				// Camera world yaw = player heading + freeRotation.x when free rotation is on.
				// Solve for the offset that keeps the camera at a fixed world heading.
				a_state->freeRotationEnabled = true;
				const float desired = DegToRad(Settings::Yaw);
				a_state->freeRotation.x = NormalizeAngle(desired - player->data.angle.z);
			}

			if (Settings::LockPitch) {
				float pitch = DegToRad(Settings::Pitch);
				if (Settings::InvertPitch) {
					pitch = -pitch;
				}
				if (Settings::PitchMode == 1) {
					a_state->freeRotation.y = pitch;
				} else {
					player->data.angle.x = pitch;
					// TDM's target lock adds its own camera tilt here (seen hitting -60 deg, which points the
					// camera at the ground). Camera tilt = player pitch - freeRotation.y, so keep it at zero.
					a_state->freeRotation.y = 0.0f;
				}
			}
		}

		void DebugLog(RE::ThirdPersonState* a_state)
		{
			if (!Settings::CameraDebugLog || !a_state) {
				return;
			}
			static auto last = std::chrono::steady_clock::now();
			const auto  now = std::chrono::steady_clock::now();
			if (now - last < std::chrono::seconds(1)) {
				return;
			}
			last = now;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			logs::info("[camera] targetLock={} freeRotEnabled={} freeRot=({:.3f}, {:.3f}) playerAngle=(x {:.3f}, z {:.3f}) curYaw={:.3f} tgtYaw={:.3f}",
				IsTargetLocked(player), a_state->freeRotationEnabled, a_state->freeRotation.x, a_state->freeRotation.y,
				player->data.angle.x, player->data.angle.z, a_state->currentYaw, a_state->targetYaw);
		}

		// -----------------------------------------------------------------------
		// Interior room culling
		// -----------------------------------------------------------------------
		// Interiors are split into rooms joined by portals, and the engine only draws the rooms it can
		// reach from the room the CAMERA is in. An overhead camera is often above the ceiling or in a
		// neighbouring room, so the player's own room vanishes. When the engine asks a room "does it
		// contain this point?" and the answer is no, ask again at the player's eye and at points walking
		// from there back toward the camera. Approach from Skyrim Camera Disocclusion System.
		std::atomic<float> g_eyeX{ 0.0f };
		std::atomic<float> g_eyeY{ 0.0f };
		std::atomic<float> g_eyeZ{ 0.0f };
		std::atomic<bool>  g_eyeValid{ false };

		void UpdateEye()
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
				if (!Settings::FixInteriorRooms || !g_eyeValid.load(std::memory_order_acquire)) {
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
				logs::info("[camera] hooked BSMultiBoundRoom::QPointWithin");
			}
		};

		// ThirdPersonState::Update (vtable slot 0x3).
		struct ThirdPersonUpdateHook
		{
			static void thunk(RE::ThirdPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
			{
				UpdateEye();
				ApplyLock(a_this);  // set before the game builds this frame's camera
				func(a_this, a_nextState);
				ApplyLock(a_this);  // undo anything mouse input or other mods changed this frame
				DebugLog(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_ThirdPersonState[0] };
				func = vtbl.write_vfunc(0x3, thunk);
				logs::info("[camera] hooked ThirdPersonState::Update");
			}
		};
	}

	void Install()
	{
		g_active = Settings::CameraEnabled;
		ThirdPersonUpdateHook::Install();
		RoomContainsPointHook::Install();
	}

	bool IsActive()
	{
		return g_active;
	}

	void SetActive(bool a_active)
	{
		g_active = a_active;
	}

	void Toggle()
	{
		g_active = !g_active;
		RE::SendHUDMessage::ShowHUDMessage(g_active ? "Camera lock: ON" : "Camera lock: OFF");
		logs::info("[camera] toggled {}", g_active ? "on" : "off");
	}

	void AdjustPitch(float a_delta)
	{
		Settings::Pitch = std::clamp(Settings::Pitch + a_delta, 5.0f, 85.0f);
		Settings::Save();
		static std::string msg;  // keep the text alive in case the HUD queues the pointer
		msg = std::format("Camera pitch: {:.0f} deg", Settings::Pitch);
		RE::SendHUDMessage::ShowHUDMessage(msg.c_str());
		logs::info("[camera] pitch set to {:.0f}", Settings::Pitch);
	}

	// [GameSettings] entries are engine settings (the ones `setgs` changes). The engine resets them
	// on every launch, so they are re-applied once the game data has loaded.
	void ApplyGameSettings()
	{
		auto* collection = RE::GameSettingCollection::GetSingleton();
		if (!collection) {
			return;
		}
		for (const auto& [name, value] : Settings::GameSettings) {
			auto* setting = collection->GetSetting(name.c_str());
			if (!setting) {
				logs::warn("[game] setting {} not found", name);
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
					logs::warn("[game] setting {} has a type this plugin can't set", name);
					continue;
				}
				logs::info("[game] {} = {}", name, value);
			} catch (...) {
				logs::warn("[game] bad value for {}: {}", name, value);
			}
		}
	}

	// [Player] values are character stats (the ones `player.setav` changes). They live in the save,
	// so they are applied whenever a save loads or a new game starts.
	void ApplyPlayerValues()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		auto* av = player->AsActorValueOwner();
		if (!av) {
			return;
		}
		if (Settings::SpeedMult > 0.0f) {
			av->SetBaseActorValue(RE::ActorValue::kSpeedMult, Settings::SpeedMult);
		}
		if (Settings::CarryWeight > 0.0f) {
			// Also refreshes movement speed, which otherwise only updates on the next weight change.
			av->SetBaseActorValue(RE::ActorValue::kCarryWeight, Settings::CarryWeight);
		}
		logs::info("[player] values applied: speedMult={} carryWeight={}", Settings::SpeedMult, Settings::CarryWeight);
	}
}
