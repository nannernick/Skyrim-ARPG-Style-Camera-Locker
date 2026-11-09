#pragma once

// Horizontal cut: for meshes that reach above a plane at CutHeight over the
// player's feet, the render pass is drawn with an oblique projection whose near
// plane is that horizontal plane. Everything above it (between the camera and
// the plane) is clipped by the GPU; everything below is untouched.
namespace Cut
{
	void Install();

	// The cut is a plane through height cutZ at the player, rising by up to
	// `slope` per unit away from the camera (ax, ay = horizontal unit direction
	// from camera to player). The render side lowers the slope if needed so the
	// plane never dips below floorZ anywhere on screen.
	struct Params
	{
		float cutZ{ 0.0f };
		float floorZ{ 0.0f };
		float ax{ 0.0f };
		float ay{ 0.0f };
		float slope{ 0.0f };
	};

	// Main thread: publish this frame's cut parameters.
	void Publish(const Params& a_params, const RE::NiPoint3& a_player, const RE::NiPoint3& a_camera);

	// Slope actually used on the last frame (after the floor clamp), for logging.
	float LastSlope();
	void Deactivate();

	// Number of passes drawn with the cut since the last call.
	std::uint32_t TakeCutPassCount();
	std::uint32_t TakeSkippedCount();
}
