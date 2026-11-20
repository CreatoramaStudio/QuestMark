// Chain-limb test, PhysX backend (CPU only, reduced-coordinate articulation with a fixed base and 12 revolute joints).
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <vector>
#include <cstdio>
#include <memory>

#include "backend.h"

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

class PhysxBackend final : public artic::Backend {
 public:
  ~PhysxBackend() override {
    teardown();
    if (physics_) physics_->release();
    if (foundation_) foundation_->release();
  }

  std::string name() const override {
    char b[512];
    std::snprintf(b, sizeof b,
                  "physx-%d.%d.%d cpu float32 reduced-coordinate-articulation fixed-base solver-iters pos=%d vel=%d ccd=%s threads=%d",
                  PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_,
                  ccd_ ? "sweep(eENABLE_CCD)" : "off", threads_);
    return b;
  }

  // ---- limb scenario (--scenario limb): articulation tree from p.limb (fixed tiny base at the root anchor, one revolute joint per hinge)
  void build_limb(const artic::Params& p) {
    const artic::LimbSpec& L = *p.limb;
    links_ = static_cast<int>(L.body.size());
    threads_ = p.threads;
    ccd_ = p.ccd;
    dispatcher_ = PxDefaultCpuDispatcherCreate(p.threads <= 1 ? 0u : static_cast<PxU32>(p.threads));
    PxSceneDesc sd(physics_->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -p.gravity);
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    if (p.ccd) sd.flags |= PxSceneFlag::eENABLE_CCD;
    scene_ = physics_->createScene(sd);
    mat_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);
    auto V3f = [](const double* d) { return PxVec3(float(d[0]), float(d[1]), float(d[2])); };
    auto Qf = [](const double* d) { return PxQuat(float(d[0]), float(d[1]), float(d[2]), float(d[3])).getNormalized(); };
    if (L.wall) {
      PxRigidStatic* wall = physics_->createRigidStatic(PxTransform(V3f(L.wall_c), Qf(L.wall_q)));
      PxRigidActorExt::createExclusiveShape(*wall, PxBoxGeometry(V3f(L.wall_half)), *mat_);
      PxSetGroup(*wall, 2);
      scene_->addActor(*wall);
    }
    art_ = physics_->createArticulationReducedCoordinate();
    art_->setArticulationFlag(PxArticulationFlag::eFIX_BASE, true);
    if (p.pos_iters >= 0 || p.vel_iters >= 0)
      art_->setSolverIterationCounts(static_cast<PxU32>(p.pos_iters >= 0 ? p.pos_iters : 4), static_cast<PxU32>(p.vel_iters >= 0 ? p.vel_iters : 1));
    art_->setSleepThreshold(0.0f);
    art_->setWakeCounter(1e9f);
    const PxTransform basePose(V3f(L.hinge[0].anchor), PxQuat(PxIdentity));
    PxArticulationLink* base = art_->createLink(nullptr, basePose);
    PxRigidActorExt::createExclusiveShape(*base, PxBoxGeometry(0.05f, 0.05f, 0.05f), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*base, 1000.0f);
    PxSetGroup(*base, 1);
    links_vec_.clear();
    std::vector<PxTransform> pose;
    PxArticulationLink* parent = base;
    PxTransform parent_pose = basePose;
    for (size_t i = 0; i < L.body.size(); ++i) {
      const artic::LimbBody& b = L.body[i];
      const PxTransform bp(V3f(b.c), Qf(b.q));
      PxArticulationLink* l = art_->createLink(parent, bp);
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(V3f(b.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, float(b.mass));
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      l->setMaxLinearVelocity(1e4f);
      if (p.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      const artic::LimbHinge& h = L.hinge[i];
      // joint frame: origin at the anchor, local x = hinge axis
      const PxVec3 ax = V3f(h.axis).getNormalized();
      PxQuat jq = PxShortestRotation(PxVec3(1, 0, 0), ax);
      const PxTransform jw(V3f(h.anchor), jq);
      auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
      j->setJointType(PxArticulationJointType::eREVOLUTE);
      j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
      j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(float(h.lo), float(h.hi)));
      j->setParentPose(parent_pose.transformInv(jw));
      j->setChildPose(bp.transformInv(jw));
      if (h.drive) {
        j->setDriveParams(PxArticulationAxis::eTWIST, PxArticulationDrive(float(h.kp), float(h.kd), PxPerformanceEnvelope(float(h.max_torque)), PxArticulationDriveType::eFORCE));
        j->setDriveTarget(PxArticulationAxis::eTWIST, 0.0f);
      }
      links_vec_.push_back(l);
      parent = l;
      parent_pose = bp;
    }
    scene_->addArticulation(*art_);
    if (L.launch_hinge >= 0) {
      auto* j0 = static_cast<PxArticulationJointReducedCoordinate*>(links_vec_[static_cast<size_t>(L.launch_hinge)]->getInboundJoint());
      j0->setJointVelocity(PxArticulationAxis::eTWIST, float(L.launch_omega));
    }
    if (L.proj) {
      proj_ = physics_->createRigidDynamic(PxTransform(V3f(L.proj_c)));
      PxRigidActorExt::createExclusiveShape(*proj_, PxBoxGeometry(V3f(L.proj_half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*proj_, float(L.proj_mass));
      proj_->setLinearDamping(0.0f);
      proj_->setAngularDamping(0.0f);
      PxSetGroup(*proj_, 3);
      if (p.ccd) proj_->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      scene_->addActor(*proj_);
      proj_->setLinearVelocity(V3f(L.proj_v));
    }
    PxU32 pi = 0, vi = 0;
    art_->getSolverIterationCounts(pi, vi);
    pos_ = static_cast<int>(pi);
    vel_ = static_cast<int>(vi);
  }

  void build(const artic::Params& p) override {
    teardown();
    if (!foundation_) {
      foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, alloc_, err_);
      physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, PxTolerancesScale(), false, nullptr);
      PxSetGroupCollisionFlag(1, 1, false);  // links never collide with each other
    }
    if (p.limb) { build_limb(p); return; }
    links_ = p.links;
    threads_ = p.threads;
    ccd_ = p.ccd;
    dispatcher_ = PxDefaultCpuDispatcherCreate(p.threads <= 1 ? 0u : static_cast<PxU32>(p.threads));
    PxSceneDesc sd(physics_->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -p.gravity);
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    if (p.ccd) sd.flags |= PxSceneFlag::eENABLE_CCD;
    scene_ = physics_->createScene(sd);
    mat_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);

