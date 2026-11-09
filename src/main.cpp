// Skyrim ARPG Style Camera Mod: camera lock + interior cutaway + in-game settings.
// Built against CommonLibSSE-NG (alandtse fork) for SE/AE/1.7.x runtimes.

#include "Camera.h"
#include "Cut.h"
#include "Input.h"
#include "Menu.h"
#include "Occlusion.h"
#include "Settings.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			Menu::Register();
			break;
		case SKSE::MessagingInterface::kInputLoaded:
			Input::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Camera::ApplyGameSettings();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Occlusion::RestoreAll();
			break;
		case SKSE::MessagingInterface::kNewGame:
			Occlusion::RestoreAll();
			Camera::ApplyPlayerValues();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			Camera::ApplyPlayerValues();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .trampoline = true, .trampolineSize = 64 });
	logs::info("Skyrim ARPG Style Camera Mod loading");

	Settings::Load();
	Camera::Install();
	Occlusion::Install();
	Cut::Install();

	if (auto* messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	logs::info("Skyrim ARPG Style Camera Mod loaded");
	return true;
}
