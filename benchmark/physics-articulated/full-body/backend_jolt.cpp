// Full-body test, Jolt backend: humanoid as maximal-coordinate bodies + HingeConstraint (PD motor) / FixedConstraint, debris as separate dynamic bodies.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
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
static constexpr ObjectLayer NON_MOVING = 0, HUMANOID = 1, DEBRIS = 2, PROJ = 3, NUM = 4;
}
namespace BPLayers {
static constexpr BroadPhaseLayer NON_MOVING(0), HUMANOID(1), DEBRIS(2), PROJ(3);
static constexpr uint NUM = 4;
}  // namespace BPLayers

class ObjVsObj : public ObjectLayerPairFilter {
 public:
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
    if (a == Layers::NON_MOVING && b == Layers::NON_MOVING) return false;
    if (a == Layers::HUMANOID && b == Layers::HUMANOID) return false;  // humanoid links never collide with each other
    return true;
  }
};
class BPLayerIf : public BroadPhaseLayerInterface {
 public:
  uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM; }
  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(static_cast<BroadPhaseLayer::Type>(l)); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "L"; }
#endif
};
class ObjVsBP : public ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(ObjectLayer l, BroadPhaseLayer b) const override {
    if (l == Layers::NON_MOVING) return b != BPLayers::NON_MOVING;
    if (l == Layers::HUMANOID) return b != BPLayers::HUMANOID;
    return true;
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

Vec3 V3d(const double* d) { return Vec3(float(d[0]), float(d[1]), float(d[2])); }

class JoltBackend final : public artic_body::Backend {
 public:
  JoltBackend() { global_init(); }
  ~JoltBackend() override { teardown(); }

  std::string name() const override {
    char b[300];
    std::snprintf(b, sizeof b, "jolt-%d.%d.%d cpu float32 bodies+HingeConstraint(motor PD)+FixedConstraint solver-iters vel=%d pos=%d ccd(humanoid,proj)=%s debris-ccd=off threads=1 sleeping(debris)=on", JPH_VERSION_MAJOR,
                  JPH_VERSION_MINOR, JPH_VERSION_PATCH, vel_, pos_, ccd_ ? "LinearCast" : "off");
    return b;
  }

