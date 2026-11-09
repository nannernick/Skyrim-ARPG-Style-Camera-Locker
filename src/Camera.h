#pragma once

// Camera lock: pins the third-person camera's pitch (and optionally yaw) for ARPG play,
// keeps the player's room drawn indoors, and applies the [Player]/[GameSettings] tweaks.
namespace Camera
{
	void Install();

	bool IsActive();
	void SetActive(bool a_active);
	void Toggle();                    // with a HUD message
	void AdjustPitch(float a_delta);  // clamps, saves, shows a HUD message

	void ApplyGameSettings();  // after kDataLoaded
	void ApplyPlayerValues();  // after a save loads or a new game starts
}
