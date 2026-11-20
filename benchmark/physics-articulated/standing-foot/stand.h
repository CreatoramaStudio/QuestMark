#pragma once
// Standing-foot test: shared scene description and backend interface (Jolt, PhysX articulation, PhysX maximal coordinates).
// World: +z up, gravity -z. Body 0 = pelvis (free, NOT fixed to the world), then 4 bodies per leg (gimbal, thigh, shank, foot).
#include <memory>
#include <string>
#include <vector>

namespace artic_foot {

struct Body { std::string name; double c[3], q[4], half[3], mass; };  // q = x y z w
struct Hinge { std::string name; int parent, child; double anchor[3], axis[3], lo, hi, kp, kd, max_torque; };  // lo/hi relative to the start pose; PD to a target angle (start 0)

struct Spec {
  std::vector<Body> body;
  std::vector<Hinge> hinge;       // hinge i joins body parent to body child (every hinge is PD driven)
  std::vector<int> foot;          // body index of each foot
  std::vector<bool> foot_stance;  // true = foot meant to carry weight
  double ground_c[3] = {0, 0, 0}, ground_half[3] = {0, 0, 0}, ground_top = 0, friction = 0.8;
  bool proj = false;              // impact body (box), appended as the last body index (no hinge)
  double proj_c[3] = {0, 0, 0}, proj_half[3] = {0, 0, 0}, proj_mass = 0, proj_v[3] = {0, 0, 0};
  double gravity = 9.81;
  int vel_iters = -1, pos_iters = -1;  // <0: library default
  bool ccd = true;
  std::string label;
};

struct State { float p[3], q[4], v[3], w[3]; };

class Backend {
 public:
  virtual ~Backend() = default;
  virtual std::string name() const = 0;     // what actually ran
  virtual void build(const Spec& s) = 0;    // (re)create the world from scratch
  virtual void step(float dt) = 0;
  virtual State body(int i) const = 0;      // i == body.size(): the impact body
  virtual void drive_target(int hinge, double angle) = 0;
};

std::unique_ptr<Backend> make_jolt();
std::unique_ptr<Backend> make_physx();          // reduced-coordinate articulation, root = pelvis (floating base)
std::unique_ptr<Backend> make_physx_maximal();  // rigid dynamics + PxD6Joint (twist drive), maximal coordinates

}  // namespace artic_foot
