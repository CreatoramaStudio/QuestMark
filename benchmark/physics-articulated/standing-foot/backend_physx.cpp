// Standing-foot test, PhysX backends (CPU): (1) reduced-coordinate articulation whose root is the pelvis (floating base, NOT fixed),
// (2) maximal-coordinate control: rigid dynamics joined by PxD6Joint with a twist position drive. Static ground box, same boxes/masses/frames.
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

#include "stand.h"

using namespace physx;

namespace {

class ErrCb final : public PxErrorCallback {
 public:
  void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
    std::fprintf(stderr, "[physx] error %d: %s (%s:%d)\n", static_cast<int>(code), message, file, line);
    ++count;
  }
  int count = 0;
};

PxVec3 V3f(const double* d) { return PxVec3(float(d[0]), float(d[1]), float(d[2])); }
PxQuat Qf(const double* d) { return PxQuat(float(d[0]), float(d[1]), float(d[2]), float(d[3])).getNormalized(); }
artic_foot::State state_of(const PxTransform& t, const PxVec3& v, const PxVec3& w) {
  artic_foot::State s{};
  s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z; s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
  s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z; s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
  return s;
}

class Common {
 public:
  PxDefaultAllocator alloc_;
  ErrCb err_;
  PxFoundation* foundation_ = nullptr;
  PxPhysics* physics_ = nullptr;
  PxDefaultCpuDispatcher* dispatcher_ = nullptr;
  PxScene* scene_ = nullptr;
  PxMaterial* mat_ = nullptr;
  PxRigidDynamic* proj_ = nullptr;
  bool ext_ = false;

  void init(bool extensions) {
    if (foundation_) return;
    foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, alloc_, err_);
    physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, PxTolerancesScale(), false, nullptr);
    if (extensions) ext_ = PxInitExtensions(*physics_, nullptr);
    PxSetGroupCollisionFlag(1, 1, false);  // links never collide with each other
  }
  void scene(const artic_foot::Spec& s) {
    dispatcher_ = PxDefaultCpuDispatcherCreate(0u);
    PxSceneDesc sd(physics_->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -float(s.gravity));
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    if (s.ccd) sd.flags |= PxSceneFlag::eENABLE_CCD;
    scene_ = physics_->createScene(sd);
    mat_ = physics_->createMaterial(float(s.friction), float(s.friction), 0.0f);
    PxRigidStatic* g = physics_->createRigidStatic(PxTransform(V3f(s.ground_c)));
    PxRigidActorExt::createExclusiveShape(*g, PxBoxGeometry(V3f(s.ground_half)), *mat_);
    PxSetGroup(*g, 2);
    scene_->addActor(*g);
  }
  void projectile(const artic_foot::Spec& s) {
    if (!s.proj) return;
    proj_ = physics_->createRigidDynamic(PxTransform(V3f(s.proj_c)));
    PxRigidActorExt::createExclusiveShape(*proj_, PxBoxGeometry(V3f(s.proj_half)), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*proj_, float(s.proj_mass));
    proj_->setLinearDamping(0.0f);
    proj_->setAngularDamping(0.0f);
    proj_->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);  // flies straight (see README)
    PxSetGroup(*proj_, 3);
    if (s.ccd) proj_->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    scene_->addActor(*proj_);
    proj_->setLinearVelocity(V3f(s.proj_v));
  }
  void teardown_common() {
    proj_ = nullptr;
    if (scene_) { scene_->release(); scene_ = nullptr; }
    if (mat_) { mat_->release(); mat_ = nullptr; }
    if (dispatcher_) { dispatcher_->release(); dispatcher_ = nullptr; }
  }
  void release_all() {
    teardown_common();
    if (ext_) PxCloseExtensions();
    if (physics_) physics_->release();
    if (foundation_) foundation_->release();
  }
};

