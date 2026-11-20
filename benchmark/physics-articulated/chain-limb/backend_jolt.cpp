// Chain-limb test, Jolt backend (maximal-coordinate bodies + HingeConstraint, solved by Jolt's iterative solver).
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>

#include "backend.h"

using namespace JPH;
using namespace JPH::literals;

namespace {

namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer PROJ = 2;  // limb scenario impact body: collides with the links and the wall
static constexpr ObjectLayer NUM = 3;
}  // namespace Layers
namespace BPLayers {
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr BroadPhaseLayer PROJ(2);  // own broad-phase layer so the layer filters stay symmetric (Jolt reports a dynamic pair from the lower body id only)
static constexpr uint NUM = 3;
}  // namespace BPLayers

class ObjVsObj : public ObjectLayerPairFilter {
 public:
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != b; }  // links never collide with each other
};
class BPLayerIf : public BroadPhaseLayerInterface {
 public:
  uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM; }
  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return l == Layers::NON_MOVING ? BPLayers::NON_MOVING : l == Layers::PROJ ? BPLayers::PROJ : BPLayers::MOVING; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "L"; }
#endif
};
class ObjVsBP : public ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(ObjectLayer l, BroadPhaseLayer b) const override {
    if (l == Layers::PROJ) return b != BPLayers::PROJ;                       // projectile: wall and links
    if (l == Layers::MOVING) return b == BPLayers::NON_MOVING || b == BPLayers::PROJ;  // links: wall and projectile, never each other
    return b != BPLayers::NON_MOVING;                                        // wall: links and projectile
  }
};

void trace(const char* fmt, ...) {
  va_list a;
  va_start(a, fmt);
  char buf[1024];
  std::vsnprintf(buf, sizeof buf, fmt, a);
  va_end(a);
  std::fprintf(stderr, "[jolt] %s\n", buf);
}

void global_init() {
  static bool done = false;
  if (done) return;
  done = true;
  RegisterDefaultAllocator();
  Trace = trace;
  Factory::sInstance = new Factory();
  RegisterTypes();
}

class JoltBackend final : public artic::Backend {
 public:
  JoltBackend() { global_init(); }
  ~JoltBackend() override { teardown(); }

  std::string name() const override {
    char b[512];
    std::snprintf(b, sizeof b,
                  "jolt-%d.%d.%d cpu float32 hinge-constraints solver-iters vel=%d pos=%d ccd=%s threads=%d axis=%s prio=%d heavy-body1=%d inertia-mult-light=%g max-angvel=%g light-half-thick=%g",
                  JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH, vel_, pos_, ccd_ ? "LinearCast" : "off", threads_, axis_.c_str(), int(prio_), int(heavy1_),
                  double(imult_), double(maxw_), double(lht_));
    return b;
  }

