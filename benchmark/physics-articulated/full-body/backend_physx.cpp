// Full-body test, PhysX backend (CPU only): the humanoid is ONE reduced-coordinate articulation tree (free root = torso, one inbound revolute or fixed joint
// per link); debris, wall, ground and the projectile are separate rigid actors in the same scene.
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "backend.h"

using namespace physx;

namespace {

class ErrCb final : public PxErrorCallback {
 public:
  void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
    std::fprintf(stderr, "[physx] error %d: %s (%s:%d)\n", static_cast<int>(code), message, file, line);
    ++count;
    last = std::string(message) + " (" + file + ":" + std::to_string(line) + ")";
  }
  int count = 0;
  std::string last;
};

struct Globals {
  PxDefaultAllocator alloc;
  ErrCb err;
  PxFoundation* foundation = nullptr;
  PxPhysics* physics = nullptr;
  Globals() {
    foundation = PxCreateFoundation(PX_PHYSICS_VERSION, alloc, err);
    physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), false, nullptr);
    PxSetGroupCollisionFlag(1, 1, false);  // humanoid links never collide with each other
  }
};
Globals& G() { static Globals g; return g; }

PxVec3 V3d(const double* d) { return PxVec3(float(d[0]), float(d[1]), float(d[2])); }
PxQuat Qd(const double* d) { return PxQuat(float(d[0]), float(d[1]), float(d[2]), float(d[3])).getNormalized(); }

class PhysxBackend final : public artic_body::Backend {
 public:
  ~PhysxBackend() override { teardown(); }

  std::string name() const override {
    char b[300];
    std::snprintf(b, sizeof b, "physx-%d.%d.%d cpu float32 reduced-coordinate-articulation (1 tree, free root) + rigid dynamics for debris, PGS solver pos=%d vel=%d ccd(humanoid,proj)=%s debris-ccd=off threads=1 sleeping(debris)=on",
                  PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX, pos_, vel_, ccd_ ? "sweep(eENABLE_CCD)" : "off");
    return b;
  }
  std::string notes() const override { return notes_; }

