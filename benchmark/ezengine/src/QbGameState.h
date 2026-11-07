#pragma once

#include <Jolt/Jolt.h>

#include <Core/World/World.h>
#include <Foundation/Math/Random.h> // ezRandom
#include <Foundation/Time/Time.h>
#include <GameEngine/GameState/GameState.h>
#include <JoltPlugin/System/JoltWorldModule.h>
#include <RendererCore/Meshes/MeshResource.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <QbOptions.h>
#include <deque>
#include <string>
#include <vector>

constexpr int QB_SEED = 12345;
constexpr double QB_RAY_LEN = 30.0;
constexpr double QB_SPACING = 0.6;
constexpr int QB_PHYS_HZ = 200;
constexpr double QB_DT = 0.005; ///< fixed physics step, 1 / QB_PHYS_HZ
constexpr int QB_MAX_STEPS_PER_FRAME = 64;
constexpr double QB_TAU = 6.283185307179586;

inline JPH::RVec3 QbToJoltPos(const ezVec3& v)
{
  return JPH::RVec3(v.x, v.y, v.z);
}

inline JPH::Quat QbToJoltQuat(const ezQuat& q)
{
  return JPH::Quat(q.x, q.y, q.z, q.w);
}

/// One CSV row: one rendered frame in paced runs, one step in unpaced runs.
struct QbRow
{
  int m_iFrame = 0;
  double m_fWallMs = 0.0;
  double m_fStepMs = 0.0; ///< physics_ms: sum of the Jolt steps of this frame (the engine step only, not the benchmark's own rays or spawning)
  double m_fQueryMs = 0.0;
  int m_iAwake = 0;
  double m_fEventMs = 0.0;
  double m_fDespawnMs = 0.0;
  int m_iLive = 0;
  int m_iSteps = 0;
  double m_fStepMean = 0.0;
};

/// A static body with an optional game object.
struct QbStatic
{
  JPH::BodyID m_Id;
  ezGameObjectHandle m_hObj;
};

/// A debris piece: the Jolt body, its game object (render mode only) and, for the pooled variant, its park position.
struct QbPiece
{
  JPH::BodyID m_Id;
  ezGameObjectHandle m_hObj;
  ezVec3 m_vPark = ezVec3::MakeZero();
};

/// One debris event: 64 pieces that live for 8 s of simulated time.
struct QbDebrisEvent
{
  double m_fTime = 0.0;
  int m_iBlock = 0;
  std::vector<QbPiece> m_Pieces;
};

/// Everything of one frame that is known inside BeforeWorldUpdate; the wall time of the frame is only known at the next call.
struct QbPending
{
  bool m_bValid = false;
  bool m_bTicked = false;
  bool m_bEventFrame = false;
  bool m_bDespawnFrame = false;
  QbRow m_Row;
};

/// The whole benchmark: builds the scenario, runs the fixed-step loop inside ezGameState::BeforeWorldUpdate and writes the result files.
/// Physics is the ezEngine Jolt plugin (ezJoltWorldModule). Bodies are created with direct JPH calls and stepped here
/// (the cvar Jolt.Simulation.Pause stops the module's own stepping), see NOTES.md.
class QbGameState : public ezGameState
{
  EZ_ADD_DYNAMIC_REFLECTION(QbGameState, ezGameState);

public:
  QbGameState();
  ~QbGameState();

  virtual void OnActivation(ezWorld* pWorld, ezStringView sStartPosition, const ezTransform& startPositionOffset) override;
  virtual void OnDeactivation() override;
  virtual void BeforeWorldUpdate() override;
  virtual void ConfigureMainCamera() override {} ///< the camera is set once by ComputeCamera, nothing may move it

private:
  // ---- QbSetup.cpp ----
  void ApplyCVars();
  void CreateWorldAndJolt();
  void ResolveScenario();
  void BuildScene();
  void SetupRenderScene();
  void ComputeCamera();
  void CreateMeshes();
  ezGameObjectHandle CreateRenderObject(int iMesh, const ezVec3& vPos, const ezQuat& qRot, const ezVec3& vScale, bool bDynamic);

  // ---- QbScenarios.cpp ----
  void Bound(const ezVec3& vMin, const ezVec3& vMax);
  QbStatic AddStaticBox(const ezVec3& vSize, const ezVec3& vPos, ezUInt32 uiGroup = 0xFFFFFFFFu);
  void AddStaticShape(int iKind, const ezVec3& vPos, const ezQuat& qRot);
  void BuildPit(double fInterior, double fWallH = 6.0);
  JPH::BodyID AddDynamic(int iKind, const ezVec3& vPos, const ezQuat& qRot, ezUInt32 uiGroup, ezGameObjectHandle& out_hObj, float fMass = 1.0f, bool bAddNow = false);
  void FlushAdd();
  double Jit();
  void BuildPitBodies(bool bRing, int iCount = 2000);
  void BuildRayStatics();
  void BuildRays();
  void BuildChains();
  void BuildDebrisWorld();
  void ReshuffleBlocks();
  QbPiece MakePiece(const ezVec3& vPos, bool bAsleep);

  // ---- QbLoop.cpp ----
  void SpawnEvent(double fTime);
  void DespawnDue();
  void TickWork(double& out_fStepMs); ///< one physics tick: impulses, debris, rays, then the Jolt step
  void SyncRender();
  int CountAwake();
  void SetBodyObject(JPH::BodyID id, ezGameObjectHandle h);
  void StorePending();
  void DeleteRenderObject(ezGameObjectHandle hObj);
  void FinalizePending(double fWallMs);
  void BeginRecordingIfDue(ezTime now);
  void RunPaced(ezTime now);
  void RunUnpaced(ezTime now);
  void FinishRun();

