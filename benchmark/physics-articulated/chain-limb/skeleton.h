#pragma once
// Chain-limb test limb scenarios: read a humanoid skeleton (bind pose) with ufbx and derive one limb chain (boxes, hinges, masses, case set-up).
#include <string>

#include "backend.h"

namespace artic {
// Print every bone: world position in the file's default pose, parent, local axes in world coordinates.
int dump_skeleton(const std::string& fbx_path);

struct LimbRequest {
  std::string fbx, limb = "leg";           // limb: leg | arm (left side, _L bones)
  std::string sizes_json, size_label;          // sizes.json (rows of the size table), size_label name
  std::string limb_case = "hold";          // hold | swing | wall | impact
  std::string arm_pose = "hang";           // hang (arm rotated down about the shoulder) | tpose (file bind pose, horizontal)
  bool worst_axes = false;                 // every hinge axis = world z (parallel to gravity), no raise, no launch
  double ton_kg = 1000.0;                  // weight unit reading: 1000 (metric) or 907.185 (short)
  double tip_speed = 30.0, wall_time = 0.08, raise = -0.9;  // m/s, s (wall placed where the leading corner arrives), rad (start angle of the pitch hinge in swing)
  double impact_mass = 2000.0, impact_speed = 30.0;
  double pd_freq = 2.0, pd_zeta = 1.0, torque_factor = 5.0;  // PD: kp = I_eff (2 pi f)^2, kd = 2 zeta sqrt(kp I_eff); torque cap = factor * static gravity torque (with a floor)
  double gravity = 9.81;
};
bool build_limb(const LimbRequest& rq, LimbSpec& out, std::string& report);
}  // namespace artic
