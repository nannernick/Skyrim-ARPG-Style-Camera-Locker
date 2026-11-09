#include "Input.h"
#include "Camera.h"
#include "Occlusion.h"
#include "Settings.h"

namespace Input
{
	namespace
	{
		class KeySink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static KeySink* Get()
			{
				static KeySink s_sink;
				return &s_sink;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* e = *a_event; e; e = e->next) {
					if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || e->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
						continue;
					}
					auto* button = e->AsButtonEvent();
					if (!button || !button->IsDown()) {
						continue;
					}
					auto* ui = RE::UI::GetSingleton();
					if (ui && ui->GameIsPaused()) {
						continue;  // menus, console
					}
					Handle(button->GetIDCode());
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			static void Handle(std::uint32_t a_code)
			{
				if (a_code == 0) {
					return;
				}
				if (a_code == Settings::CameraToggleKey) {
					Camera::Toggle();
				}
				if (a_code == Settings::StyleKey) {
					Occlusion::ToggleStyle();
				}
				if (a_code == Settings::PitchUpKey) {
					Camera::AdjustPitch(Settings::PitchStep);
				}
				if (a_code == Settings::PitchDownKey) {
					Camera::AdjustPitch(-Settings::PitchStep);
				}
				if (a_code == Settings::ReloadKey) {
					Reload();
					RE::SendHUDMessage::ShowHUDMessage("Skyrim ARPG Camera: settings reloaded");
				}
			}
		};
	}

	void Register()
	{
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(KeySink::Get()));
			logs::info("[input] keys registered: camera {:#x}, style {:#x}, pitch {:#x}/{:#x}, reload {:#x}",
				Settings::CameraToggleKey, Settings::StyleKey, Settings::PitchUpKey, Settings::PitchDownKey, Settings::ReloadKey);
		} else {
			logs::error("[input] no input device manager; hotkeys unavailable");
		}
	}

	void Reload()
	{
		// Keep the cut style F9 picked unless the INI's AutoLift itself was changed.
		static bool s_fileAutoLift = Settings::AutoLift;
		Settings::Load();
		Camera::SetActive(Settings::CameraEnabled);
		if (Settings::AutoLift != s_fileAutoLift) {
			s_fileAutoLift = Settings::AutoLift;
			Occlusion::SetAutoLift(Settings::AutoLift);
		} else {
			Settings::AutoLift = Occlusion::GetAutoLift();  // so Save to INI keeps the current style
		}
		Occlusion::RestoreAll();  // re-reads cell overrides, fog and sky on the next frame
		Camera::ApplyGameSettings();
		Camera::ApplyPlayerValues();
	}
}