    PxRigidStatic* wall = physics_->createRigidStatic(PxTransform(PxVec3(12.5f, p.wall_y + p.wall_half_thick, 0.0f)));
    PxRigidActorExt::createExclusiveShape(*wall, PxBoxGeometry(13.5f, p.wall_half_thick, 10.0f), *mat_);
    PxSetGroup(*wall, 2);
    scene_->addActor(*wall);

    art_ = physics_->createArticulationReducedCoordinate();
    art_->setArticulationFlag(PxArticulationFlag::eFIX_BASE, true);
    if (p.pos_iters >= 0 || p.vel_iters >= 0)
      art_->setSolverIterationCounts(static_cast<PxU32>(p.pos_iters >= 0 ? p.pos_iters : 4), static_cast<PxU32>(p.vel_iters >= 0 ? p.vel_iters : 1));
    art_->setSleepThreshold(0.0f);
    art_->setWakeCounter(1e9f);

    // fixed base at the pivot (a tiny static-like mount, not one of the 12 limb links); then 12 limb links
    PxArticulationLink* base = art_->createLink(nullptr, PxTransform(PxVec3(0, 0, 0)));
    PxRigidActorExt::createExclusiveShape(*base, PxBoxGeometry(0.05f, 0.05f, 0.05f), *mat_);
    PxRigidBodyExt::setMassAndUpdateInertia(*base, 1000.0f);
    PxSetGroup(*base, 1);

