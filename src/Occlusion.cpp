#include "Occlusion.h"
#include "Cut.h"
#include "Settings.h"

// ARPG Cutaway - Mode 0 (hide whole meshes) plus the shared per-frame tick.
//
// Every few frames we build a "corridor": the segment from the camera to the
// player's head, padded by CorridorRadius. Every loaded reference whose 3D
// reaches into that corridor is walked down to its individual meshes
// (BSGeometry). A mesh is hidden (app-culled) when:
//   - its bound center is at least MinHeight above the player's feet,
//   - it is a "ceiling" piece (center >= CeilingHeight), or it is on the
//     camera's side of the player (walls behind the player stay),
//   - its bound radius is <= MaxShapeRadius (so whole-room shells survive),
//   - its bound comes within CorridorRadius of the segment.
// Hidden meshes come back Linger seconds after they stop qualifying, and
// everything is restored on cell change and on save load.

namespace Occlusion
{
	namespace
	{
		constexpr auto          kContinue = RE::BSContainer::ForEachResult::kContinue;
		constexpr std::uint32_t kMaxShapeLogsPerCell = 400;
		constexpr std::uint32_t kMaxShapesPerRef = 512;

		struct HiddenEntry
		{
			RE::NiPointer<RE::NiAVObject> keepalive;
			bool                          wasCulled{ false };
			std::uint32_t                 lastSweep{ 0 };
			float                         unseenTime{ 0.0f };
		};

		struct FogBackup
		{
			RE::INTERIOR_DATA* data{ nullptr };
			RE::Color          nearColor{};
			RE::Color          farColor{};
		};

		std::unordered_map<RE::NiAVObject*, HiddenEntry> g_hidden;
		std::unordered_set<RE::FormID>                  g_loggedRefs;
		std::vector<FogBackup>                          g_fogBackups;
		RE::TESObjectCELL*                              g_lastCell{ nullptr };
		std::uint32_t                                   g_sweepId{ 0 };
		float                                           g_sweepTimer{ 0.0f };
		std::uint32_t                                   g_loggedShapes{ 0 };
		bool                                            g_skyHidden{ false };

		// Cut-mode anchor: the cut height is fixed per area and only re-anchors on a
		// sustained change in floor level (another storey), gliding to the new height.
		bool                            g_anchorValid{ false };
		float                           g_anchorZ{ 0.0f };
		float                           g_reanchorTimer{ 0.0f };
		bool                            g_smoothValid{ false };
		float                           g_smoothFloor{ 0.0f };
		bool                            g_autoLift{ true };  // runtime style, toggled by StyleKey / the menu
		const Settings::CellOverride*   g_cellOverride{ nullptr };

		// Auto-lift: how far the cut is currently raised above its normal height.
		float g_lift{ 0.0f };
		float g_clearTime{ 0.0f };
		bool  g_viewBlocked{ true };

		// Eases a_value toward a_target: exponential (time constant HeightSmoothing)
		// with a hard speed cap (MaxHeightSpeed units/second).
		void Smooth(float& a_value, float a_target, float a_dt)
		{
			const float tau = Settings::HeightSmoothing;
			const float alpha = tau > 0.0f ? 1.0f - std::exp(-a_dt / tau) : 1.0f;
			float       step = (a_target - a_value) * alpha;
			const float cap = Settings::MaxHeightSpeed * a_dt;
			step = std::clamp(step, -cap, cap);
			a_value = std::abs(a_target - a_value) <= std::abs(step) ? a_target : a_value + step;
		}