  // ---- limb scenario (--scenario limb): bodies and hinges from p.limb
  void build_limb(const artic::Params& p) {
    const artic::LimbSpec& L = *p.limb;
    links_ = static_cast<int>(L.body.size());
    axis_ = "limb"; threads_ = p.threads; ccd_ = p.ccd; prio_ = false; heavy1_ = false; imult_ = 1.0f; maxw_ = -1.0f; lht_ = -1.0f;
    temp_ = std::make_unique<TempAllocatorImpl>(64 * 1024 * 1024);
    if (p.threads <= 1) jobs_ = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
    else jobs_ = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, p.threads - 1);
    sys_ = std::make_unique<PhysicsSystem>();
    sys_->Init(64, 0, 1024, 1024, bpif_, objvsbp_, objvsobj_);
    sys_->SetGravity(Vec3(0, 0, -p.gravity));
    PhysicsSettings ps = sys_->GetPhysicsSettings();
    if (p.vel_iters >= 0) ps.mNumVelocitySteps = static_cast<uint>(p.vel_iters);
    if (p.pos_iters >= 0) ps.mNumPositionSteps = static_cast<uint>(p.pos_iters);
    ps.mAllowSleeping = false;
    sys_->SetPhysicsSettings(ps);
    vel_ = static_cast<int>(ps.mNumVelocitySteps);
    pos_ = static_cast<int>(ps.mNumPositionSteps);
    BodyInterface& bi = sys_->GetBodyInterface();
    auto V3f = [](const double* d) { return Vec3(float(d[0]), float(d[1]), float(d[2])); };
    if (L.wall) {
      BodyCreationSettings ws(new BoxShape(V3f(L.wall_half), 0.01f), RVec3(L.wall_c[0], L.wall_c[1], L.wall_c[2]),
                              Quat(float(L.wall_q[0]), float(L.wall_q[1]), float(L.wall_q[2]), float(L.wall_q[3])), EMotionType::Static, Layers::NON_MOVING);
      ws.mFriction = 0.5f; ws.mRestitution = 0.0f;
      wall_ = bi.CreateAndAddBody(ws, EActivation::DontActivate);
    }
    ids_.clear();
    for (const artic::LimbBody& b : L.body) {
      const float mh = float(std::min(std::min(b.half[0], b.half[1]), b.half[2]));
      BodyCreationSettings bs(new BoxShape(V3f(b.half), std::min(0.05f, 0.25f * mh)), RVec3(b.c[0], b.c[1], b.c[2]), Quat(float(b.q[0]), float(b.q[1]), float(b.q[2]), float(b.q[3])),
                              EMotionType::Dynamic, Layers::MOVING);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(b.mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3f(b.v); bs.mAngularVelocity = V3f(b.w);
      bs.mMotionQuality = p.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    for (size_t i = 0; i < L.hinge.size(); ++i) {
      const artic::LimbHinge& h = L.hinge[i];
      HingeConstraintSettings hs;
      hs.mSpace = EConstraintSpace::WorldSpace;
      hs.mPoint1 = hs.mPoint2 = RVec3(h.anchor[0], h.anchor[1], h.anchor[2]);
      const Vec3 ax = V3f(h.axis).Normalized();
      hs.mHingeAxis1 = hs.mHingeAxis2 = ax;
      hs.mNormalAxis1 = hs.mNormalAxis2 = ax.GetNormalizedPerpendicular();
      hs.mLimitsMin = float(h.lo); hs.mLimitsMax = float(h.hi);
      const BodyLockInterfaceNoLock& li = sys_->GetBodyLockInterfaceNoLock();
      Body* child = li.TryGetBody(ids_[i]);
      Body* par = (i == 0) ? &Body::sFixedToWorld : li.TryGetBody(ids_[i - 1]);
      HingeConstraint* c = static_cast<HingeConstraint*>(hs.Create(*par, *child));
      if (h.drive) {
        MotorSettings& ms = c->GetMotorSettings();
        ms.mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(h.kp), float(h.kd));
        ms.SetTorqueLimit(float(h.max_torque));
        c->SetMotorState(EMotorState::Position);
        c->SetTargetAngle(0.0f);
      }
      sys_->AddConstraint(c);
      cons_.push_back(c);
    }
    if (L.proj) {
      BodyCreationSettings bs(new BoxShape(V3f(L.proj_half), 0.05f), RVec3(L.proj_c[0], L.proj_c[1], L.proj_c[2]), Quat::sIdentity(), EMotionType::Dynamic, Layers::PROJ);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(L.proj_mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3f(L.proj_v);
      bs.mMotionQuality = p.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    sys_->OptimizeBroadPhase();
  }

  void build(const artic::Params& p) override {
    teardown();
    if (p.limb) { build_limb(p); return; }
    links_ = p.links;
    axis_ = p.axis; prio_ = p.jolt_prio; heavy1_ = p.jolt_heavy_body1; imult_ = p.jolt_inertia_mult; maxw_ = p.jolt_max_angvel; lht_ = p.light_half_thick;
    threads_ = p.threads;
    ccd_ = p.ccd;
    temp_ = std::make_unique<TempAllocatorImpl>(16 * 1024 * 1024);
    if (p.threads <= 1)
      jobs_ = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
    else
      jobs_ = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, p.threads - 1);
    sys_ = std::make_unique<PhysicsSystem>();
    sys_->Init(64, 0, 1024, 1024, bpif_, objvsbp_, objvsobj_);
    sys_->SetGravity(Vec3(0, 0, -p.gravity));
    PhysicsSettings ps = sys_->GetPhysicsSettings();
    if (p.vel_iters >= 0) ps.mNumVelocitySteps = static_cast<uint>(p.vel_iters);
    if (p.pos_iters >= 0) ps.mNumPositionSteps = static_cast<uint>(p.pos_iters);
    ps.mAllowSleeping = false;
    sys_->SetPhysicsSettings(ps);
    vel_ = static_cast<int>(ps.mNumVelocitySteps);
    pos_ = static_cast<int>(ps.mNumPositionSteps);

    BodyInterface& bi = sys_->GetBodyInterface();
    // wall: centre of a slab spanning x -1..26, y wall_y..wall_y+2*half_thick, z -10..10
    {
      Vec3 he(13.5f, p.wall_half_thick, 10.0f);
      BodyCreationSettings ws(new BoxShape(he, std::min(0.05f, 0.5f * p.wall_half_thick)), RVec3(12.5_r, p.wall_y + p.wall_half_thick, 0.0_r),
                              Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
      ws.mFriction = 0.5f;
      ws.mRestitution = 0.0f;
      wall_ = bi.CreateAndAddBody(ws, EActivation::DontActivate);
    }
    const float omega = p.tip_speed / (2.0f * p.link_half_len * static_cast<float>(p.links));
    ids_.clear();
    for (int i = 0; i < p.links; ++i) {
      const float h = p.half_thick[static_cast<size_t>(i)];
      const float cx = p.link_half_len * (2.0f * static_cast<float>(i) + 1.0f);
      BodyCreationSettings bs(new BoxShape(Vec3(p.link_half_len, h, h), std::min(0.05f, 0.5f * h)), RVec3(cx, 0.0f, 0.0f), Quat::sIdentity(),
                              EMotionType::Dynamic, Layers::MOVING);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = p.mass[static_cast<size_t>(i)];
      bs.mLinearDamping = 0.0f;
      bs.mAngularDamping = 0.0f;
      bs.mAllowSleeping = false;
      bs.mFriction = 0.5f;
      bs.mRestitution = 0.0f;
      float lv[3], ax[3];
      artic::launch_lin(p, omega, cx, lv);
      artic::axis_vec(p, ax);
      bs.mLinearVelocity = Vec3(lv[0], lv[1], lv[2]);
      bs.mAngularVelocity = Vec3(ax[0] * omega, ax[1] * omega, ax[2] * omega);
      if (p.mass[static_cast<size_t>(i)] < p.mass_max) bs.mInertiaMultiplier = p.jolt_inertia_mult;
      if (p.jolt_max_angvel > 0.0f) bs.mMaxAngularVelocity = p.jolt_max_angvel;
      bs.mMotionQuality = p.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    static bool printed_inertia = false;  // once per process: the inertia Jolt really uses (principal diagonal, kg m^2), local x = long axis
    if (!printed_inertia) {
      printed_inertia = true;
      const BodyLockInterfaceNoLock& li0 = sys_->GetBodyLockInterfaceNoLock();
      for (int i = 0; i < std::min(p.links, 2); ++i) {
        const Vec3 inv = li0.TryGetBody(ids_[static_cast<size_t>(i)])->GetMotionProperties()->GetInverseInertiaDiagonal();
        std::fprintf(stderr, "[jolt] link %d mass %g kg: inertia diag (long x, y, z) = %g %g %g kg m^2\n", i, double(p.mass[static_cast<size_t>(i)]),
                     double(1.0f / inv.GetX()), double(1.0f / inv.GetY()), double(1.0f / inv.GetZ()));
      }
    }
    for (int i = 0; i < p.links; ++i) {
      HingeConstraintSettings hs;
      hs.mSpace = EConstraintSpace::WorldSpace;
      const float ax = 2.0f * p.link_half_len * static_cast<float>(i);  // anchor at the shared end face
      hs.mPoint1 = hs.mPoint2 = RVec3(ax, 0.0f, 0.0f);
      float hax[3];
      artic::axis_vec(p, hax);
      hs.mHingeAxis1 = hs.mHingeAxis2 = Vec3(hax[0], hax[1], hax[2]);
      if (p.jolt_prio) hs.mConstraintPriority = static_cast<uint32>(100 - i);
      hs.mNormalAxis1 = hs.mNormalAxis2 = Vec3(1, 0, 0);
      hs.mLimitsMin = -p.joint_limit;
      hs.mLimitsMax = p.joint_limit;
      Constraint* c;
      const BodyLockInterfaceNoLock& li = sys_->GetBodyLockInterfaceNoLock();  // single-threaded setup
      Body* child = li.TryGetBody(ids_[static_cast<size_t>(i)]);
      Body* par = (i == 0) ? &Body::sFixedToWorld : li.TryGetBody(ids_[static_cast<size_t>(i - 1)]);
      const bool swap = p.jolt_heavy_body1 && i > 0 && p.mass[static_cast<size_t>(i)] > p.mass[static_cast<size_t>(i - 1)];
      c = swap ? hs.Create(*child, *par) : hs.Create(*par, *child);
      sys_->AddConstraint(c);
      cons_.push_back(c);
    }
    sys_->OptimizeBroadPhase();
  }

  void step(float dt) override { sys_->Update(dt, 1, temp_.get(), jobs_.get()); }

  artic::LinkState link(int i) const override {
    artic::LinkState s{};
    const BodyInterface& bi = sys_->GetBodyInterface();
    const BodyID id = ids_[static_cast<size_t>(i)];
    const RVec3 p = bi.GetCenterOfMassPosition(id);
    const Quat q = bi.GetRotation(id);
    const Vec3 v = bi.GetLinearVelocity(id), w = bi.GetAngularVelocity(id);
    s.p[0] = float(p.GetX()); s.p[1] = float(p.GetY()); s.p[2] = float(p.GetZ());
    s.q[0] = q.GetX(); s.q[1] = q.GetY(); s.q[2] = q.GetZ(); s.q[3] = q.GetW();
    s.v[0] = v.GetX(); s.v[1] = v.GetY(); s.v[2] = v.GetZ();
    s.w[0] = w.GetX(); s.w[1] = w.GetY(); s.w[2] = w.GetZ();
    return s;
  }

 private:
  void teardown() {
    if (!sys_) return;
    BodyInterface& bi = sys_->GetBodyInterface();
    for (Constraint* c : cons_) sys_->RemoveConstraint(c);
    cons_.clear();
    for (BodyID id : ids_) { bi.RemoveBody(id); bi.DestroyBody(id); }
    ids_.clear();
    if (!wall_.IsInvalid()) { bi.RemoveBody(wall_); bi.DestroyBody(wall_); wall_ = BodyID(); }
    sys_.reset();
    jobs_.reset();
    temp_.reset();
  }

  BPLayerIf bpif_;
  ObjVsBP objvsbp_;
  ObjVsObj objvsobj_;
  std::unique_ptr<TempAllocatorImpl> temp_;
  std::unique_ptr<JobSystem> jobs_;
  std::unique_ptr<PhysicsSystem> sys_;
  std::vector<BodyID> ids_;
  std::vector<Ref<Constraint>> cons_;
  BodyID wall_;
  int links_ = 0, threads_ = 1, vel_ = 0, pos_ = 0;
  std::string axis_ = "z";
  bool prio_ = false, heavy1_ = false;
  float imult_ = 1.0f, maxw_ = -1.0f, lht_ = -1.0f;
  bool ccd_ = true;
};

}  // namespace

namespace artic {
std::unique_ptr<Backend> make_jolt() { return std::make_unique<JoltBackend>(); }
}  // namespace artic
