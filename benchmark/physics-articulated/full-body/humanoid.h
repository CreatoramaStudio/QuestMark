#pragma once
// Full-body test: whole humanoid (torso, head, two legs, two arms) derived from the humanoid skeleton, scaled to a size_label. See README.md.
#include <string>
#include <vector>

namespace artic_body {

// One rigid body of the humanoid tree. Bodies are stored parent-first; body 0 is the torso (free, no joint).
// Every other body has one inbound joint to `parent`: a revolute hinge (anchor, axis, limits relative to the start pose) or a fixed joint.
struct Body {
  std::string name;
  double c[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, half[3] = {1, 1, 1}, mass = 1;  // world space start pose, q = x y z w
  double v[3] = {0, 0, 0}, w[3] = {0, 0, 0};                                   // start velocities (case set-up)
  int parent = -1;
  int jtype = 0;  // 0 none (root), 1 revolute, 2 fixed
  double anchor[3] = {0, 0, 0}, axis[3] = {0, 0, 1}, lo = 0, hi = 0;
  bool drive = false;
  double kp = 0, kd = 0, max_torque = 0;
  double inertia_sub = 0, inertia_rest = 0, static_tq = 0;  // report only (about the hinge axis)
};

struct Request {
  std::string fbx, sizes_json, size_label;
  double ton_kg = 1000.0;
  double pd_freq = 2.0, pd_zeta = 1.0, torque_factor = 5.0;
  double stand_mult = 0.25;  // leg joints: effective inertia = max(reduced inertia, stand_mult * inertia of the rest of the humanoid about the axis)
  double leg_cap = 0.5;      // leg joint torque cap floor = leg_cap * M g * hip_height
  double gravity = 9.81;
  bool free_shoulder_pitch = false;  // wall case: left shoulder pitch not driven
};

struct Humanoid {
  std::vector<Body> b;
  std::string report;
  double total_mass = 0, height = 0, scale = 0, hip_height = 0;
  // indices (side 0 = left, +x; side 1 = right, -x)
  int torso = 0, head = 1;
  int hip_abd[2] = {-1, -1}, hip_pitch[2] = {-1, -1}, knee[2] = {-1, -1}, ankle_pitch[2] = {-1, -1}, ankle_roll[2] = {-1, -1}, foot[2] = {-1, -1};
  int sh_abd[2] = {-1, -1}, sh_pitch[2] = {-1, -1}, elbow[2] = {-1, -1}, wrist[2] = {-1, -1}, hand[2] = {-1, -1};
  double thigh0[2][2] = {{0, 0}, {0, 0}}, shank0[2][2] = {{0, 0}, {0, 0}};  // start vectors hip->knee and knee->ankle in the (y, z) plane, per leg
  double hip_x[2] = {0, 0};                                                   // lateral position of the hip pivots
  double foot_len = 0;                                                        // foot AABB length along y
  double arm_tip[2][3] = {{0, 0, 0}, {0, 0, 0}};                              // finger tip (world, start pose)
};

bool build_humanoid(const Request& rq, Humanoid& out);
int dump_skeleton(const std::string& fbx_path);

}  // namespace artic_body