		// True if static world geometry (walls, roofs, rock) sits between the camera
		// and the player. Uses collision, so it is unaffected by the visual cut.
		bool SightBlocked(RE::PlayerCharacter* a_player, RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_camPos, const RE::NiPoint3& a_feet)
		{
			auto* world = a_cell ? a_cell->GetbhkWorld() : nullptr;
			if (!world) {
				return true;  // unknown: keep cutting
			}
			const float scale = RE::bhkWorld::GetWorldScale();
			RE::CFilter filter;
			a_player->GetCollisionFilterInfo(filter);

			const float targets[3] = { 40.0f, 90.0f, Settings::HeadHeight };
			RE::BSReadLockGuard lock(world->worldLock);
			for (const float h : targets) {
				RE::bhkPickData pick;
				pick.rayInput.from = { a_camPos.x * scale, a_camPos.y * scale, a_camPos.z * scale, 0.0f };
				pick.rayInput.to = { a_feet.x * scale, a_feet.y * scale, (a_feet.z + h) * scale, 0.0f };
				pick.rayInput.filterInfo = filter;
				if (!world->PickObject(pick) || !pick.rayOutput.HasHit() || !pick.rayOutput.rootCollidable) {
					continue;
				}
				const auto layer = pick.rayOutput.rootCollidable->GetCollisionLayer();
				if (layer == RE::COL_LAYER::kStatic || layer == RE::COL_LAYER::kAnimStatic ||
					layer == RE::COL_LAYER::kTrees || layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround) {
					return true;
				}
			}
			return false;
		}

		float         g_perfTimer{ 0.0f };
		std::uint32_t g_perfSweeps{ 0 };
		std::uint64_t g_perfRefs{ 0 };
		double        g_perfMs{ 0.0 };

		float DistSqToSegment(const RE::NiPoint3& a_p, const RE::NiPoint3& a_a, const RE::NiPoint3& a_b)
		{
			const float abx = a_b.x - a_a.x, aby = a_b.y - a_a.y, abz = a_b.z - a_a.z;
			const float apx = a_p.x - a_a.x, apy = a_p.y - a_a.y, apz = a_p.z - a_a.z;
			const float len2 = abx * abx + aby * aby + abz * abz;
			float       t = len2 > 1e-6f ? (apx * abx + apy * aby + apz * abz) / len2 : 0.0f;
			t = std::clamp(t, 0.0f, 1.0f);
			const float dx = a_p.x - (a_a.x + abx * t);
			const float dy = a_p.y - (a_a.y + aby * t);
			const float dz = a_p.z - (a_a.z + abz * t);
			return dx * dx + dy * dy + dz * dz;
		}

		std::string ColorHex(const RE::Color& a_c)
		{
			return std::format("#{:02X}{:02X}{:02X}", a_c.red, a_c.green, a_c.blue);
		}

		const char* SafeStr(const char* a_s)
		{
			return a_s ? a_s : "";
		}

		const char* ModelPath(RE::TESObjectREFR* a_ref)
		{
			auto*       base = a_ref->GetBaseObject();
			auto*       model = base ? base->As<RE::TESModel>() : nullptr;
			const char* path = model ? model->GetModel() : nullptr;
			return (path && path[0]) ? path : "<none>";
		}

