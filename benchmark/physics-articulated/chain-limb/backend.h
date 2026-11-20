#pragma once
// Chain-limb test: tiny shared interface for the two physics backends. Both build the same scenario from the same Params.
#include <memory>
#include <string>
#include <vector>

namespace artic {

// Limb scenario (--scenario limb): a chain of boxes derived from a humanoid skeleton, body i is the child of body i-1 (body 0 hangs from the fixed root).
// All values are world space at the start pose; hinge angles are measured relative to this start pose.
struct LimbBody { std::string name; double c[3], q[4], half[3], mass, v[3], w[3]; };  // q = x y z w, local x = long axis
struct LimbHinge { std::string name; double anchor[3], axis[3], lo, hi, kp, kd, max_torque; bool drive; };  // lo/hi relative to the start pose; drive: PD to angle 0
struct LimbSpec {
  std::vector<LimbBody> body;
  std::vector<LimbHinge> hinge;   // hinge[i] joins body i-1 (world for i = 0) to body i
  double tip[3] = {0, 0, 0};      // tip point (world), carried by the last body
  int launch_hinge = -1;          // hinge whose joint velocity is set (rigid rotation of everything below it), -1 = none
  double launch_omega = 0;        // rad/s about that hinge axis
  bool wall = false;              // thin slab: centre, orientation, half extents; n = unit normal pointing at the oncoming limb
  double wall_c[3] = {0, 0, 0}, wall_q[4] = {0, 0, 0, 1}, wall_half[3] = {0, 0, 0}, wall_n[3] = {0, 0, 0};
  bool proj = false;              // impact body (box) and its start velocity
  double proj_c[3] = {0, 0, 0}, proj_half[3] = {0, 0, 0}, proj_mass = 0, proj_v[3] = {0, 0, 0};
  std::string label;              // what was built (printed and written to json)
};

// World: +z up (gravity -z). Limb chain along +x from a pivot at the origin, swinging in the xy-plane.
// Hinge axis = world z for every joint. A thin wall (a box slab) stands at y >= wall_y, normal -y.
struct Params {
  int links = 12;
  float link_half_len = 1.0f;       // every link is a box with half extents (link_half_len, h_i, h_i)
  float mass_min = 50.0f;           // kg
  float mass_max = 50000.0f;        // kg
  std::string profile = "geometric";  // geometric: root heavy -> tip light; alternating: neighbour ratio mass_max:mass_min
  float tip_speed = 30.0f;          // m/s, speed of the end of the last link at launch (rigid rotation about the pivot)
  float wall_y = 2.4f;              // front face of the wall
  float wall_half_thick = 0.025f;   // 5 cm thick wall
  float joint_limit = 2.0f;         // rad, hinge limits +-joint_limit
  float gravity = 9.81f;
  double hz = 120.0;
  bool ccd = true;
  int threads = 1;                  // 1 = simulate on the calling thread only
  int vel_iters = -1, pos_iters = -1;  // <0: library default
  // Follow-up experiment switches. Every default is the old behaviour (baseline).
  std::string axis = "z";           // hinge axis: "z" (parallel to gravity, 5-axis locked load carries the weight) or "y" (perpendicular to gravity, chain swings in the xz-plane)
  float light_half_thick = -1.0f;   // >0: half thickness of the light links of the alternating profile (same mass, thicker box); both backends
  bool jolt_prio = false;           // Jolt: constraint priority 100 - i (the root hinge has the highest priority = solved last)
  bool jolt_heavy_body1 = false;    // Jolt: the heavier of the two bodies of a hinge is body 1 (the world stays body 1 of hinge 0)
  float jolt_inertia_mult = 1.0f;   // Jolt: mInertiaMultiplier on the light links (mass < mass_max)
  float jolt_max_angvel = -1.0f;    // Jolt: mMaxAngularVelocity, <0 = library default (0.25*pi*60 = 47.12 rad/s)
  bool physx_tgs = false;           // PhysX maximal-coordinate backend: TGS solver instead of the default PGS
  std::shared_ptr<const LimbSpec> limb;  // set: limb scenario (the chain parameters above are ignored); null: the old chain
  // derived by derive()
  std::vector<float> mass, half_thick;
};

struct LinkState {
  float p[3];  // centre of mass (= box centre)
  float q[4];  // x y z w
  float v[3];  // centre of mass linear velocity
  float w[3];  // angular velocity
};

class Backend {
 public:
  virtual ~Backend() = default;
  virtual std::string name() const = 0;       // what actually ran: library, version, settings
  virtual void build(const Params& p) = 0;    // (re)create the world from scratch
  virtual void step(float dt) = 0;
  virtual LinkState link(int i) const = 0;
};

void derive(Params& p);
// Hinge axis (unit) and the launch velocity of a link whose centre is at x = cx: rigid rotation omega about the axis through the pivot.
inline void axis_vec(const Params& p, float a[3]) { a[0] = 0; a[1] = p.axis == "y" ? 1.0f : 0.0f; a[2] = p.axis == "y" ? 0.0f : 1.0f; }
inline void launch_lin(const Params& p, float omega, float cx, float v[3]) {  // omega x (cx,0,0)
  v[0] = 0;
  v[1] = p.axis == "y" ? 0.0f : omega * cx;
  v[2] = p.axis == "y" ? -omega * cx : 0.0f;
}
std::unique_ptr<Backend> make_jolt();    // null if built without Jolt
std::unique_ptr<Backend> make_physx();   // null if built without PhysX (articulation, reduced coordinates)
std::unique_ptr<Backend> make_physx_maximal();  // null if built without PhysX (rigid dynamics + PxRevoluteJoint, maximal coordinates)

}  // namespace artic