class PhysxArt final : public artic_foot::Backend, Common {
 public:
  ~PhysxArt() override { teardown(); release_all(); }

  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "physx-%d.%d.%d cpu float32 reduced-coordinate-articulation floating-base(pelvis) solver-iters pos=%d vel=%d ccd=%s threads=1", PX_PHYSICS_VERSION_MAJOR,
                  PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_, ccd_ ? "sweep(eENABLE_CCD)" : "off");
    return b;
  }

  void build(const artic_foot::Spec& s) override {
    teardown();
    init(false);
    ccd_ = s.ccd;
    scene(s);
    art_ = physics_->createArticulationReducedCoordinate();  // eFIX_BASE not set: the root link (pelvis) is free
    if (s.vel_iters >= 0 || s.pos_iters >= 0) art_->setSolverIterationCounts(PxU32(s.pos_iters >= 0 ? s.pos_iters : 4), PxU32(s.vel_iters >= 0 ? s.vel_iters : 1));
    art_->setSleepThreshold(0.0f);
    art_->setWakeCounter(1e9f);
    std::vector<int> inb(s.body.size(), -1);
    for (size_t h = 0; h < s.hinge.size(); ++h) inb[size_t(s.hinge[h].child)] = int(h);
    links_.clear();
    joints_.clear();
    joints_.resize(s.hinge.size(), nullptr);
    for (size_t k = 0; k < s.body.size(); ++k) {
      const artic_foot::Body& b = s.body[k];
      const PxTransform bp(V3f(b.c), Qf(b.q));
      PxArticulationLink* parent = inb[k] < 0 ? nullptr : links_[size_t(s.hinge[size_t(inb[k])].parent)];
      PxArticulationLink* l = art_->createLink(parent, bp);
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(V3f(b.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, float(b.mass));
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      l->setMaxLinearVelocity(1e4f);
      if (s.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      links_.push_back(l);
      if (inb[k] >= 0) {
        const artic_foot::Hinge& h = s.hinge[size_t(inb[k])];
        const PxVec3 ax = V3f(h.axis).getNormalized();
        const PxTransform jw(V3f(h.anchor), PxShortestRotation(PxVec3(1, 0, 0), ax));  // joint frame: x = hinge axis
        const artic_foot::Body& pb = s.body[size_t(h.parent)];
        const PxTransform pp(V3f(pb.c), Qf(pb.q));
        auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
        j->setJointType(PxArticulationJointType::eREVOLUTE);
        j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
        j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(float(h.lo), float(h.hi)));
        j->setParentPose(pp.transformInv(jw));
        j->setChildPose(bp.transformInv(jw));
        j->setDriveParams(PxArticulationAxis::eTWIST, PxArticulationDrive(float(h.kp), float(h.kd), PxPerformanceEnvelope(float(h.max_torque)), PxArticulationDriveType::eFORCE));
        j->setDriveTarget(PxArticulationAxis::eTWIST, 0.0f);
        joints_[size_t(inb[k])] = j;
      }
    }
    scene_->addArticulation(*art_);
    projectile(s);
    PxU32 pi = 0, vi = 0;
    art_->getSolverIterationCounts(pi, vi);
    pos_ = int(pi);
    vel_ = int(vi);
    nbody_ = int(s.body.size());
  }

  void step(float dt) override { scene_->simulate(dt); scene_->fetchResults(true); }
  void drive_target(int hinge, double angle) override { joints_[size_t(hinge)]->setDriveTarget(PxArticulationAxis::eTWIST, float(angle)); }

  artic_foot::State body(int i) const override {
    if (i == nbody_) return state_of(proj_->getGlobalPose(), proj_->getLinearVelocity(), proj_->getAngularVelocity());
    const PxArticulationLink* l = links_[size_t(i)];
    return state_of(l->getGlobalPose(), l->getLinearVelocity(), l->getAngularVelocity());
  }

 private:
  void teardown() {
    links_.clear();
    joints_.clear();
    art_ = nullptr;
    teardown_common();
  }
  PxArticulationReducedCoordinate* art_ = nullptr;
  std::vector<PxArticulationLink*> links_;
  std::vector<PxArticulationJointReducedCoordinate*> joints_;
  int pos_ = 4, vel_ = 1, nbody_ = 0;
  bool ccd_ = true;
};

