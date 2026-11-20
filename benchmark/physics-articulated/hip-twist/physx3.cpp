// Hip-twist test, PhysX backends (CPU only): the hip/shoulder is a real 3-DOF joint between the fixed world and the thigh/upper arm.
//   variant "physx-art-spherical": reduced-coordinate articulation, PxArticulationJointType::eSPHERICAL (twist, swing1, swing2 all
//                                  eLIMITED, one drive per axis), knee/ankle are eREVOLUTE joints with drives (as in the chain-limb test)
//   variant "physx-d6":            maximal coordinates, PxD6Joint (3 locked translations, twist + 2 swing limited, eSWING_TWIST drives),
//                                  knee/ankle are PxD6Joint with only twist free (hinge) and a twist drive
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

#include "hip.h"

using namespace physx;

namespace {

class ErrCb final : public PxErrorCallback {
 public:
  void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
    std::fprintf(stderr, "[physx] error %d: %s (%s:%d)\n", int(code), message, file, line);
    ++count;
  }
  int count = 0;
};

PxVec3 P3(artic_hip::Vd v) { return PxVec3(float(v.x), float(v.y), float(v.z)); }
PxQuat PQ(artic_hip::Qd q) { return PxQuat(float(q.x), float(q.y), float(q.z), float(q.w)).getNormalized(); }
PxVec3 P3d(const double* d) { return PxVec3(float(d[0]), float(d[1]), float(d[2])); }

class PxBase : public artic_hip::Backend {
 public:
  ~PxBase() override = default;  // foundation, physics and extensions are process-wide singletons (PhysX allows one foundation per process) and live until exit
  artic::LinkState link(int i) const override {
    artic::LinkState s{};
    if (proj_ && i == 3) {
      const PxTransform t = proj_->getGlobalPose();
      const PxVec3 v = proj_->getLinearVelocity(), w = proj_->getAngularVelocity();
      s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z; s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
      s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z; s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
      return s;
    }
    const PxRigidBody* l = body_[size_t(i)];
    const PxTransform t = l->getGlobalPose();
    const PxVec3 v = l->getLinearVelocity(), w = l->getAngularVelocity();
    s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z;
    s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
    s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z;
    s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
    return s;
  }
  void step(float dt) override {
    scene_->simulate(dt);
    scene_->fetchResults(true);
  }

 protected:
  void init() {
    static PxDefaultAllocator alloc;
    static ErrCb err;
    static PxFoundation* foundation = PxCreateFoundation(PX_PHYSICS_VERSION, alloc, err);
    static PxPhysics* physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), false, nullptr);
    static bool ext = PxInitExtensions(*physics, nullptr);
    static bool grp = (PxSetGroupCollisionFlag(1, 1, false), true);
    (void)ext; (void)grp;
    foundation_ = foundation; physics_ = physics; err_ptr_ = &err;
  }
  void make_scene(const artic_hip::Spec& S) {
    dispatcher_ = PxDefaultCpuDispatcherCreate(0u);
    PxSceneDesc sd(physics_->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -float(S.gravity));
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    sd.flags |= PxSceneFlag::eENABLE_CCD;
    scene_ = physics_->createScene(sd);
    mat_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);
    if (S.wall) {
      PxRigidStatic* wall = physics_->createRigidStatic(PxTransform(P3d(S.wall_c), PQ(artic_hip::qd(S.wall_q))));
      PxRigidActorExt::createExclusiveShape(*wall, PxBoxGeometry(P3d(S.wall_half)), *mat_);
      PxSetGroup(*wall, 2);
      scene_->addActor(*wall);
    }
  }
  void make_proj(const artic_hip::Spec& S) {
    if (!S.proj) return;
    proj_ = physics_->createRigidDynamic(PxTransform(P3d(S.proj_c)));
    PxRigidActorExt::createExclusiveShape(*proj_, PxBoxGeometry(P3d(S.proj_half)), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*proj_, float(S.proj_mass));
    proj_->setLinearDamping(0.0f);
    proj_->setAngularDamping(0.0f);
    PxSetGroup(*proj_, 3);
    proj_->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    scene_->addActor(*proj_);
    proj_->setLinearVelocity(P3d(S.proj_v));
  }
  void setup_body(PxRigidBody* l, const artic_hip::Body& b) {
    PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(P3d(b.half)), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*l, float(b.mass));
    l->setLinearDamping(0.0f);
    l->setAngularDamping(0.0f);
    l->setMaxAngularVelocity(100.0f);
    l->setMaxLinearVelocity(1e4f);
    l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    PxSetGroup(*l, 1);
  }
  void teardown() {
    body_.clear();
    proj_ = nullptr;
    if (scene_) { scene_->release(); scene_ = nullptr; }
    if (mat_) { mat_->release(); mat_ = nullptr; }
    if (dispatcher_) { dispatcher_->release(); dispatcher_ = nullptr; }
  }
  ErrCb* err_ptr_ = nullptr;
  PxFoundation* foundation_ = nullptr;
  PxPhysics* physics_ = nullptr;
  PxDefaultCpuDispatcher* dispatcher_ = nullptr;
  PxScene* scene_ = nullptr;
  PxMaterial* mat_ = nullptr;
  std::vector<PxRigidBody*> body_;
  PxRigidDynamic* proj_ = nullptr;
  bool ext_ = false;
  int pos_ = 4, vel_ = 1;
};