		template <class F>
		void ForEachShape(RE::NiAVObject* a_obj, F& a_fn, std::uint32_t& a_budget)
		{
			if (!a_obj || a_budget == 0) {
				return;
			}
			if (auto* geom = a_obj->AsGeometry()) {
				--a_budget;
				a_fn(static_cast<RE::NiAVObject*>(geom));
				return;
			}
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					if (child) {
						ForEachShape(child.get(), a_fn, a_budget);
					}
				}
			}
		}

		void RestoreEntry(const HiddenEntry& a_entry)
		{
			if (a_entry.wasCulled) {
				return;  // it was already hidden by something else; leave it alone
			}
			if (auto* live = a_entry.keepalive.get()) {
				live->SetAppCulled(false);
			}
		}

		void RestoreShapes()
		{
			for (auto& [obj, entry] : g_hidden) {
				RestoreEntry(entry);
			}
			g_hidden.clear();
		}

		void RestoreFog()
		{
			for (auto& backup : g_fogBackups) {
				if (backup.data) {
					backup.data->fogColorNear = backup.nearColor;
					backup.data->fogColorFar = backup.farColor;
				}
			}
			g_fogBackups.clear();
		}

		void BackupAndBlacken(RE::INTERIOR_DATA* a_data)
		{
			if (!a_data) {
				return;
			}
			for (const auto& backup : g_fogBackups) {
				if (backup.data == a_data) {
					return;
				}
			}
			g_fogBackups.push_back(FogBackup{ a_data, a_data->fogColorNear, a_data->fogColorFar });
			a_data->fogColorNear.red = 0;
			a_data->fogColorNear.green = 0;
			a_data->fogColorNear.blue = 0;
			a_data->fogColorFar.red = 0;
			a_data->fogColorFar.green = 0;
			a_data->fogColorFar.blue = 0;
		}

		// The sky dome still renders behind interiors. With the roof gone it is what
		// shows through (night sky = dark blue), so hide it while inside.
		void SetSkyHidden(bool a_hide)
		{
			auto* sky = RE::Sky::GetSingleton();
			auto* root = sky ? sky->root.get() : nullptr;
			if (!root) {
				return;
			}
			if (a_hide) {
				if (!g_skyHidden && Settings::DebugLog) {
					logs::info("[sky] hidden");
				}
				root->SetAppCulled(true);  // every frame, in case the engine flips it back
				g_skyHidden = true;
			} else if (g_skyHidden) {
				root->SetAppCulled(false);
				g_skyHidden = false;
				if (Settings::DebugLog) {
					logs::info("[sky] restored");
				}
			}
		}

		void OnCellChanged(RE::TESObjectCELL* a_cell)
		{
			RestoreShapes();
			RestoreFog();
			g_loggedRefs.clear();
			g_loggedShapes = 0;
			g_anchorValid = false;
			g_reanchorTimer = 0.0f;
			g_cellOverride = nullptr;
			g_lift = 0.0f;
			g_clearTime = 0.0f;
			g_viewBlocked = true;
			g_smoothValid = false;

			if (!a_cell) {
				return;
			}

			g_cellOverride = Settings::FindCellOverride(a_cell->GetFormEditorID());
			if (g_cellOverride) {
				if (g_cellOverride->off) {
					logs::info("[cut] override for '{}': off", SafeStr(a_cell->GetFormEditorID()));
				} else {
					logs::info("[cut] override for '{}': height {:.0f}", SafeStr(a_cell->GetFormEditorID()), g_cellOverride->height);
				}
			}

			const bool interior = a_cell->IsInteriorCell();
			auto*      lighting = interior ? a_cell->GetLighting() : nullptr;
			auto*      tmpl = interior ? a_cell->GetRuntimeData().lightingTemplate : nullptr;

			if (Settings::DebugLog) {
				logs::info("[cell] '{}' ({:08X}) interior={}", SafeStr(a_cell->GetFormEditorID()), a_cell->GetFormID(), interior);
				if (lighting) {
					logs::info("[cell]   own fog near={} far={}", ColorHex(lighting->fogColorNear), ColorHex(lighting->fogColorFar));
				}
				if (tmpl) {
					logs::info("[cell]   template {:08X} fog near={} far={}", tmpl->GetFormID(), ColorHex(tmpl->data.fogColorNear), ColorHex(tmpl->data.fogColorFar));
				}
			}

			if (interior && Settings::BlackFog) {
				BackupAndBlacken(lighting);
				if (tmpl) {
					BackupAndBlacken(&tmpl->data);
				}
				logs::info("[fog] interior fog set to black ({} block(s))", g_fogBackups.size());
			}
		}

		void Sweep(RE::PlayerCharacter* a_player, float a_dt)
		{
			const auto start = std::chrono::steady_clock::now();

			auto* camera = RE::PlayerCamera::GetSingleton();
			auto* camNode = camera ? camera->cameraRoot.get() : nullptr;
			if (!camNode) {
				return;
			}

			const RE::NiPoint3 camPos = camNode->world.translate;
			const RE::NiPoint3 feet = a_player->GetPosition();
			const RE::NiPoint3 head{ feet.x, feet.y, feet.z + Settings::HeadHeight };
			const float        sx = head.x - camPos.x, sy = head.y - camPos.y, sz = head.z - camPos.z;
			const float        segLen = std::sqrt(sx * sx + sy * sy + sz * sz);

			++g_sweepId;
			std::uint64_t refsVisited = 0;

			auto* tes = RE::TES::GetSingleton();
			if (tes && segLen >= Settings::MinCameraDistance) {
				const float corridor = Settings::CorridorRadius;
				const float minZ = feet.z + Settings::MinHeight;
				const float ceilZ = feet.z + Settings::CeilingHeight;

				// Horizontal direction from the player toward the camera. Wall-height
				// pieces only hide on this side, so walls behind the player stay.
				float      toCamX = camPos.x - feet.x;
				float      toCamY = camPos.y - feet.y;
				const float toCamLen = std::sqrt(toCamX * toCamX + toCamY * toCamY);
				const bool haveDir = toCamLen > 1.0f;
				if (haveDir) {
					toCamX /= toCamLen;
					toCamY /= toCamLen;
				}

				tes->ForEachReferenceInRange(a_player, segLen + corridor, [&](RE::TESObjectREFR* a_ref) {
					++refsVisited;
					if (!a_ref || a_ref == a_player || a_ref->IsDisabled() || a_ref->Is(RE::FormType::ActorCharacter)) {
						return kContinue;
					}
					auto* root = a_ref->Get3D();
					if (!root) {
						return kContinue;
					}
					const auto& rb = root->worldBound;
					const float slack = Settings::Hysteresis;  // so already-hidden pieces still get visited
					if (rb.center.z + rb.radius < minZ - slack) {
						return kContinue;  // entirely below the cutoff height
					}
					const float rootReach = corridor + rb.radius + slack;
					if (DistSqToSegment(rb.center, camPos, head) > rootReach * rootReach) {
						return kContinue;  // nowhere near the corridor
					}

					const bool logRef = Settings::DebugLog && g_loggedShapes < kMaxShapeLogsPerCell &&
					                    g_loggedRefs.insert(a_ref->GetFormID()).second;
					if (logRef) {
						logs::info("[ref] {:08X} model={} rootR={:.0f} rootZ={:+.0f}",
							a_ref->GetFormID(), ModelPath(a_ref), rb.radius, rb.center.z - feet.z);
					}

					auto onShape = [&](RE::NiAVObject* a_shape) {
						const auto& b = a_shape->worldBound;
						// Hysteresis: a piece that is already hidden only comes back once it is
						// clearly out of range, not the moment it brushes a threshold.
						const auto  existing = g_hidden.find(a_shape);
						const bool  wasHidden = existing != g_hidden.end();
						const float m = wasHidden ? Settings::Hysteresis : 0.0f;
						const float reach = corridor + b.radius + m;
						const bool  high = b.center.z >= minZ - m;
						const bool  notTooBig = b.radius <= Settings::MaxShapeRadius;
						const bool  inCorridor = DistSqToSegment(b.center, camPos, head) <= reach * reach;
						const float along = haveDir ? (b.center.x - feet.x) * toCamX + (b.center.y - feet.y) * toCamY : 0.0f;
						const bool  ceiling = b.center.z >= ceilZ - m;
						const bool  cameraSide = along >= -Settings::BehindMargin - m;
						const bool  hide = high && notTooBig && inCorridor && (ceiling || cameraSide);

						if (logRef && g_loggedShapes < kMaxShapeLogsPerCell) {
							++g_loggedShapes;
							logs::info("[shape]   '{}' z={:+.0f} r={:.0f} along={:+.0f} high={} ceiling={} cameraSide={} notTooBig={} inCorridor={} -> hide={}",
								SafeStr(a_shape->name.c_str()), b.center.z - feet.z, b.radius, along, high, ceiling, cameraSide, notTooBig, inCorridor, hide);
						}
						if (!hide) {
							return;
						}

						if (wasHidden) {
							existing->second.lastSweep = g_sweepId;
							existing->second.unseenTime = 0.0f;
							return;
						}
						auto [it, inserted] = g_hidden.try_emplace(a_shape);
						if (inserted) {
							it->second.keepalive = RE::NiPointer<RE::NiAVObject>(a_shape);
							it->second.wasCulled = a_shape->GetAppCulled();
							if (!it->second.wasCulled) {
								a_shape->SetAppCulled(true);
							}
						}
						it->second.lastSweep = g_sweepId;
						it->second.unseenTime = 0.0f;
					};

					std::uint32_t budget = kMaxShapesPerRef;
					ForEachShape(root, onShape, budget);
					return kContinue;
				});
			}

			// Bring back pieces that stopped qualifying.
			for (auto it = g_hidden.begin(); it != g_hidden.end();) {
				auto& entry = it->second;
				if (entry.lastSweep != g_sweepId) {
					entry.unseenTime += a_dt;
					if (entry.unseenTime >= Settings::Linger) {
						RestoreEntry(entry);
						it = g_hidden.erase(it);
						continue;
					}
				}
				++it;
			}

			++g_perfSweeps;
			g_perfRefs += refsVisited;
			g_perfMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		void Tick(RE::Actor* a_actor, float a_delta)
		{
			if (!Settings::Enabled) {
				Cut::Deactivate();
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || a_actor != player) {
				return;
			}
			if (!player->Is3DLoaded()) {
				RestoreAll();
				return;
			}

			auto* cell = player->GetParentCell();
			if (cell != g_lastCell) {
				OnCellChanged(cell);
				g_lastCell = cell;
			}
			if (!cell) {
				return;
			}
			SetSkyHidden(Settings::HideSky && cell->IsInteriorCell());
			if (Settings::InteriorOnly && !cell->IsInteriorCell()) {
				RestoreShapes();
				Cut::Deactivate();
				return;
			}

			if (Settings::Mode == 1) {
				// Horizontal cut mode: the render hook does the work; no per-mesh hiding.
				if (!g_hidden.empty()) {
					RestoreShapes();
				}
				auto*      camera = RE::PlayerCamera::GetSingleton();
				auto*      camNode = camera ? camera->cameraRoot.get() : nullptr;
				const auto feet = player->GetPosition();

				// Per-area anchor with a dead zone, so stairs and bumps don't move the cut.
				if (!g_anchorValid) {
					g_anchorZ = feet.z;
					g_anchorValid = true;
					g_smoothValid = false;
				}
				// Only count time on the ground at a new height: jumps and falls never re-anchor.
				const bool grounded = !player->IsInMidair();
				if (grounded && std::abs(feet.z - g_anchorZ) > Settings::ReanchorDistance) {
					g_reanchorTimer += a_delta;
					if (g_reanchorTimer >= Settings::ReanchorDelay) {
						if (Settings::DebugLog) {
							logs::info("[cut] re-anchoring {:+.0f} -> {:+.0f}", g_anchorZ, feet.z);
						}
						g_anchorZ = feet.z;  // the smoothing below glides the cut there
						g_reanchorTimer = 0.0f;
					}
				} else {
					g_reanchorTimer = 0.0f;
				}

				// Auto style: lift the cut out of the way once the view has been clear for
				// ClearDelay. Cutaway style (Diablo look): always cut.
				float liftTarget = 0.0f;
				if (g_autoLift && camNode) {
					const bool blocked = SightBlocked(player, cell, camNode->world.translate, feet);
					if (blocked != g_viewBlocked && Settings::DebugLog) {
						logs::info("[cut] view {}", blocked ? "blocked -> lowering cut" : "clear -> raising cut");
					}
					g_viewBlocked = blocked;
					g_clearTime = blocked ? 0.0f : g_clearTime + a_delta;
					if (!blocked && g_clearTime >= Settings::ClearDelay) {
						liftTarget = Settings::LiftMax;
					}
				}

				// Every height change (new floor, lift, drop) eases toward its target:
				// exponential smoothing with a speed cap, so the cut never jumps.
				if (!g_smoothValid) {
					g_smoothFloor = g_anchorZ;
					g_lift = liftTarget;
					g_smoothValid = true;
				}
				Smooth(g_smoothFloor, g_anchorZ, a_delta);
				Smooth(g_lift, liftTarget, a_delta);

				const bool liftedAway = liftTarget > 0.0f && g_lift >= Settings::LiftMax - 1.0f;
				const bool off = (g_cellOverride && g_cellOverride->off) || liftedAway;
				if (camNode && !off) {
					const float  height = g_cellOverride ? g_cellOverride->height : Settings::CutHeight;
					const float  cutZ = g_smoothFloor + height + g_lift;
					const auto&  camPos = camNode->world.translate;

					// Keep points with z <= cutZ + slope * (distance past the player, away from
					// the camera). The render side clamps the slope to keep the floor on screen.
					Cut::Params params;
					params.cutZ = cutZ;
					params.floorZ = g_smoothFloor;
					float       ax = feet.x - camPos.x;
					float       ay = feet.y - camPos.y;
					const float aLen = std::sqrt(ax * ax + ay * ay);
					if (aLen > 1.0f) {
						params.ax = ax / aLen;
						params.ay = ay / aLen;
						params.slope = Settings::CutSlope;
					}
					Cut::Publish(params, feet, camPos);
				} else {
					Cut::Deactivate();
				}

				g_perfTimer += a_delta;
				if (g_perfTimer >= 2.0f) {
					const auto cut = Cut::TakeCutPassCount();
					const auto skipped = Cut::TakeSkippedCount();
					if (Settings::DebugLog) {
						logs::info("[cut] passes/s={:.0f} skipped/s={:.0f} anchor={:+.0f} feet={:+.0f} camZ={:+.0f} slopeUsed={:.2f} lift={:.0f} blocked={} off={}",
							cut / g_perfTimer, skipped / g_perfTimer, g_anchorZ, feet.z,
							camNode ? camNode->world.translate.z - feet.z : 0.0f, Cut::LastSlope(), g_lift, g_viewBlocked, off);
					}
					g_perfTimer = 0.0f;
				}
				return;
			}
			Cut::Deactivate();

			g_perfTimer += a_delta;
			if (g_perfTimer >= 2.0f) {
				if (Settings::DebugLog && g_perfSweeps > 0) {
					logs::info("[perf] sweeps/s={:.1f} refs/sweep={} hidden={} ms/sweep={:.3f}",
						g_perfSweeps / g_perfTimer, g_perfRefs / g_perfSweeps, g_hidden.size(), g_perfMs / g_perfSweeps);
				}
				g_perfTimer = 0.0f;
				g_perfSweeps = 0;
				g_perfRefs = 0;
				g_perfMs = 0.0;
			}

			g_sweepTimer += a_delta;
			if (g_sweepTimer < Settings::SweepInterval) {
				return;
			}
			const float dt = g_sweepTimer;
			g_sweepTimer = 0.0f;
			Sweep(player, dt);
		}

		struct PlayerUpdate
		{
			static void thunk(RE::Actor* a_this, float a_delta)
			{
				func(a_this, a_delta);
				Tick(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		g_autoLift = Settings::AutoLift;  // starting style
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = vtbl.write_vfunc(0xAD, PlayerUpdate::thunk);
		logs::info("installed PlayerCharacter::Update hook (vtbl[0xAD])");
	}

	bool GetAutoLift()
	{
		return g_autoLift;
	}

	void SetAutoLift(bool a_autoLift)
	{
		g_autoLift = a_autoLift;
		g_clearTime = 0.0f;
	}

	void ToggleStyle()
	{
		SetAutoLift(!g_autoLift);
		RE::SendHUDMessage::ShowHUDMessage(g_autoLift ? "Cutaway: Auto (cuts only when blocked)" : "Cutaway: Always cut");
		logs::info("[cut] style toggled: {}", g_autoLift ? "auto" : "always cut");
	}

	void RestoreAll()
	{
		Cut::Deactivate();
		RestoreShapes();
		RestoreFog();
		SetSkyHidden(false);
		g_lastCell = nullptr;
	}
}
