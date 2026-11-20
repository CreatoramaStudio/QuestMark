// Standing-foot test, Jolt backend: maximal-coordinate bodies + HingeConstraint (position motor = PD), pelvis free, static ground box.
// Layer set-up and body/constraint settings follow the chain-limb test (backend_jolt.cpp, limb scenario).
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
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

#include "stand.h"

using namespace JPH;

namespace {

namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer PROJ = 2;
}  // namespace Layers
namespace BPLayers {
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr BroadPhaseLayer PROJ(2);
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
    if (l == Layers::PROJ) return b != BPLayers::PROJ;
    if (l == Layers::MOVING) return b == BPLayers::NON_MOVING || b == BPLayers::PROJ;
    return b != BPLayers::NON_MOVING;
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

class JoltBackend final : public artic_foot::Backend {
 public:
  JoltBackend() { global_init(); }
  ~JoltBackend() override { teardown(); }

  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "jolt-%d.%d.%d cpu float32 hinge-constraints position-motor solver-iters vel=%d pos=%d ccd=%s threads=1", JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH, vel_,
                  pos_, ccd_ ? "LinearCast" : "off");
    return b;
  }

  void build(const artic_foot::Spec& s) override {
    teardown();
    ccd_ = s.ccd;
    temp_ = std::make_unique<TempAllocatorImpl>(64 * 1024 * 1024);
    jobs_ = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
    sys_ = std::make_unique<PhysicsSystem>();
    sys_->Init(64, 0, 1024, 1024, bpif_, objvsbp_, objvsobj_);
    sys_->SetGravity(Vec3(0, 0, -float(s.gravity)));
    PhysicsSettings ps = sys_->GetPhysicsSettings();
    if (s.vel_iters >= 0) ps.mNumVelocitySteps = uint(s.vel_iters);
    if (s.pos_iters >= 0) ps.mNumPositionSteps = uint(s.pos_iters);
    ps.mAllowSleeping = false;
    sys_->SetPhysicsSettings(ps);
    vel_ = int(ps.mNumVelocitySteps);
    pos_ = int(ps.mNumPositionSteps);
    BodyInterface& bi = sys_->GetBodyInterface();
    auto V3f = [](const double* d) { return Vec3(float(d[0]), float(d[1]), float(d[2])); };
    {
      BodyCreationSettings gs(new BoxShape(V3f(s.ground_half), 0.05f), RVec3(s.ground_c[0], s.ground_c[1], s.ground_c[2]), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
      gs.mFriction = float(s.friction); gs.mRestitution = 0.0f;
      ground_ = bi.CreateAndAddBody(gs, EActivation::DontActivate);
    }
    ids_.clear();
    for (const artic_foot::Body& b : s.body) {
      const float mh = float(std::min(std::min(b.half[0], b.half[1]), b.half[2]));
      BodyCreationSettings bs(new BoxShape(V3f(b.half), std::min(0.05f, 0.25f * mh)), RVec3(b.c[0], b.c[1], b.c[2]), Quat(float(b.q[0]), float(b.q[1]), float(b.q[2]), float(b.q[3])),
                              EMotionType::Dynamic, Layers::MOVING);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(b.mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = float(s.friction); bs.mRestitution = 0.0f;
      bs.mMotionQuality = s.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    const BodyLockInterfaceNoLock& li = sys_->GetBodyLockInterfaceNoLock();
    cons_.clear();
    for (const artic_foot::Hinge& h : s.hinge) {
      HingeConstraintSettings hs;
      hs.mSpace = EConstraintSpace::WorldSpace;
      hs.mPoint1 = hs.mPoint2 = RVec3(h.anchor[0], h.anchor[1], h.anchor[2]);
      const Vec3 ax = V3f(h.axis).Normalized();
      hs.mHingeAxis1 = hs.mHingeAxis2 = ax;
      hs.mNormalAxis1 = hs.mNormalAxis2 = ax.GetNormalizedPerpendicular();
      hs.mLimitsMin = float(h.lo); hs.mLimitsMax = float(h.hi);
      Body* par = li.TryGetBody(ids_[size_t(h.parent)]);
      Body* child = li.TryGetBody(ids_[size_t(h.child)]);
      HingeConstraint* c = static_cast<HingeConstraint*>(hs.Create(*par, *child));
      MotorSettings& ms = c->GetMotorSettings();
      ms.mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(h.kp), float(h.kd));
      ms.SetTorqueLimit(float(h.max_torque));
      c->SetMotorState(EMotorState::Position);
      c->SetTargetAngle(0.0f);
      sys_->AddConstraint(c);
      cons_.push_back(c);
    }
    if (s.proj) {
      BodyCreationSettings bs(new BoxShape(V3f(s.proj_half), 0.05f), RVec3(s.proj_c[0], s.proj_c[1], s.proj_c[2]), Quat::sIdentity(), EMotionType::Dynamic, Layers::PROJ);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(s.proj_mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = float(s.friction); bs.mRestitution = 0.0f;
      bs.mGravityFactor = 0.0f;  // flies straight (see README)
      bs.mLinearVelocity = V3f(s.proj_v);
      bs.mMotionQuality = s.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    sys_->OptimizeBroadPhase();
  }

  void step(float dt) override { sys_->Update(dt, 1, temp_.get(), jobs_.get()); }

  void drive_target(int hinge, double angle) override { static_cast<HingeConstraint*>(cons_[size_t(hinge)].GetPtr())->SetTargetAngle(float(angle)); }

  artic_foot::State body(int i) const override {
    artic_foot::State s{};
    const BodyInterface& bi = sys_->GetBodyInterface();
    const BodyID id = ids_[size_t(i)];
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
    if (!ground_.IsInvalid()) { bi.RemoveBody(ground_); bi.DestroyBody(ground_); ground_ = BodyID(); }
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
  BodyID ground_;
  int vel_ = 0, pos_ = 0;
  bool ccd_ = true;
};

}  // namespace

namespace artic_foot {
std::unique_ptr<Backend> make_jolt() { return std::make_unique<JoltBackend>(); }
}  // namespace artic_foot