class PhysxArt final : public PxBase {
 public:
  ~PhysxArt() override { teardown(); }
  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "physx-%d.%d.%d cpu float32 reduced-coordinate articulation, eSPHERICAL ball joint (all 3 axes eLIMITED, drives per axis) + eREVOLUTE knee/ankle, ccd=sweep, iters pos=%d vel=%d (library default), 1 thread",
                  PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_);
    return b;
  }
  bool build(const artic_hip::Spec& S, double, std::string& note) override {
    teardown();
    init();
    make_scene(S);
    const artic_hip::Ball& B = S.ball;
    art_ = physics_->createArticulationReducedCoordinate();
    art_->setArticulationFlag(PxArticulationFlag::eFIX_BASE, true);
    art_->setSleepThreshold(0.0f);
    art_->setWakeCounter(1e9f);
    const PxTransform basePose(P3d(B.anchor), PxQuat(PxIdentity));
    PxArticulationLink* base = art_->createLink(nullptr, basePose);
    PxRigidActorExt::createExclusiveShape(*base, PxBoxGeometry(0.05f, 0.05f, 0.05f), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*base, 1000.0f);
    PxSetGroup(*base, 1);
    const artic_hip::Qd F = artic_hip::qd(B.F), Rs = artic_hip::qd(B.start);
    PxArticulationLink* parent = base;
    PxTransform parent_pose = basePose;
    std::vector<PxArticulationLink*> links;
    for (int i = 0; i < 3; ++i) {
      const artic_hip::Body& b = S.body[i];
      const PxTransform bp(P3d(b.c), PQ(artic_hip::qd(b.q)));
      PxArticulationLink* l = art_->createLink(parent, bp);
      setup_body(l, b);
      auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
      if (i == 0) {
        j->setJointType(PxArticulationJointType::eSPHERICAL);
        j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
        j->setMotion(PxArticulationAxis::eSWING1, PxArticulationMotion::eLIMITED);
        j->setMotion(PxArticulationAxis::eSWING2, PxArticulationMotion::eLIMITED);
        j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(float(-B.tw_lim), float(B.tw_lim)));
        j->setLimitParams(PxArticulationAxis::eSWING1, PxArticulationLimit(float(-B.sw1_lim), float(B.sw1_lim)));
        j->setLimitParams(PxArticulationAxis::eSWING2, PxArticulationLimit(float(-B.sw2_lim), float(B.sw2_lim)));
        const PxTransform jw(P3d(B.anchor), PQ(F));  // joint frame, neutral pose
        j->setParentPose(parent_pose.transformInv(jw));
        j->setChildPose(bp.transformInv(PxTransform(P3d(B.anchor), PQ(Rs * F))));  // frame as seen from the child at the start pose
        const artic_hip::Vd tv = artic_hip::rotvec(artic_hip::conj(F) * artic_hip::qd(B.target) * F);
        const PxArticulationAxis::Enum ax3[3] = {PxArticulationAxis::eTWIST, PxArticulationAxis::eSWING1, PxArticulationAxis::eSWING2};
        const double tg[3] = {tv.x, tv.y, tv.z};
        for (int k = 0; k < 3; ++k) {
          const bool on = k == 0 ? B.drive_twist : B.drive_swing;
          if (!on) continue;
          j->setDriveParams(ax3[k], PxArticulationDrive(float(B.kp[k]), float(B.kd[k]), PxPerformanceEnvelope(float(B.cap[k])), PxArticulationDriveType::eFORCE));
          j->setDriveTarget(ax3[k], float(tg[k]));
        }
      } else {
        const artic_hip::Hinge& H = S.hinge[i - 1];
        const PxVec3 ax = P3d(H.axis).getNormalized();
        const PxQuat jq = PxShortestRotation(PxVec3(1, 0, 0), ax);
        const PxTransform jw(P3d(H.anchor), jq);
        j->setJointType(PxArticulationJointType::eREVOLUTE);
        j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
        j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(float(H.lo), float(H.hi)));
        j->setParentPose(parent_pose.transformInv(jw));
        j->setChildPose(bp.transformInv(jw));
        if (H.drive) {
          j->setDriveParams(PxArticulationAxis::eTWIST, PxArticulationDrive(float(H.kp), float(H.kd), PxPerformanceEnvelope(float(H.cap)), PxArticulationDriveType::eFORCE));
          j->setDriveTarget(PxArticulationAxis::eTWIST, 0.0f);
        }
      }
      links.push_back(l);
      parent = l;
      parent_pose = bp;
    }
    scene_->addArticulation(*art_);
    // initial ball joint position = the start pose (exp-map components in the parent joint frame, see PxArticulationJointReducedCoordinate::setJointPosition)
    auto* j0 = static_cast<PxArticulationJointReducedCoordinate*>(links[0]->getInboundJoint());
    const artic_hip::Vd sv = artic_hip::rotvec(artic_hip::conj(F) * Rs * F);
    j0->setJointPosition(PxArticulationAxis::eTWIST, float(sv.x));
    j0->setJointPosition(PxArticulationAxis::eSWING1, float(sv.y));
    j0->setJointPosition(PxArticulationAxis::eSWING2, float(sv.z));
    art_->updateKinematic(PxArticulationKinematicFlag::ePOSITION);
    if (S.launch_omega != 0.0) j0->setJointVelocity(PxArticulationAxis::eSWING1, float(S.launch_omega));  // start pose is neutral in the launch cases
    body_.clear();
    for (auto* l : links) body_.push_back(l);
    make_proj(S);
    PxU32 pi = 0, vi = 0;
    art_->getSolverIterationCounts(pi, vi);
    pos_ = int(pi); vel_ = int(vi);
    // self-check: the link poses after the kinematic update must equal the requested start poses
    double dmax = 0;
    for (int i = 0; i < 3; ++i) {
      const PxTransform t = links[size_t(i)]->getGlobalPose();
      dmax = std::max(dmax, double((t.p - P3d(S.body[i].c)).magnitude()));
    }
    char nb[160];
    std::snprintf(nb, sizeof nb, "start pose check: max link centre deviation after setJointPosition+updateKinematic = %.3g m; ext errors so far %d", dmax, err_ptr_->count);
    note = nb;
    return true;
  }

 private:
  void teardown() {
    PxBase::teardown();
    art_ = nullptr;
  }
  PxArticulationReducedCoordinate* art_ = nullptr;
};