    // joint frame: its x axis is the revolute axis; rotate x to world z
    const PxQuat jq = p.axis == "y" ? PxQuat(PxHalfPi, PxVec3(0, 0, 1)) : PxQuat(-PxHalfPi, PxVec3(0, 1, 0));  // x -> hinge axis (y or z)
    links_vec_.clear();
    PxArticulationLink* parent = base;
    for (int i = 0; i < p.links; ++i) {
      const float h = p.half_thick[static_cast<size_t>(i)];
      const float cx = p.link_half_len * (2.0f * static_cast<float>(i) + 1.0f);
      PxArticulationLink* l = art_->createLink(parent, PxTransform(PxVec3(cx, 0, 0)));
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(p.link_half_len, h, h), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, p.mass[static_cast<size_t>(i)]);
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      if (p.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
      j->setJointType(PxArticulationJointType::eREVOLUTE);
      j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
      j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(-p.joint_limit, p.joint_limit));
      const float px = (i == 0) ? 0.0f : p.link_half_len;  // parent-side anchor: base is at the pivot, otherwise parent's +x end
      j->setParentPose(PxTransform(PxVec3(px, 0, 0), jq));
      j->setChildPose(PxTransform(PxVec3(-p.link_half_len, 0, 0), jq));
      links_vec_.push_back(l);
      parent = l;
    }
    scene_->addArticulation(*art_);
    // launch: rigid rotation about the pivot = only the first joint has a relative velocity
    const float omega = p.tip_speed / (2.0f * p.link_half_len * static_cast<float>(p.links));
    auto* j0 = static_cast<PxArticulationJointReducedCoordinate*>(links_vec_[0]->getInboundJoint());
    j0->setJointVelocity(PxArticulationAxis::eTWIST, omega);  // positive about +axis (z or y), matches Jolt's launch
    PxU32 pi = 0, vi = 0;
    art_->getSolverIterationCounts(pi, vi);  // what the articulation really uses (library default unless overridden)
    pos_ = static_cast<int>(pi);
    vel_ = static_cast<int>(vi);
  }

  void step(float dt) override {
    scene_->simulate(dt);
    scene_->fetchResults(true);
  }

  artic::LinkState link(int i) const override {
    artic::LinkState s{};
    if (proj_ && static_cast<size_t>(i) == links_vec_.size()) {
      const PxTransform t = proj_->getGlobalPose();
      const PxVec3 v = proj_->getLinearVelocity(), w = proj_->getAngularVelocity();
      s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z; s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
      s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z; s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
      return s;
    }
    const PxArticulationLink* l = links_vec_[static_cast<size_t>(i)];
    const PxTransform t = l->getGlobalPose();
    const PxVec3 v = l->getLinearVelocity(), w = l->getAngularVelocity();
    s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z;
    s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
    s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z;
    s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
    return s;
  }

  int error_count() const { return err_.count; }

 private:
  void teardown() {
    links_vec_.clear();
    proj_ = nullptr;
    if (scene_) { scene_->release(); scene_ = nullptr; }  // releases the actors and the articulation it owns
    art_ = nullptr;
    if (mat_) { mat_->release(); mat_ = nullptr; }
    if (dispatcher_) { dispatcher_->release(); dispatcher_ = nullptr; }
  }

  PxDefaultAllocator alloc_;
  ErrCb err_;
  PxFoundation* foundation_ = nullptr;
  PxPhysics* physics_ = nullptr;
  PxDefaultCpuDispatcher* dispatcher_ = nullptr;
  PxScene* scene_ = nullptr;
  PxMaterial* mat_ = nullptr;
  PxArticulationReducedCoordinate* art_ = nullptr;
  std::vector<PxArticulationLink*> links_vec_;
  PxRigidDynamic* proj_ = nullptr;
  int links_ = 0, threads_ = 1, vel_ = 1, pos_ = 4;
  bool ccd_ = true;
};

// Control backend: the same limb as 12 independent rigid dynamics joined by PxRevoluteJoint (maximal coordinates, iterative solver),
// same masses, boxes, joint frames, limits, gravity and library-default solver iterations (PGS pos 4 / vel 1) as the articulation backend.
class PhysxMaximalBackend final : public artic::Backend {
 public:
  ~PhysxMaximalBackend() override {
    teardown();
    if (ext_) PxCloseExtensions();
    if (physics_) physics_->release();
    if (foundation_) foundation_->release();
  }

  std::string name() const override {
    char b[512];
    std::snprintf(b, sizeof b,
                  "physx-%d.%d.%d cpu float32 maximal-coordinate rigid-dynamics+PxRevoluteJoint solver=%s iters pos=%d vel=%d ccd=%s threads=%d axis=%s light-half-thick=%g",
                  PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, tgs_ ? "TGS" : "PGS", pos_, vel_,
                  ccd_ ? "sweep(eENABLE_CCD)" : "off", threads_, axis_.c_str(), double(lht_));
    return b;
  }

