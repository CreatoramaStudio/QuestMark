// Hip-twist test, Jolt backends: the hip/shoulder is a real 3-DOF joint between the fixed world and the thigh/upper arm.
//   variant "jolt-swingtwist": SwingTwistConstraint (cone swing limit, twist limit, swing motor + twist motor)
//   variant "jolt-sixdof":     SixDOFConstraint (translations fixed, 3 rotation axes limited, one motor per rotation axis)
// Knee and ankle (or elbow and wrist) are HingeConstraints with a position motor, as in the chain-limb test.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>

#include "hip.h"

using namespace JPH;

namespace {

namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0, MOVING = 1, PROJ = 2, NUM = 3;
}
namespace BPLayers {
static constexpr BroadPhaseLayer NON_MOVING(0), MOVING(1), PROJ(2);
static constexpr uint NUM = 3;
}

class ObjVsObj : public ObjectLayerPairFilter {
 public:
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != b; }
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

artic_hip::Qd Fj_from(double m00, double m10, double m20, double m01, double m11, double m21, double m02, double m12, double m22) {  // quaternion of the rotation whose columns are (m00 m10 m20), (m01 m11 m21), (m02 m12 m22)
  artic_hip::Qd q;
  const double tr = m00 + m11 + m22;
  if (tr > 0) { const double s = std::sqrt(tr + 1.0) * 2; q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s}; }
  else if (m00 > m11 && m00 > m22) { const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2; q = {0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s}; }
  else if (m11 > m22) { const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2; q = {(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s}; }
  else { const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2; q = {(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s}; }
  return artic_hip::normalize(q);
}
Vec3 V3f(const double* d) { return Vec3(float(d[0]), float(d[1]), float(d[2])); }
Vec3 V3(artic_hip::Vd v) { return Vec3(float(v.x), float(v.y), float(v.z)); }
Quat Q4(artic_hip::Qd q) { return Quat(float(q.x), float(q.y), float(q.z), float(q.w)); }

class JoltHip final : public artic_hip::Backend {
 public:
  explicit JoltHip(bool sixdof) : sixdof_(sixdof) { global_init(); }
  ~JoltHip() override { teardown(); }

  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "jolt-%d.%d.%d cpu float32 %s ball joint + HingeConstraint knee/ankle, ccd=LinearCast, vel=%d pos=%d, 1 thread",
                  JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH, sixdof_ ? "SixDOFConstraint(cone swing)" : "SwingTwistConstraint(cone swing)", vel_, pos_);
    return b;
  }

  bool build(const artic_hip::Spec& S, double hz, std::string& note) override {
    (void)hz;
    teardown();
    temp_ = std::make_unique<TempAllocatorImpl>(64 * 1024 * 1024);
    jobs_ = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
    sys_ = std::make_unique<PhysicsSystem>();
    sys_->Init(64, 0, 1024, 1024, bpif_, objvsbp_, objvsobj_);
    sys_->SetGravity(Vec3(0, 0, -float(S.gravity)));
    PhysicsSettings ps = sys_->GetPhysicsSettings();
    ps.mAllowSleeping = false;
    sys_->SetPhysicsSettings(ps);
    vel_ = int(ps.mNumVelocitySteps);
    pos_ = int(ps.mNumPositionSteps);
    BodyInterface& bi = sys_->GetBodyInterface();
    if (S.wall) {
      BodyCreationSettings ws(new BoxShape(V3f(S.wall_half), 0.01f), RVec3(S.wall_c[0], S.wall_c[1], S.wall_c[2]), Quat(float(S.wall_q[0]), float(S.wall_q[1]), float(S.wall_q[2]), float(S.wall_q[3])),
                              EMotionType::Static, Layers::NON_MOVING);
      ws.mFriction = 0.5f; ws.mRestitution = 0.0f;
      wall_ = bi.CreateAndAddBody(ws, EActivation::DontActivate);
    }
    for (int i = 0; i < 3; ++i) {
      const artic_hip::Body& b = S.body[i];
      const float mh = float(std::min(std::min(b.half[0], b.half[1]), b.half[2]));
      BodyCreationSettings bs(new BoxShape(V3f(b.half), std::min(0.05f, 0.25f * mh)), RVec3(b.c[0], b.c[1], b.c[2]), Quat(float(b.q[0]), float(b.q[1]), float(b.q[2]), float(b.q[3])),
                              EMotionType::Dynamic, Layers::MOVING);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(b.mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3f(b.v); bs.mAngularVelocity = V3f(b.w);
      bs.mMotionQuality = EMotionQuality::LinearCast;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    const BodyLockInterfaceNoLock& li = sys_->GetBodyLockInterfaceNoLock();
    // ---- ball joint world -> body 0. World space is not usable (the body may start rotated off the neutral frame), so use LocalToBodyCOM.
    {
      const artic_hip::Ball& B = S.ball;
      const artic_hip::Qd F = artic_hip::qd(B.F), Rs = artic_hip::qd(B.start), Rt = artic_hip::qd(B.target), qs = artic_hip::qd(S.body[0].q);
      const artic_hip::Vd an = artic_hip::vd(B.anchor);
      const artic_hip::Qd Lc = artic_hip::conj(qs) * Rs * F;  // joint frame in the child body frame
      const artic_hip::Vd lp = artic_hip::rot(artic_hip::conj(qs), an - artic_hip::vd(S.body[0].c));
      const artic_hip::Vd fx{1, 0, 0}, fy{0, 1, 0};
      const artic_hip::Vd t1 = artic_hip::rot(F, fx), p1 = artic_hip::rot(F, fy), t2 = artic_hip::rot(Lc, fx), p2 = artic_hip::rot(Lc, fy);
      // SixDOF constraint space = the frame F (x twist, y pitch axis, z = x cross y). SwingTwist constraint space (Jolt source, SwingTwistConstraint.cpp:101) = columns (twist axis, plane x twist, plane axis):
      // its Y is the NORMAL axis (= -F.z here) and its Z is the plane axis (= F.y, the pitch axis). So rotation about the pitch axis is limited by mNormalHalfConeAngle.
      const artic_hip::Qd Fj = artic_hip::normalize(artic_hip::Qd{0, 0, 0, 1}) * [&] {
        const artic_hip::Vd fx = artic_hip::rot(F, artic_hip::Vd{1, 0, 0}), fy = artic_hip::rot(F, artic_hip::Vd{0, 1, 0}), fz = artic_hip::rot(F, artic_hip::Vd{0, 0, 1});
        const double m00 = fx.x, m10 = fx.y, m20 = fx.z, m01 = -fz.x, m11 = -fz.y, m21 = -fz.z, m02 = fy.x, m12 = fy.y, m22 = fy.z;
        return Fj_from(m00, m10, m20, m01, m11, m21, m02, m12, m22);
      }();
      const artic_hip::Qd Rcs = sixdof_ ? artic_hip::conj(F) * Rt * F : artic_hip::conj(Fj) * Rt * Fj;  // target orientation in constraint space (parent = fixed world)
      Body* child = li.TryGetBody(ids_[0]);
      if (!sixdof_) {
        SwingTwistConstraintSettings ss;
        ss.mSpace = EConstraintSpace::LocalToBodyCOM;
        ss.mPosition1 = V3(an); ss.mTwistAxis1 = V3(t1); ss.mPlaneAxis1 = V3(p1);
        ss.mPosition2 = V3(lp); ss.mTwistAxis2 = V3(t2); ss.mPlaneAxis2 = V3(p2);
        ss.mSwingType = ESwingType::Cone;
        ss.mNormalHalfConeAngle = float(B.sw1_lim);  // rotation about the pitch axis (= plane axis) is limited by the NORMAL half cone angle (see the Fj comment above)
        ss.mPlaneHalfConeAngle = float(B.sw2_lim);   // rotation about the normal axis (frame z = abduction)
        ss.mTwistMinAngle = float(-B.tw_lim); ss.mTwistMaxAngle = float(B.tw_lim);
        SwingTwistConstraint* c = static_cast<SwingTwistConstraint*>(ss.Create(Body::sFixedToWorld, *child));
        c->GetTwistMotorSettings().mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(B.kp[0]), float(B.kd[0]));
        c->GetTwistMotorSettings().SetTorqueLimit(float(B.cap[0]));
        const int sa = B.kp[1] >= B.kp[2] ? 1 : 2;  // one swing motor for both swing axes: the settings of the stiffer axis
        c->GetSwingMotorSettings().mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(B.kp[sa]), float(B.kd[sa]));
        c->GetSwingMotorSettings().SetTorqueLimit(float(std::max(B.cap[1], B.cap[2])));
        c->SetTargetOrientationCS(Q4(Rcs));
        c->SetTwistMotorState(B.drive_twist ? EMotorState::Position : EMotorState::Off);
        c->SetSwingMotorState(B.drive_swing ? EMotorState::Position : EMotorState::Off);
        sys_->AddConstraint(c);
        cons_.push_back(c);
      } else {
        SixDOFConstraintSettings ss;
        ss.mSpace = EConstraintSpace::LocalToBodyCOM;
        ss.mPosition1 = V3(an); ss.mAxisX1 = V3(t1); ss.mAxisY1 = V3(p1);
        ss.mPosition2 = V3(lp); ss.mAxisX2 = V3(t2); ss.mAxisY2 = V3(p2);
        ss.mSwingType = ESwingType::Cone;
        ss.MakeFixedAxis(SixDOFConstraintSettings::EAxis::TranslationX);
        ss.MakeFixedAxis(SixDOFConstraintSettings::EAxis::TranslationY);
        ss.MakeFixedAxis(SixDOFConstraintSettings::EAxis::TranslationZ);
        ss.SetLimitedAxis(SixDOFConstraintSettings::EAxis::RotationX, float(-B.tw_lim), float(B.tw_lim));
        ss.SetLimitedAxis(SixDOFConstraintSettings::EAxis::RotationY, float(-B.sw1_lim), float(B.sw1_lim));
        ss.SetLimitedAxis(SixDOFConstraintSettings::EAxis::RotationZ, float(-B.sw2_lim), float(B.sw2_lim));
        SixDOFConstraint* c = static_cast<SixDOFConstraint*>(ss.Create(Body::sFixedToWorld, *child));
        const SixDOFConstraintSettings::EAxis ax[3] = {SixDOFConstraintSettings::EAxis::RotationX, SixDOFConstraintSettings::EAxis::RotationY, SixDOFConstraintSettings::EAxis::RotationZ};
        for (int k = 0; k < 3; ++k) {
          c->GetMotorSettings(ax[k]).mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(B.kp[k]), float(B.kd[k]));
          c->GetMotorSettings(ax[k]).SetTorqueLimit(float(B.cap[k]));
        }
        c->SetTargetOrientationCS(Q4(Rcs));
        c->SetMotorState(ax[0], B.drive_twist ? EMotorState::Position : EMotorState::Off);
        c->SetMotorState(ax[1], B.drive_swing ? EMotorState::Position : EMotorState::Off);
        c->SetMotorState(ax[2], B.drive_swing ? EMotorState::Position : EMotorState::Off);
        sys_->AddConstraint(c);
        cons_.push_back(c);
      }
    }
    // ---- hinges (knee, ankle): bodies 0->1, 1->2, world-space frames at the start pose, as in chain-limb
    for (int h = 0; h < 2; ++h) {
      const artic_hip::Hinge& H = S.hinge[h];
      HingeConstraintSettings hs;
      hs.mSpace = EConstraintSpace::WorldSpace;
      hs.mPoint1 = hs.mPoint2 = RVec3(H.anchor[0], H.anchor[1], H.anchor[2]);
      const Vec3 ax = V3f(H.axis).Normalized();
      hs.mHingeAxis1 = hs.mHingeAxis2 = ax;
      hs.mNormalAxis1 = hs.mNormalAxis2 = ax.GetNormalizedPerpendicular();
      hs.mLimitsMin = float(H.lo); hs.mLimitsMax = float(H.hi);
      HingeConstraint* c = static_cast<HingeConstraint*>(hs.Create(*li.TryGetBody(ids_[size_t(h)]), *li.TryGetBody(ids_[size_t(h + 1)])));
      if (H.drive) {
        MotorSettings& ms = c->GetMotorSettings();
        ms.mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, float(H.kp), float(H.kd));
        ms.SetTorqueLimit(float(H.cap));
        c->SetMotorState(EMotorState::Position);
        c->SetTargetAngle(0.0f);
      }
      sys_->AddConstraint(c);
      cons_.push_back(c);
    }
    if (S.proj) {
      BodyCreationSettings bs(new BoxShape(V3f(S.proj_half), 0.05f), RVec3(S.proj_c[0], S.proj_c[1], S.proj_c[2]), Quat::sIdentity(), EMotionType::Dynamic, Layers::PROJ);
      bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
      bs.mMassPropertiesOverride.mMass = float(S.proj_mass);
      bs.mLinearDamping = 0.0f; bs.mAngularDamping = 0.0f; bs.mAllowSleeping = false; bs.mFriction = 0.5f; bs.mRestitution = 0.0f;
      bs.mLinearVelocity = V3f(S.proj_v);
      bs.mMotionQuality = EMotionQuality::LinearCast;
      ids_.push_back(bi.CreateAndAddBody(bs, EActivation::Activate));
    }
    sys_->OptimizeBroadPhase();
    note.clear();
    return true;
  }

  void step(float dt) override { sys_->Update(dt, 1, temp_.get(), jobs_.get()); }

  artic::LinkState link(int i) const override {
    artic::LinkState s{};
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
    if (!wall_.IsInvalid()) { bi.RemoveBody(wall_); bi.DestroyBody(wall_); wall_ = BodyID(); }
    sys_.reset();
    jobs_.reset();
    temp_.reset();
  }

  bool sixdof_;
  BPLayerIf bpif_;
  ObjVsBP objvsbp_;
  ObjVsObj objvsobj_;
  std::unique_ptr<TempAllocatorImpl> temp_;
  std::unique_ptr<JobSystem> jobs_;
  std::unique_ptr<PhysicsSystem> sys_;
  std::vector<BodyID> ids_;
  std::vector<Ref<Constraint>> cons_;
  BodyID wall_;
  int vel_ = 0, pos_ = 0;
};

}  // namespace

namespace artic_hip {
std::unique_ptr<Backend> make_jolt_swingtwist() { return std::make_unique<JoltHip>(false); }
std::unique_ptr<Backend> make_jolt_sixdof() { return std::make_unique<JoltHip>(true); }
}  // namespace artic_hip