  void build(const artic_body::Scene& sc) override {
    teardown();
    ccd_ = sc.ccd;
    const artic_body::Humanoid& M = sc.humanoid;
    temp_ = std::make_unique<TempAllocatorImpl>(256u * 1024 * 1024);
    jobs_ = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
    sys_ = std::make_unique<PhysicsSystem>();
    sys_->Init(4096, 0, 65536, 65536, bpif_, objvsbp_, objvsobj_);
    sys_->SetGravity(Vec3(0, 0, -float(sc.gravity)));
    PhysicsSettings ps = sys_->GetPhysicsSettings();
    ps.mAllowSleeping = true;  // debris may sleep; humanoid bodies are created with mAllowSleeping = false
    if (sc.vel_iters >= 0) ps.mNumVelocitySteps = static_cast<uint>(sc.vel_iters);
    if (sc.pos_iters >= 0) ps.mNumPositionSteps = static_cast<uint>(sc.pos_iters);
    sys_->SetPhysicsSettings(ps);
    vel_ = static_cast<int>(ps.mNumVelocitySteps);
    pos_ = static_cast<int>(ps.mNumPositionSteps);
    BodyInterface& bi = sys_->GetBodyInterface();
    {  // ground: top face at z = 0
      BodyCreationSettings gs(new BoxShape(Vec3(float(sc.ground_half), float(sc.ground_half), 5.0f), 0.01f), RVec3(0, 0, -5.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
      gs.mFriction = 0.5f; gs.mRestitution = 0.0f;
      ground_ = bi.CreateAndAddBody(gs, EActivation::DontActivate);
    }
    if (sc.has_wall) {
      BodyCreationSettings ws(new BoxShape(V3d(sc.wall.half), 0.01f), RVec3(sc.wall.c[0], sc.wall.c[1], sc.wall.c[2]),
                              Quat(float(sc.wall.q[0]), float(sc.wall.q[1]), float(sc.wall.q[2]), float(sc.wall.q[3])), EMotionType::Static, Layers::NON_MOVING);
      ws.mFriction = 0.5f; ws.mRestitution = 0.0f;
      wall_ = bi.CreateAndAddBody(ws, EActivation::DontActivate);
    }
    humanoid_.clear();
    for (const artic_body::Body& b : M.b) {
      const float mh = float(std::min(std::min(b.half[0], b.half[1]), b.half[2]));
      BodyCreationSettings bs(new BoxShape(V3d(b.half), std::min(0.05f, 0.25f * mh)), RVec3(b.c[0], b.c[1], b.c[2]), Quat(float(b.q[0]), float(b.q[1]), float(b.q[2]), float(b.q[3])), EMotionType::Dynamic,
                              Layers::HUMANOID);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(b.mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3d(b.v); bs.mAngularVelocity = V3d(b.w);
      bs.mMotionQuality = sc.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      humanoid_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    hinge_.assign(M.b.size(), nullptr);
    const BodyLockInterfaceNoLock& li = sys_->GetBodyLockInterfaceNoLock();
    for (size_t i = 1; i < M.b.size(); ++i) {
      const artic_body::Body& b = M.b[i];
      Body* child = li.TryGetBody(humanoid_[i]);
      Body* par = li.TryGetBody(humanoid_[static_cast<size_t>(b.parent)]);
      Constraint* c = nullptr;
      if (b.jtype == 2) {
        FixedConstraintSettings fs;
        fs.mSpace = EConstraintSpace::WorldSpace;
        fs.mAutoDetectPoint = true;
        c = fs.Create(*par, *child);
      } else {
        HingeConstraintSettings hs;
        hs.mSpace = EConstraintSpace::WorldSpace;
        hs.mPoint1 = hs.mPoint2 = RVec3(b.anchor[0], b.anchor[1], b.anchor[2]);
        const Vec3 ax = V3d(b.axis).Normalized();
        hs.mHingeAxis1 = hs.mHingeAxis2 = ax;
        hs.mNormalAxis1 = hs.mNormalAxis2 = ax.GetNormalizedPerpendicular();
        hs.mLimitsMin = float(b.lo); hs.mLimitsMax = float(b.hi);
        HingeConstraint* hc = static_cast<HingeConstraint*>(hs.Create(*par, *child));
        if (b.drive) {
          MotorSettings& ms = hc->GetMotorSettings();
          ms.mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(b.kp), float(b.kd));
          ms.SetTorqueLimit(float(b.max_torque));
          hc->SetMotorState(EMotorState::Position);
          hc->SetTargetAngle(0.0f);
        }
        hinge_[i] = hc;
        c = hc;
      }
      sys_->AddConstraint(c);
      cons_.push_back(c);
    }
    deb_.clear();
    for (const artic_body::DebrisBody& d : sc.debris) {
      BodyCreationSettings bs(new BoxShape(V3d(d.half), std::min(0.02f, 0.25f * float(std::min(std::min(d.half[0], d.half[1]), d.half[2])))), RVec3(d.c[0], d.c[1], d.c[2]),
                              Quat(float(d.q[0]), float(d.q[1]), float(d.q[2]), float(d.q[3])), EMotionType::Dynamic, Layers::DEBRIS);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(d.mass);
      bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mMotionQuality = EMotionQuality::Discrete;
      deb_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    if (sc.has_proj) {
      BodyCreationSettings bs(new BoxShape(V3d(sc.proj.half), 0.05f), RVec3(sc.proj.c[0], sc.proj.c[1], sc.proj.c[2]), Quat::sIdentity(), EMotionType::Dynamic, Layers::PROJ);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(sc.proj.mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3d(sc.proj.v);
      bs.mMotionQuality = sc.ccd ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
      proj_ = bi.CreateAndAddBody(bs, EActivation::Activate);
    }
    sys_->OptimizeBroadPhase();
  }

  void set_targets(const std::vector<double>& ang) override {
    for (size_t i = 1; i < hinge_.size() && i < ang.size(); ++i)
      if (hinge_[i] && hinge_[i]->GetMotorState() == EMotorState::Position) hinge_[i]->SetTargetAngle(float(ang[i]));
  }
  void step(float dt) override { sys_->Update(dt, 1, temp_.get(), jobs_.get()); }

  artic_body::State get(BodyID id) const {
    artic_body::State s{};
    const BodyInterface& bi = sys_->GetBodyInterface();
    const RVec3 p = bi.GetCenterOfMassPosition(id);
    const Quat q = bi.GetRotation(id);
    const Vec3 v = bi.GetLinearVelocity(id), w = bi.GetAngularVelocity(id);
    s.p[0] = float(p.GetX()); s.p[1] = float(p.GetY()); s.p[2] = float(p.GetZ());
    s.q[0] = q.GetX(); s.q[1] = q.GetY(); s.q[2] = q.GetZ(); s.q[3] = q.GetW();
    s.v[0] = v.GetX(); s.v[1] = v.GetY(); s.v[2] = v.GetZ();
    s.w[0] = w.GetX(); s.w[1] = w.GetY(); s.w[2] = w.GetZ();
    return s;
  }
  artic_body::State humanoid_body(int i) const override { return get(humanoid_[static_cast<size_t>(i)]); }
  artic_body::State debris_body(int i) const override { return get(deb_[static_cast<size_t>(i)]); }
  artic_body::State proj_body() const override { return get(proj_); }
  bool debris_awake(int i) const override { return sys_->GetBodyInterface().IsActive(deb_[static_cast<size_t>(i)]); }

 private:
  void teardown() {
    if (!sys_) return;
    BodyInterface& bi = sys_->GetBodyInterface();
    for (Constraint* c : cons_) sys_->RemoveConstraint(c);
    cons_.clear();
    hinge_.clear();
    auto kill = [&](BodyID id) { if (!id.IsInvalid()) { bi.RemoveBody(id); bi.DestroyBody(id); } };
    for (BodyID id : humanoid_) kill(id);
    for (BodyID id : deb_) kill(id);
    humanoid_.clear();
    deb_.clear();
    kill(proj_); kill(wall_); kill(ground_);
    proj_ = wall_ = ground_ = BodyID();
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
  std::vector<BodyID> humanoid_, deb_;
  std::vector<Ref<Constraint>> cons_;
  std::vector<HingeConstraint*> hinge_;
  BodyID ground_, wall_, proj_;
  int vel_ = 0, pos_ = 0;
  bool ccd_ = true;
};

}  // namespace

namespace artic_body {
std::unique_ptr<Backend> make_jolt() { return std::make_unique<JoltBackend>(); }
}  // namespace artic_body
