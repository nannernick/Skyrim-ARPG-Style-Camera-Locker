#include "Menu.h"
#include "Camera.h"
#include "Input.h"
#include "Occlusion.h"
#include "Settings.h"

#include <filesystem>
#include "SKSEMenuFramework.h"

// Widgets write straight into Settings, so changes apply live. The render callbacks run
// on the render thread, so anything that touches game objects is queued to the main thread.
namespace Menu
{
	namespace
	{
		using namespace ImGuiMCP;

		std::string g_status;

		void RunOnMainThread(std::function<void()> a_task)
		{
			if (auto* tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask(std::move(a_task));
			}
		}

		void Hint(const char* a_text)
		{
			if (IsItemHovered()) {
				SetTooltip("%s", a_text);
			}
		}

		void Footer()
		{
			Spacing();
			Separator();
			if (Button("Save to INI")) {
				g_status = Settings::Save() ? "Saved." : "Couldn't write the INI - see the log.";
			}
			Hint("Writes every setting to Data/SKSE/Plugins/SkyrimARPGCamera.ini.");
			SameLine();
			if (Button("Reload INI")) {
				RunOnMainThread([] { Input::Reload(); });
				g_status = "Reloaded from the INI.";
			}
			Hint("Discards unsaved changes and re-reads the INI.");
			if (!g_status.empty()) {
				SameLine();
				TextDisabled("%s", g_status.c_str());
			}
		}

		void __stdcall RenderCamera()
		{
			SeparatorText("Camera lock");
			bool active = Camera::IsActive();
			if (Checkbox("Camera lock on", &active)) {
				Camera::SetActive(active);
				Settings::CameraEnabled = active;
			}
			Hint("Also toggled with the camera key (F10 by default).");

			Checkbox("Lock pitch", &Settings::LockPitch);
			SliderFloat("Pitch (degrees down)", &Settings::Pitch, 5.0f, 85.0f, "%.0f");
			Hint("30 = Path of Exile 2 / Diablo II, 50 = Path of Exile.");
			Checkbox("Lock yaw", &Settings::LockYaw);
			BeginDisabled(!Settings::LockYaw);
			SliderFloat("Yaw (compass degrees)", &Settings::Yaw, 0.0f, 360.0f, "%.0f");
			EndDisabled();
			SliderFloat("Pitch key step", &Settings::PitchStep, 1.0f, 15.0f, "%.0f");

			SeparatorText("Compatibility");
			Checkbox("Fix interior rooms", &Settings::FixInteriorRooms);
			Hint("Indoors, draw the room you're in even when the camera is above the ceiling.");
			Checkbox("Pause on TDM target lock", &Settings::PauseOnTargetLock);
			Checkbox("Invert pitch", &Settings::InvertPitch);
			Checkbox("Camera debug log", &Settings::CameraDebugLog);

			Footer();
		}

		void __stdcall RenderCutaway()
		{
			SeparatorText("Cutaway");
			Checkbox("Enabled", &Settings::Enabled);
			Checkbox("Interiors only", &Settings::InteriorOnly);

			const char* const styles[] = { "Auto - rises with the room", "Locked height (Diablo look)" };
			int               style = Occlusion::GetAutoLift() ? 0 : 1;
			if (Combo("Style", &style, styles, 2)) {
				Occlusion::SetAutoLift(style == 0);
				Settings::AutoLift = style == 0;
			}
			Hint("Also switched with the style key (F9 by default).");

			SeparatorText("Cut shape");
			SliderFloat("Height", &Settings::CutHeight, 50.0f, 800.0f, "%.0f");
			Hint("How far above the floor walls are cut (a human is ~128).");
			SliderFloat("Slope behind you", &Settings::CutSlope, 0.0f, 3.0f, "%.2f");
			Hint("How fast the cut rises away from the camera, so far walls stay. 0 = flat.");

			SeparatorText("Height changes");
			SliderFloat("Smoothing (s)", &Settings::HeightSmoothing, 0.0f, 3.0f, "%.2f");
			SliderFloat("Max speed", &Settings::MaxHeightSpeed, 50.0f, 3000.0f, "%.0f");
			SliderFloat("New floor distance", &Settings::ReanchorDistance, 50.0f, 1000.0f, "%.0f");
			Hint("How far above/below the current height you must stand before the cut moves.");
			SliderFloat("New floor delay (s)", &Settings::ReanchorDelay, 0.0f, 5.0f, "%.1f");

			SeparatorText("Auto style");
			BeginDisabled(!Occlusion::GetAutoLift());
			SliderFloat("Clear delay (s)", &Settings::ClearDelay, 0.0f, 5.0f, "%.2f");
			SliderFloat("Max rise", &Settings::LiftMax, 100.0f, 2000.0f, "%.0f");
			Hint("How far above Height the cut may rise in rooms with nothing in the way.");
			EndDisabled();

			SeparatorText("Background");
			Checkbox("Black interior fog", &Settings::BlackFog);
			Checkbox("Hide sky indoors", &Settings::HideSky);
			Hint("Fog and sky changes apply the next time you enter a cell.");

			SeparatorText("Advanced");
			Checkbox("All geometry (consistent depth)", &Settings::CutAllGeometry);
			Hint("Leave on. Off only cuts meshes that cross the plane, which hides props in many rooms.");
			Checkbox("Cutaway debug log", &Settings::DebugLog);

			Footer();
		}

		void __stdcall RenderPlayer()
		{
			SeparatorText("Player (applied on every load)");
			SliderFloat("Carry weight", &Settings::CarryWeight, 0.0f, 20000.0f, "%.0f");
			Hint("0 = leave the game's value alone.");
			SliderFloat("Speed mult", &Settings::SpeedMult, 0.0f, 300.0f, "%.0f");
			Hint("100 = vanilla. 0 = leave the game's value alone.");
			if (Button("Apply now")) {
				RunOnMainThread([] { Camera::ApplyPlayerValues(); });
				g_status = "Player values applied.";
			}

			SeparatorText("Keys");
			TextWrapped("Camera on/off: %#x   Cutaway style: %#x   Pitch up/down: %#x / %#x   Reload: %#x",
				Settings::CameraToggleKey, Settings::StyleKey, Settings::PitchUpKey, Settings::PitchDownKey, Settings::ReloadKey);
			TextDisabled("Change keys in the INI (DirectX scan codes), then Reload.");

			Footer();
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logs::info("[menu] SKSE Menu Framework not installed; in-game menu unavailable (INI still works)");
			return;
		}
		SKSEMenuFramework::SetSection("Skyrim ARPG Style Camera");
		SKSEMenuFramework::AddSectionItem("Camera", RenderCamera);
		SKSEMenuFramework::AddSectionItem("Cutaway", RenderCutaway);
		SKSEMenuFramework::AddSectionItem("Player", RenderPlayer);
		logs::info("[menu] registered with SKSE Menu Framework");
	}
}