  void build(const artic_body::Scene& sc) override {
    teardown();
    Globals& g = G();
    ccd_ = sc.ccd;
    const int err0 = g.err.count;
    const artic_body::Humanoid& M = sc.humanoid;
    dispatcher_ = PxDefaultCpuDispatcherCreate(0u);
    PxSceneDesc sd(g.physics->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -float(sc.gravity));
    sd.cpuDispatcher = dispatcher_;
    sd.filterShader = PxDefaultSimulationFilterShader;
    if (sc.ccd) sd.flags |= PxSceneFlag::eENABLE_CCD;
    scene_ = g.physics->createScene(sd);
    mat_ = g.physics->createMaterial(0.5f, 0.5f, 0.0f);
    {
      PxRigidStatic* gr = g.physics->createRigidStatic(PxTransform(PxVec3(0, 0, -5.0f)));
      PxRigidActorExt::createExclusiveShape(*gr, PxBoxGeometry(float(sc.ground_half), float(sc.ground_half), 5.0f), *mat_);
      PxSetGroup(*gr, 2);
      scene_->addActor(*gr);
    }
    if (sc.has_wall) {
      PxRigidStatic* wall = g.physics->createRigidStatic(PxTransform(V3d(sc.wall.c), Qd(sc.wall.q)));
      PxRigidActorExt::createExclusiveShape(*wall, PxBoxGeometry(V3d(sc.wall.half)), *mat_);
      PxSetGroup(*wall, 2);
      scene_->addActor(*wall);
    }
    art_ = g.physics->createArticulationReducedCoordinate();
    if (sc.pos_iters >= 0 || sc.vel_iters >= 0) art_->setSolverIterationCounts(static_cast<PxU32>(sc.pos_iters >= 0 ? sc.pos_iters : 4), static_cast<PxU32>(sc.vel_iters >= 0 ? sc.vel_iters : 1));
    art_->setSleepThreshold(0.0f);
    art_->setWakeCounter(1e9f);
    links_.clear();
    std::vector<PxTransform> pose;
    for (size_t i = 0; i < M.b.size(); ++i) {
      const artic_body::Body& b = M.b[i];
      const PxTransform bp(V3d(b.c), Qd(b.q));
      pose.push_back(bp);
      PxArticulationLink* l = art_->createLink(i == 0 ? nullptr : links_[static_cast<size_t>(b.parent)], bp);
      if (!l) { notes_ += "createLink returned null at body " + std::to_string(i) + "; "; links_.push_back(nullptr); continue; }
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(V3d(b.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, float(b.mass));
      l->setLinearDamping(0.0f);
      l->setAngularDamping(0.0f);
      l->setMaxAngularVelocity(100.0f);
      l->setMaxLinearVelocity(1e4f);
      if (sc.ccd) l->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      PxSetGroup(*l, 1);
      if (i > 0) {
        auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
        const PxVec3 ax = V3d(b.axis).getNormalized();
        const PxQuat jq = b.jtype == 2 ? PxQuat(PxIdentity) : PxShortestRotation(PxVec3(1, 0, 0), ax);  // joint frame: x = hinge axis
        const PxTransform jw(V3d(b.anchor), jq);
        if (b.jtype == 2) {
          j->setJointType(PxArticulationJointType::eFIX);
        } else {
          j->setJointType(PxArticulationJointType::eREVOLUTE);
          j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
          j->setLimitParams(PxArticulationAxis::eTWIST, PxArticulationLimit(float(b.lo), float(b.hi)));
        }
        j->setParentPose(pose[static_cast<size_t>(b.parent)].transformInv(jw));
        j->setChildPose(bp.transformInv(jw));
        if (b.jtype == 1 && b.drive) {
          j->setDriveParams(PxArticulationAxis::eTWIST, PxArticulationDrive(float(b.kp), float(b.kd), PxPerformanceEnvelope(float(b.max_torque)), PxArticulationDriveType::eFORCE));
          j->setDriveTarget(PxArticulationAxis::eTWIST, 0.0f);
        }
      }
      links_.push_back(l);
    }
    scene_->addArticulation(*art_);
    if (sc.launch_joint >= 0) {
      auto* j = static_cast<PxArticulationJointReducedCoordinate*>(links_[static_cast<size_t>(sc.launch_joint)]->getInboundJoint());
      j->setJointVelocity(PxArticulationAxis::eTWIST, float(sc.launch_omega));
    }
    deb_.clear();
    for (const artic_body::DebrisBody& d : sc.debris) {
      PxRigidDynamic* r = g.physics->createRigidDynamic(PxTransform(V3d(d.c), Qd(d.q)));
      PxRigidActorExt::createExclusiveShape(*r, PxBoxGeometry(V3d(d.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*r, float(d.mass));
      PxSetGroup(*r, 4);
      scene_->addActor(*r);
      deb_.push_back(r);
    }
    proj_ = nullptr;
    if (sc.has_proj) {
      proj_ = g.physics->createRigidDynamic(PxTransform(V3d(sc.proj.c)));
      PxRigidActorExt::createExclusiveShape(*proj_, PxBoxGeometry(V3d(sc.proj.half)), *mat_);
      PxRigidBodyExt::setMassAndUpdateInertia(*proj_, float(sc.proj.mass));
      proj_->setLinearDamping(0.0f);
      proj_->setAngularDamping(0.0f);
      PxSetGroup(*proj_, 3);
      if (sc.ccd) proj_->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
      scene_->addActor(*proj_);
      proj_->setLinearVelocity(V3d(sc.proj.v));
    }
    PxU32 pi = 0, vi = 0;
    art_->getSolverIterationCounts(pi, vi);
    pos_ = static_cast<int>(pi);
    vel_ = static_cast<int>(vi);
    notes_ += "articulation links " + std::to_string(art_->getNbLinks()) + ", scene errors during build " + std::to_string(g.err.count - err0) + (g.err.count > err0 ? " last: " + g.err.last : "");
    drive_ = M.b;
  }

  void set_targets(const std::vector<double>& ang) override {
    for (size_t i = 1; i < links_.size() && i < ang.size(); ++i) {
      if (!links_[i] || drive_[i].jtype != 1 || !drive_[i].drive) continue;
      static_cast<PxArticulationJointReducedCoordinate*>(links_[i]->getInboundJoint())->setDriveTarget(PxArticulationAxis::eTWIST, float(ang[i]));
    }
  }
  void step(float dt) override {
    scene_->simulate(dt);
    scene_->fetchResults(true);
  }

  static artic_body::State of(const PxRigidBody* l) {
    artic_body::State s{};
    const PxTransform t = l->getGlobalPose();
    const PxVec3 v = l->getLinearVelocity(), w = l->getAngularVelocity();
    s.p[0] = t.p.x; s.p[1] = t.p.y; s.p[2] = t.p.z;
    s.q[0] = t.q.x; s.q[1] = t.q.y; s.q[2] = t.q.z; s.q[3] = t.q.w;
    s.v[0] = v.x; s.v[1] = v.y; s.v[2] = v.z;
    s.w[0] = w.x; s.w[1] = w.y; s.w[2] = w.z;
    return s;
  }
  artic_body::State humanoid_body(int i) const override { return of(links_[static_cast<size_t>(i)]); }
  artic_body::State debris_body(int i) const override { return of(deb_[static_cast<size_t>(i)]); }
  artic_body::State proj_body() const override { return of(proj_); }
  bool debris_awake(int i) const override { return !deb_[static_cast<size_t>(i)]->isSleeping(); }

 private:
  void teardown() {
    links_.clear();
    deb_.clear();
    proj_ = nullptr;
    if (scene_) { scene_->release(); scene_ = nullptr; }  // releases the actors and the articulation it owns
    art_ = nullptr;
    if (mat_) { mat_->release(); mat_ = nullptr; }
    if (dispatcher_) { dispatcher_->release(); dispatcher_ = nullptr; }
    notes_.clear();
  }

  PxDefaultCpuDispatcher* dispatcher_ = nullptr;
  PxScene* scene_ = nullptr;
  PxMaterial* mat_ = nullptr;
  PxArticulationReducedCoordinate* art_ = nullptr;
  std::vector<PxArticulationLink*> links_;
  std::vector<PxRigidDynamic*> deb_;
  PxRigidDynamic* proj_ = nullptr;
  std::vector<artic_body::Body> drive_;
  std::string notes_;
  int vel_ = 1, pos_ = 4;
  bool ccd_ = true;
};

}  // namespace

namespace artic_body {
std::unique_ptr<Backend> make_physx() { return std::make_unique<PhysxBackend>(); }

// Probe: chains of N links (revolute, one DOF each), a spherical-joint link, and a wide tree; reports what the library did. No simulation claims beyond "10 steps ran, states finite".
std::string physx_probe_limits() {
  Globals& g = G();
  std::string out;
  char b[400];
  auto run = [&](const char* label, int n, bool spherical_last, bool wide) {
    const int err0 = g.err.count;
    PxDefaultCpuDispatcher* d = PxDefaultCpuDispatcherCreate(0u);
    PxSceneDesc sd(g.physics->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -9.81f);
    sd.cpuDispatcher = d;
    sd.filterShader = PxDefaultSimulationFilterShader;
    PxScene* sc = g.physics->createScene(sd);
    PxMaterial* m = g.physics->createMaterial(0.5f, 0.5f, 0.0f);
    PxArticulationReducedCoordinate* art = g.physics->createArticulationReducedCoordinate();
    std::vector<PxArticulationLink*> ls;
    int null_at = -1;
    for (int i = 0; i < n; ++i) {
      const PxTransform pose(PxVec3(0.0f, 0.0f, 1.0f + 0.5f * float(i)));
      PxArticulationLink* parent = i == 0 ? nullptr : (wide ? ls[0] : ls.back());
      PxArticulationLink* l = art->createLink(parent, pose);
      if (!l) { null_at = i; break; }
      PxRigidActorExt::createExclusiveShape(*l, PxBoxGeometry(0.2f, 0.2f, 0.2f), *m);
      PxRigidBodyExt::setMassAndUpdateInertia(*l, 1.0f);
      PxSetGroup(*l, 1);
      if (i > 0) {
        auto* j = static_cast<PxArticulationJointReducedCoordinate*>(l->getInboundJoint());
        if (spherical_last && i == n - 1) {
          j->setJointType(PxArticulationJointType::eSPHERICAL);
          j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eLIMITED);
          j->setMotion(PxArticulationAxis::eSWING1, PxArticulationMotion::eLIMITED);
          j->setMotion(PxArticulationAxis::eSWING2, PxArticulationMotion::eLIMITED);
        } else {
          j->setJointType(PxArticulationJointType::eREVOLUTE);
          j->setMotion(PxArticulationAxis::eTWIST, PxArticulationMotion::eFREE);
        }
        const PxTransform pp(PxVec3(0, 0, 0.25f)), cp(PxVec3(0, 0, -0.25f));
        j->setParentPose(wide ? PxTransform(PxVec3(0.0f, 0.0f, 0.0f) + PxVec3(0.0f, 0.0f, -0.5f * float(i) + 0.25f)) : pp);
        j->setChildPose(cp);
      }
      ls.push_back(l);
    }
    bool added = false;
    bool finite = true;
    if (null_at < 0) {
      sc->addArticulation(*art);
      added = true;
      for (int s = 0; s < 10; ++s) { sc->simulate(1.0f / 120.0f); sc->fetchResults(true); }
      for (PxArticulationLink* l : ls) { const PxVec3 p = l->getGlobalPose().p; if (!PxIsFinite(p.x) || !PxIsFinite(p.y) || !PxIsFinite(p.z)) finite = false; }
    }
    std::snprintf(b, sizeof b, "  %-28s links requested %3d created %3zu%s, nbLinks %u, added to scene %s, 10 steps finite %s, library errors %d%s\n", label, n, ls.size(),
                  null_at >= 0 ? (" (createLink returned null at " + std::to_string(null_at) + ")").c_str() : "", art->getNbLinks(), added ? "yes" : "no", added ? (finite ? "yes" : "NO") : "n/a", g.err.count - err0,
                  g.err.count > err0 ? (" last: " + g.err.last).c_str() : "");
    out += b;
    sc->release();
    m->release();
    d->release();
  };
  run("chain, revolute", 20, false, false);
  run("chain, revolute", 63, false, false);
  run("chain, revolute", 64, false, false);
  run("chain, revolute", 65, false, false);
  run("chain, revolute", 128, false, false);
  run("chain, revolute", 512, false, false);
  run("chain, last joint spherical", 5, true, false);
  return out;
}
}  // namespace artic_body