  void build(const artic::Params& p) override {
    teardown();
    if (!foundation_) {
      foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, alloc_, err_);
      physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, PxTolerancesScale(), false, nullptr);
      ext_ = PxInitExtensions(*physics_, nullptr);
      PxSetGroupCollisionFlag(1, 1, false);
    }
    links_ = p.links; threads_ = p.threads; ccd_ = p.ccd; tgs_ = p.physx_tgs; axis_ = p.axis; lht_ = p.light_half_thick;
    dispatcher_ = PxDefaultCpuDispatcherCreate(p.threads <= 1 ? 0u : static_cast<PxU32>(p.threads));
    PxSceneDesc sd(physics_->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -p.gravity);
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    if (p.ccd) sd.flags |= PxSceneFlag::eENABLE_CCD;
    if (p.physx_tgs) sd.solverType = PxSolverType::eTGS;
    scene_ = physics_->createScene(sd);
    mat_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);

    PxRigidStatic* wall = physics_->createRigidStatic(PxTransform(PxVec3(12.5f, p.wall_y + p.wall_half_thick, 0.0f)));
    PxRigidActorExt::createExclusiveShape(*wall, PxBoxGeometry(13.5f, p.wall_half_thick, 10.0f), *mat_);
    PxSetGroup(*wall, 2);
    scene_->addActor(*wall);

    const PxQuat jq = p.axis == "y" ? PxQuat(PxHalfPi, PxVec3(0, 0, 1)) : PxQuat(-PxHalfPi, PxVec3(0, 1, 0));  // x -> hinge axis
    const float omega = p.tip_speed / (2.0f * p.link_half_len * static_cast<float>(p.links));
    float ax[3];
    artic::axis_vec(p, ax);
    links_vec_.clear();
    joints_.clear();
    for (int i = 0; i < p.links; ++i) {
      const float h = p.half_thick[static_cast<size_t>(i)];
      const float cx = p.link_half_len * (2.0f * static_cast<float>(i) + 1.0f);
      PxRigidDynamic* l = physics_->createRigidDynamic(PxTransform(PxVec3(cx, 0, 0)));
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(p.link_half_len, h, h), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, p.mass[static_cast<size_t>(i)]);
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      if (p.vel_iters >= 0 || p.pos_iters >= 0)
        l->setSolverIterationCounts(static_cast<PxU32>(p.pos_iters >= 0 ? p.pos_iters : 4), static_cast<PxU32>(p.vel_iters >= 0 ? p.vel_iters : 1));
      if (p.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      scene_->addActor(*l);
      float lv[3];
      artic::launch_lin(p, omega, cx, lv);
      l->setLinearVelocity(PxVec3(lv[0], lv[1], lv[2]));
      l->setAngularVelocity(PxVec3(ax[0] * omega, ax[1] * omega, ax[2] * omega));
      links_vec_.push_back(l);
    }
    for (int i = 0; i < p.links; ++i) {
      // hinge at the shared end face x = 2*half_len*i, axis = x of the joint frames (jq)
      PxRigidActor* a0 = i == 0 ? nullptr : static_cast<PxRigidActor*>(links_vec_[static_cast<size_t>(i - 1)]);
      const PxTransform f0 = i == 0 ? PxTransform(PxVec3(0, 0, 0), jq) : PxTransform(PxVec3(p.link_half_len, 0, 0), jq);
      const PxTransform f1(PxVec3(-p.link_half_len, 0, 0), jq);
      PxRevoluteJoint* j = PxRevoluteJointCreate(*physics_, a0, f0, links_vec_[static_cast<size_t>(i)], f1);
      j->setLimit(PxJointAngularLimitPair(-p.joint_limit, p.joint_limit));
      j->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
      joints_.push_back(j);
    }
    PxU32 pi = 0, vi = 0;
    links_vec_[0]->getSolverIterationCounts(pi, vi);
    pos_ = static_cast<int>(pi);
    vel_ = static_cast<int>(vi);
  }

  void step(float dt) override {
    scene_->simulate(dt);
    scene_->fetchResults(true);
  }

  artic::LinkState link(int i) const override {
    artic::LinkState s{};
    const PxRigidDynamic* l = links_vec_[static_cast<size_t>(i)];
    const PxTransform t = l->getGlobalPose();
    const PxVec3 v = l->getLinearVelocity(), w = l->getAngularVelocity();
    s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z;
    s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
    s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z;
    s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
    return s;
  }

 private:
  void teardown() {
    for (PxRevoluteJoint* j : joints_) j->release();
    joints_.clear();
    links_vec_.clear();
    if (scene_) { scene_->release(); scene_ = nullptr; }
    if (mat_) { mat_->release(); mat_ = nullptr; }
    if (dispatcher_) { dispatcher_->release(); dispatcher_ = nullptr; }
  }

  PxDefaultAllocator alloc_;
  ErrCb err_;
  PxFoundation* foundation_ = nullptr;
  PxPhysics* physics_ = nullptr;
  PxDefaultCpuDispatcher* dispatcher_ = nullptr;
  PxScene* scene_ = nullptr;
  PxMaterial* mat_ = nullptr;
  std::vector<PxRigidDynamic*> links_vec_;
  std::vector<PxRevoluteJoint*> joints_;
  std::string axis_ = "z";
  int links_ = 0, threads_ = 1, vel_ = 1, pos_ = 4;
  bool ccd_ = true, tgs_ = false, ext_ = false;
  float lht_ = -1.0f;
};

}  // namespace

namespace artic {
std::unique_ptr<Backend> make_physx_maximal() { return std::make_unique<PhysxMaximalBackend>(); }
std::unique_ptr<Backend> make_physx() { return std::make_unique<PhysxBackend>(); }
}  // namespace artic
