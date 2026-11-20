#pragma once
// Hip-twist test: shared description of one limb (ball joint + 2 hinges) and the backend interface. Backends build the SAME Spec.
// Chain: world(fixed) -ball(3 DOF)- body0 (thigh / upper arm) -hinge0- body1 (shank / forearm) -hinge1- body2 (foot / hand).
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "backend.h"  // from ../chain-limb: artic::LinkState

namespace artic_hip {

struct Vd { double x = 0, y = 0, z = 0; };
struct Qd { double x = 0, y = 0, z = 0, w = 1; };
inline Vd operator+(Vd a, Vd b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vd operator-(Vd a, Vd b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vd operator*(Vd a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(Vd a, Vd b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vd cross(Vd a, Vd b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double norm(Vd a) { return std::sqrt(dot(a, a)); }
inline Vd unit(Vd a) { const double l = norm(a); return l > 0 ? a * (1.0 / l) : a; }
inline Qd operator*(Qd a, Qd b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Qd conj(Qd q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Qd normalize(Qd q) { const double l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); return {q.x / l, q.y / l, q.z / l, q.w / l}; }
inline Vd rot(Qd q, Vd v) { const Vd u{q.x, q.y, q.z}; const Vd t = cross(u, v) * 2.0; return v + t * q.w + cross(u, t); }
inline Qd axis_angle(Vd ax, double a) { const Vd u = unit(ax) * std::sin(0.5 * a); return {u.x, u.y, u.z, std::cos(0.5 * a)}; }
inline Qd qd(const double q[4]) { return {q[0], q[1], q[2], q[3]}; }
inline Vd vd(const double v[3]) { return {v[0], v[1], v[2]}; }
inline void put(double d[3], Vd v) { d[0] = v.x; d[1] = v.y; d[2] = v.z; }
inline void putq(double d[4], Qd q) { d[0] = q.x; d[1] = q.y; d[2] = q.z; d[3] = q.w; }
// rotation vector (exp map) of a quaternion, shortest path, angle in [0, pi]
inline Vd rotvec(Qd q) {
  if (q.w < 0) q = {-q.x, -q.y, -q.z, -q.w};
  const double s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
  if (s < 1e-12) return {2 * q.x, 2 * q.y, 2 * q.z};
  const double a = 2.0 * std::atan2(s, q.w);
  return {q.x / s * a, q.y / s * a, q.z / s * a};
}
inline Qd from_rotvec(Vd v) { const double a = norm(v); return a < 1e-12 ? Qd{v.x * 0.5, v.y * 0.5, v.z * 0.5, 1.0} : axis_angle(v, a); }
// Swing-twist split of a rotation given in the joint frame (twist about frame x): q = swing * twist
struct SwingTwist { double twist; Vd swing; };  // twist angle (rad, -pi..pi), swing rotation vector (components about frame y and z; x = 0)
inline SwingTwist split(Qd q) {
  Qd tw{q.x, 0, 0, q.w};
  const double l = std::sqrt(tw.x * tw.x + tw.w * tw.w);
  if (l < 1e-12) tw = {1, 0, 0, 0}; else tw = {tw.x / l, 0, 0, tw.w / l};
  double ang = 2.0 * std::atan2(tw.x, tw.w);
  while (ang > 3.14159265358979323846) ang -= 2 * 3.14159265358979323846;
  while (ang < -3.14159265358979323846) ang += 2 * 3.14159265358979323846;
  return {ang, rotvec(q * conj(tw))};
}

struct Body { double c[3], q[4], half[3], mass, v[3], w[3]; };  // world, start pose; local x = long axis
struct Hinge { double anchor[3], axis[3], lo, hi, kp, kd, cap; bool drive; };  // world, start pose; lo/hi and drive target 0 are relative to the start pose
struct Ball {
  double anchor[3];            // world, fixed
  double F[4];                 // world frame at the NEUTRAL pose: x = twist axis (limb long axis), y = pitch axis, z = x cross y
  double tw_lim, sw1_lim, sw2_lim;  // twist +-tw_lim (about frame x); swing limits about frame y (pitch) and frame z (abduction)
  double kp[3], kd[3], cap[3]; // PD per axis: twist, swing1 (about y), swing2 (about z)
  bool drive_twist, drive_swing;
  double target[4];            // world rotation (relative to the neutral pose) the drives aim at
  double start[4];             // world rotation of the start pose relative to the neutral pose
};
struct Spec {
  Body body[3];
  Hinge hinge[2];
  Ball ball;
  double tip[3];
  double gravity = 9.81;
  bool wall = false;
  double wall_c[3] = {0, 0, 0}, wall_q[4] = {0, 0, 0, 1}, wall_half[3] = {0, 0, 0}, wall_n[3] = {0, 0, 0};
  bool proj = false;
  double proj_c[3] = {0, 0, 0}, proj_half[3] = {0, 0, 0}, proj_mass = 0, proj_v[3] = {0, 0, 0};
  double launch_omega = 0;     // rad/s about frame-y (pitch) axis, rigid rotation of the whole limb (0 = none)
  std::string label;
};

class Backend {
 public:
  virtual ~Backend() = default;
  virtual std::string name() const = 0;
  virtual bool build(const Spec& s, double hz, std::string& note) = 0;  // false: this variant cannot build the case (note says why)
  virtual void step(float dt) = 0;
  virtual artic::LinkState link(int i) const = 0;  // 0..2 limb bodies, 3 = projectile
};
std::unique_ptr<Backend> make_jolt_swingtwist();   // null without Jolt
std::unique_ptr<Backend> make_jolt_sixdof();       // null without Jolt
std::unique_ptr<Backend> make_physx_art();         // null without PhysX: articulation, spherical joint
std::unique_ptr<Backend> make_physx_d6();          // null without PhysX: maximal coordinates, PxD6Joint

}  // namespace artic_hip
