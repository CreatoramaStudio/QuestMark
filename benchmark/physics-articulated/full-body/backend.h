#pragma once
// Full-body test: tiny shared interface for the two physics backends (Jolt bodies + constraints, PhysX reduced-coordinate articulation).
#include <memory>
#include <string>
#include <vector>

#include "humanoid.h"

namespace artic_body {

struct DebrisBody { double c[3], q[4], half[3], mass; };
struct Wall { double c[3], q[4], half[3]; };       // static thin slab
struct Projectile { double c[3], half[3], mass, v[3]; };

struct Scene {
  Humanoid humanoid;
  double ground_half = 1000.0;  // ground = static box, top face at z = 0
  std::vector<DebrisBody> debris;
  bool has_wall = false;
  Wall wall{};
  bool has_proj = false;
  Projectile proj{};
  int launch_joint = -1;        // body index whose inbound hinge gets launch_omega (PhysX: joint velocity; Jolt: the bodies carry v/w already)
  double launch_omega = 0;
  double gravity = 9.81;
  double hz = 120.0;
  int vel_iters = -1, pos_iters = -1;  // <0: library default
  bool ccd = true;
};

struct State { float p[3]; float q[4]; float v[3]; float w[3]; };

class Backend {
 public:
  virtual ~Backend() = default;
  virtual std::string name() const = 0;
  virtual void build(const Scene& s) = 0;
  virtual void set_targets(const std::vector<double>& ang) = 0;  // PD target angle (rad, relative to the start pose) per humanoid body index; fixed/undriven joints ignored
  virtual void step(float dt) = 0;
  virtual State humanoid_body(int i) const = 0;
  virtual State debris_body(int i) const = 0;
  virtual State proj_body() const = 0;
  virtual bool debris_awake(int i) const = 0;
  virtual std::string notes() const { return ""; }  // backend-specific facts found while building (errors, limits)
};

std::unique_ptr<Backend> make_jolt();    // null if built without Jolt
std::unique_ptr<Backend> make_physx();   // null if built without PhysX
std::string physx_probe_limits();        // "" if built without PhysX: builds articulations with growing link counts and reports what the library did

}  // namespace artic_body