class PhysxMax final : public artic_foot::Backend, Common {
 public:
  ~PhysxMax() override { teardown(); release_all(); }

  std::string name() const override {
    char b[256];
    std::snprintf(b, sizeof b, "physx-%d.%d.%d cpu float32 maximal-coordinate rigid-dynamics+PxD6Joint(twist drive) solver=PGS iters pos=%d vel=%d ccd=%s threads=1", PX_PHYSICS_VERSION_MAJOR,
                  PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_, ccd_ ? "sweep(eENABLE_CCD)" : "off");
    return b;
  }

  void build(const artic_foot::Spec& s) override {
    teardown();
    init(true);
    ccd_ = s.ccd;
    scene(s);
    bodies_.clear();
    joints_.clear();
    for (const artic_foot::Body& b : s.body) {
      PxRigidDynamic* l = physics_->createRigidDynamic(PxTransform(V3f(b.c), Qf(b.q)));
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(V3f(b.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, float(b.mass));
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      if (s.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      if (s.vel_iters >= 0 || s.pos_iters >= 0) l->setSolverIterationCounts(PxU32(s.pos_iters >= 0 ? s.pos_iters : 4), PxU32(s.vel_iters >= 0 ? s.vel_iters : 1));
      scene_->addActor(*l);
      bodies_.push_back(l);
    }
    for (const artic_foot::Hinge& h : s.hinge) {
      const PxVec3 ax = V3f(h.axis).getNormalized();
      const PxTransform jw(V3f(h.anchor), PxShortestRotation(PxVec3(1, 0, 0), ax));
      const artic_foot::Body& pb = s.body[size_t(h.parent)];
      const artic_foot::Body& cb = s.body[size_t(h.child)];
      const PxTransform pp(V3f(pb.c), Qf(pb.q)), cp(V3f(cb.c), Qf(cb.q));
      PxD6Joint* j = PxD6JointCreate(*physics_, bodies_[size_t(h.parent)], pp.transformInv(jw), bodies_[size_t(h.child)], cp.transformInv(jw));
      j->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLOCKED);
      j->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
      j->setTwistLimit(PxJointAngularLimitPair(float(h.lo), float(h.hi)));
      j->setDrive(PxD6Drive::eTWIST, PxD6JointDrive(float(h.kp), float(h.kd), float(h.max_torque), false));
      j->setDrivePosition(PxTransform(PxIdentity));
      joints_.push_back(j);
    }
    projectile(s);
    PxU32 pi = 0, vi = 0;
    bodies_[0]->getSolverIterationCounts(pi, vi);
    pos_ = int(pi);
    vel_ = int(vi);
    nbody_ = int(s.body.size());
  }

  void step(float dt) override { scene_->simulate(dt); scene_->fetchResults(true); }
  void drive_target(int hinge, double angle) override { joints_[size_t(hinge)]->setDrivePosition(PxTransform(PxQuat(float(angle), PxVec3(1, 0, 0)))); }

  artic_foot::State body(int i) const override {
    if (i == nbody_) return state_of(proj_->getGlobalPose(), proj_->getLinearVelocity(), proj_->getAngularVelocity());
    const PxRigidDynamic* l = bodies_[size_t(i)];
    return state_of(l->getGlobalPose(), l->getLinearVelocity(), l->getAngularVelocity());
  }

 private:
  void teardown() {
    for (PxD6Joint* j : joints_) j->release();
    joints_.clear();
    bodies_.clear();
    teardown_common();
  }
  std::vector<PxRigidDynamic*> bodies_;
  std::vector<PxD6Joint*> joints_;
  int pos_ = 4, vel_ = 1, nbody_ = 0;
  bool ccd_ = true;
};

}  // namespace

namespace artic_foot {
std::unique_ptr<Backend> make_physx() { return std::make_unique<PhysxArt>(); }
std::unique_ptr<Backend> make_physx_maximal() { return std::make_unique<PhysxMax>(); }
}  // namespace artic_foot