class PhysxD6 final : public PxBase {
 public:
  ~PhysxD6() override { teardown(); }
  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "physx-%d.%d.%d cpu float32 maximal-coordinate rigid dynamics + PxD6Joint ball (3 translations locked, twist/swing1/swing2 limited, eSWING_TWIST drives) + PxD6Joint hinges, PGS pos=%d vel=%d (library default), ccd=sweep, 1 thread",
                  PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_);
    return b;
  }
  bool build(const artic_hip::Spec& S, double, std::string& note) override {
    teardown();
    init();
    make_scene(S);
    const artic_hip::Ball& B = S.ball;
    body_.clear();
    for (int i = 0; i < 3; ++i) {
      const artic_hip::Body& b = S.body[i];
      PxRigidDynamic* l = physics_->createRigidDynamic(PxTransform(P3d(b.c), PQ(artic_hip::qd(b.q))));
      setup_body(l, b);
      scene_->addActor(*l);
      l->setLinearVelocity(P3d(b.v));
      l->setAngularVelocity(P3d(b.w));
      body_.push_back(l);
    }
    auto lock_all = [](PxD6Joint* j) {
      j->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED); j->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED); j->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);
    };
    const artic_hip::Qd F = artic_hip::qd(B.F), Rs = artic_hip::qd(B.start), Rt = artic_hip::qd(B.target);
    const artic_hip::Qd qs = artic_hip::qd(S.body[0].q);
    const artic_hip::Vd an = artic_hip::vd(B.anchor);
    {
      const PxTransform f0(P3(an), PQ(F));
      const PxTransform f1(P3(artic_hip::rot(artic_hip::conj(qs), an - artic_hip::vd(S.body[0].c))), PQ(artic_hip::conj(qs) * Rs * F));
      PxD6Joint* j = PxD6JointCreate(*physics_, nullptr, f0, body_[0], f1);
      lock_all(j);
      j->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
      j->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
      j->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
      j->setTwistLimit(PxJointAngularLimitPair(float(-B.tw_lim), float(B.tw_lim)));
      j->setSwingLimit(PxJointLimitCone(float(B.sw1_lim), float(B.sw2_lim)));
      j->setAngularDriveConfig(PxD6AngularDriveConfig::eSWING_TWIST);
      const PxD6Drive::Enum dr[3] = {PxD6Drive::eTWIST, PxD6Drive::eSWING1, PxD6Drive::eSWING2};
      for (int k = 0; k < 3; ++k) {
        const bool on = k == 0 ? B.drive_twist : B.drive_swing;
        if (on) j->setDrive(dr[k], PxD6JointDrive(float(B.kp[k]), float(B.kd[k]), float(B.cap[k]), false));
      }
      j->setDrivePosition(PxTransform(PxVec3(0.0f), PQ(artic_hip::conj(F) * Rt * F)));
      joints_.push_back(j);
    }
    for (int h = 0; h < 2; ++h) {
      const artic_hip::Hinge& H = S.hinge[h];
      const PxVec3 ax = P3d(H.axis).getNormalized();
      const PxTransform jw(P3d(H.anchor), PxShortestRotation(PxVec3(1, 0, 0), ax));
      PxRigidActor* a0 = body_[size_t(h)];
      PxRigidActor* a1 = body_[size_t(h + 1)];
      PxD6Joint* j = PxD6JointCreate(*physics_, a0, a0->getGlobalPose().transformInv(jw), a1, a1->getGlobalPose().transformInv(jw));
      lock_all(j);
      j->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
      j->setTwistLimit(PxJointAngularLimitPair(float(H.lo), float(H.hi)));
      if (H.drive) {
        j->setDrive(PxD6Drive::eTWIST, PxD6JointDrive(float(H.kp), float(H.kd), float(H.cap), false));
        j->setDrivePosition(PxTransform(PxIdentity));
      }
      joints_.push_back(j);
    }
    make_proj(S);
    PxU32 pi = 0, vi = 0;
    body_[0]->is<PxRigidDynamic>()->getSolverIterationCounts(pi, vi);
    pos_ = int(pi); vel_ = int(vi);
    note = "maximal coordinates: PGS iterative solver, library default iterations";
    return true;
  }

 private:
  void teardown() {
    for (PxD6Joint* j : joints_) j->release();
    joints_.clear();
    PxBase::teardown();
  }
  std::vector<PxD6Joint*> joints_;
};

}  // namespace

namespace artic_hip {
std::unique_ptr<Backend> make_physx_art() { return std::make_unique<PhysxArt>(); }
std::unique_ptr<Backend> make_physx_d6() { return std::make_unique<PhysxD6>(); }
}  // namespace artic_hip
