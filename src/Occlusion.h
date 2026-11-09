#pragma once

namespace Occlusion
{
	void Install();

	// Cutaway style: true = cut only when the view is blocked, false = always cut.
	bool GetAutoLift();
	void SetAutoLift(bool a_autoLift);
	void ToggleStyle();  // with a HUD message

	void RestoreAll();
}
