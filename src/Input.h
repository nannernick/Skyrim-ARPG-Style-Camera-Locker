#pragma once

// One keyboard handler for every hotkey: camera on/off, cutaway style, pitch up/down, reload.
namespace Input
{
	void Register();  // after kInputLoaded
	void Reload();    // re-read the INI and re-apply everything (main thread)
}