  // ---- QbReport.cpp ----
  void WriteReport();
  std::string FileBase() const;

  // ---- state ----
  QbOptions m_Opt;
  std::string m_sBase;      ///< base scenario
  std::string m_sFileLabel; ///< scenario name in the file name
  std::string m_sFileMode;
  bool m_bRender = false;
  bool m_bUnpaced = false;
  bool m_bUnlockRender = false;
  bool m_bDebris = false;
  bool m_bPooled = false;
  bool m_bSleepingOff = false;
  bool m_bShadows = true;
  bool m_bVsync = false;
  bool m_bBadScenario = false;
  double m_fWarmup = 5.0;
  double m_fMeasure = 20.0;
  std::string m_sProfile = "game";
  std::string m_sCamera = "content";
  int m_iMaxBodies = 10240;
  std::string m_sRayApi = "engine";

  ezUniquePtr<ezWorld> m_pOwnedWorld;
  ezJoltWorldModule* m_pJolt = nullptr;
  JPH::PhysicsSystem* m_pSystem = nullptr;
  JPH::BodyInterface* m_pBI = nullptr;
  ezRandom m_Rng;

  JPH::ShapeRefC m_ShapeBox, m_ShapeSphere, m_ShapeCapsule;

  // render
  ezMeshResourceHandle m_hMesh[4]; ///< 0 box 0.5, 1 sphere, 2 capsule, 3 unit box (scaled)
  std::vector<ezGameObjectHandle> m_BodyObj; ///< indexed by JPH BodyID index (render mode only)
  std::string m_sRenderBackend = "n/a";
  std::string m_sAdapter = "n/a";
  std::string m_sRenderInfo;

  // scene content
  double m_fPitInterior = 0.0;
  ezVec3 m_vBMin = ezVec3::MakeZero(), m_vBMax = ezVec3::MakeZero();
  bool m_bBoundsValid = false;
  ezVec3 m_vCamPos = ezVec3::MakeZero(), m_vCamLookAt = ezVec3::MakeZero();
  double m_fCamRadius = 0.0;
  double m_fShadowDist = 0.0;
  std::vector<JPH::BodyID> m_Dynamic; ///< all dynamic bodies (not debris)
  std::vector<JPH::BodyID> m_ChainLinks;
  std::vector<JPH::BodyID> m_ImpulseLinks;
  std::vector<ezVec3> m_ImpulsePos0;
  bool m_bImpulseDone = false;
  int m_iRayCount = 0;
  std::vector<ezVec3> m_RayFrom, m_RayDir;
  std::vector<JPH::BodyID> m_PendingAdd;
  ezUInt32 m_uiCeilingGroup = 0;
  std::vector<JPH::BodyID> m_PrevActive; ///< bodies that were active at the previous render sync (they may have gone to sleep since)
  std::string m_sRenderEnv;              ///< render parity lines for env.txt (render mode)

  // debris
  std::vector<QbStatic> m_Blocks; ///< 25 blocks, m_Id invalid when removed
  std::vector<int> m_LiveAt;
  std::vector<int> m_BlockOrder;
  std::deque<QbDebrisEvent> m_LiveEvents;
  std::vector<QbPiece> m_PoolFree;
  long long m_iPoolSize0 = 0, m_iPoolMisses = 0;
  long long m_iEventsTotal = 0, m_iEventsRec = 0, m_iRemovalsRec = 0, m_iEventsOnIntact = 0, m_iEventsOnUsed = 0;
  long long m_iNextEventK = 0;
  long long m_iLivePieces = 0;
  double m_fFrameEventMs = 0.0, m_fFrameDespawnMs = 0.0;
  bool m_bFrameEvent = false, m_bFrameDespawn = false;
  double m_fVelAfterSpawn = -1.0, m_fVelNextTick = -1.0;
  JPH::BodyID m_VelProbe;
  bool m_bHasProbe = false;

  // loop
  bool m_bRecording = false;
  bool m_bFinished = false;
  bool m_bFirstCall = true;
  ezTime m_tLastEntry, m_tPhaseStart, m_tRunStart;
  double m_fAccum = 0.0; ///< paced: owed simulated time in seconds
  long long m_iTickGlobal = 0;
  int m_iFrameIdx = 0;
  int m_iAwakeLast = 0;
  double m_fQueryMsFrame = 0.0;
  long long m_iHitsTotal = 0;
  double m_fCumMs = 0.0;
  long long m_iTicksRec = 0;
  long long m_iAppFrames = 0;
  double m_fSyncMsTotal = 0.0;
  double m_fUnpacedOverheadMs = 0.0;
  QbPending m_Pending;
  int m_iTicksThisFrame = 0;
  double m_fStepMsThisFrame = 0.0;

  // recorded
  std::vector<QbRow> m_Rows;
  std::vector<double> m_Wall, m_Phys, m_PhysTicks, m_StepList, m_StepsPf, m_AwakeCost, m_ScriptCost, m_SyncCost, m_RowCum;
  std::vector<char> m_RowTicked;
  std::vector<double> m_EventMsList, m_DespawnMsList, m_WallEvent, m_WallDespawn, m_WallNonEvent;
  double m_fDeltaMin = 1e9, m_fDeltaMax = 0.0;
  std::string m_sSpaceClass;
  bool m_bQuitIssued = false;
  std::vector<unsigned> m_SyncStamp; ///< per body index: generation of the last render sync that saw it active
  unsigned m_uiSyncGen = 0;
  unsigned m_uiUpdateErrorFlags = 0; ///< OR of the JPH::EPhysicsUpdateError values returned by PhysicsSystem::Update
  long long m_iUpdateErrorCount = 0;
};
