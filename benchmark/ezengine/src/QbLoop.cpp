#include <Jolt/Jolt.h>

#include <Core/Interfaces/PhysicsQuery.h>
#include <Foundation/Application/Application.h>
#include <JoltPlugin/System/JoltCore.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <QbGameState.h>
#include <algorithm>
#include <cmath>

void QbGameState::BeforeWorldUpdate()
{
  if (m_bFinished)
    return;

  const ezTime tNow = ezTime::Now();
  m_iAppFrames++;
  if (m_bFirstCall)
  {
    // the first call comes after the scene was built and the first frame was set up: the phase clock starts here, not in OnActivation
    m_bFirstCall = false;
    m_tLastEntry = tNow;
    m_tPhaseStart = tNow;
  }

  if (m_bUnpaced)
    RunUnpaced(tNow);
  else
    RunPaced(tNow);
}

void QbGameState::BeginRecordingIfDue(ezTime tNow)
{
  if (!m_bRecording && (tNow - m_tPhaseStart).GetSeconds() >= m_fWarmup)
  {
    m_bRecording = true;
    m_tPhaseStart = tNow;
  }
}

/// Paced run (all scenarios but the unpaced E/F variants): the fixed 200 Hz steps owed since the last call are run back to back at the start
/// of the frame, like a fixed-timestep game loop; the frame (update and render, if any) follows. A row is one such frame; its wall time is only
/// known at the next call, so the row is kept pending until then.
void QbGameState::RunPaced(ezTime tNow)
{
  const double fWallMs = (tNow - m_tLastEntry).GetMilliseconds();
  const double fDelta = (tNow - m_tLastEntry).GetSeconds();
  m_tLastEntry = tNow;

  if (!m_bRecording)
  {
    m_Pending.m_bValid = false; // warm-up frames are not recorded
    BeginRecordingIfDue(tNow);  // the frame that ends the warm-up is not recorded either
  }
  else if (m_Pending.m_bValid)
  {
    FinalizePending(fWallMs);
    if ((tNow - m_tPhaseStart).GetSeconds() >= m_fMeasure)
    {
      FinishRun();
      return;
    }
  }

  m_fAccum += fDelta;
  int iSteps = (int)std::floor(m_fAccum / QB_DT);
  m_fAccum -= iSteps * QB_DT;
  if (iSteps > QB_MAX_STEPS_PER_FRAME)
    iSteps = QB_MAX_STEPS_PER_FRAME; // the excess is dropped (Godot: max_physics_steps_per_frame)

  for (int i = 0; i < iSteps; i++)
  {
    double fStepMs = 0.0;
    TickWork(fStepMs);
  }

  if (iSteps > 0 && m_bRender)
  {
    const ezTime t0 = ezTime::Now();
    SyncRender();
    const double fMs = (ezTime::Now() - t0).GetMilliseconds();
    m_fSyncMsTotal += fMs;
    if (m_bRecording)
      m_SyncCost.push_back(fMs);
  }

  StorePending();
}

/// Unpaced run (SPEC E/F): back-to-back steps without pacing. Each step is a row (wall = the whole tick: benchmark work plus Jolt step).
/// Steps are run in slices of --slice-ms so that the window keeps getting its messages; the time between slices is recorded in the summary.
void QbGameState::RunUnpaced(ezTime tNow)
{
  if (m_bRecording)
    m_fUnpacedOverheadMs += (tNow - m_tLastEntry).GetMilliseconds();

  const ezTime tSlice = tNow;
  for (;;)
  {
    const ezTime t0 = ezTime::Now();
    const bool bRec = m_bRecording;
    double fStepMs = 0.0;
    TickWork(fStepMs);
    const ezTime t1 = ezTime::Now();

    StorePending();
    if (!bRec)
    {
      m_Pending.m_bValid = false;
      BeginRecordingIfDue(t1);
    }
    else
    {
      FinalizePending((t1 - t0).GetMilliseconds());
      if ((t1 - m_tPhaseStart).GetSeconds() >= m_fMeasure)
      {
        FinishRun();
        return;
      }
    }

    if ((t1 - tSlice).GetMilliseconds() >= m_Opt.m_fSliceMs)
      break;
  }
  m_tLastEntry = ezTime::Now();
}

/// Moves the frame accumulators into a pending row and resets them.
void QbGameState::StorePending()
{
  m_Pending.m_bValid = true;
  m_Pending.m_bTicked = m_iTicksThisFrame > 0;
  m_Pending.m_bEventFrame = m_bFrameEvent;
  m_Pending.m_bDespawnFrame = m_bFrameDespawn;
  QbRow& r = m_Pending.m_Row;
  r = QbRow();
  r.m_fStepMs = m_fStepMsThisFrame;
  r.m_fQueryMs = m_fQueryMsFrame;
  r.m_fEventMs = m_fFrameEventMs;
  r.m_fDespawnMs = m_fFrameDespawnMs;
  r.m_iLive = (int)m_iLivePieces;
  r.m_iSteps = m_iTicksThisFrame;
  r.m_fStepMean = m_iTicksThisFrame > 0 ? m_fStepMsThisFrame / (double)m_iTicksThisFrame : 0.0;

  m_iTicksThisFrame = 0;
  m_fStepMsThisFrame = 0.0;
  m_fQueryMsFrame = 0.0;
  m_fFrameEventMs = 0.0;
  m_fFrameDespawnMs = 0.0;
  m_bFrameEvent = false;
  m_bFrameDespawn = false;
}

void QbGameState::FinalizePending(double fWallMs)
{
  QbRow r = m_Pending.m_Row;
  const bool bTicked = m_Pending.m_bTicked;
  m_Pending.m_bValid = false;

  // Unpaced runs tick on every row, so the awake count is refreshed every 30th row only (as the Godot side).
  if (bTicked && (!m_bUnpaced || m_iFrameIdx % 30 == 0))
  {
    const ezTime t0 = ezTime::Now();
    m_iAwakeLast = CountAwake();
    m_AwakeCost.push_back((ezTime::Now() - t0).GetMilliseconds());
  }

  r.m_iFrame = m_iFrameIdx;
  r.m_fWallMs = fWallMs;
  r.m_iAwake = m_iAwakeLast;

  m_Wall.push_back(fWallMs);
  m_Phys.push_back(r.m_fStepMs);
  if (bTicked)
  {
    m_PhysTicks.push_back(r.m_fStepMs);
    m_StepsPf.push_back((double)r.m_iSteps);
  }
  m_fCumMs += fWallMs;
  m_RowCum.push_back(m_fCumMs);
  m_RowTicked.push_back(bTicked ? 1 : 0);
  m_Rows.push_back(r);

  if (m_bDebris)
  {
    if (m_Pending.m_bEventFrame)
    {
      m_EventMsList.push_back(r.m_fEventMs);
      m_WallEvent.push_back(fWallMs);
    }
    if (m_Pending.m_bDespawnFrame)
    {
      m_DespawnMsList.push_back(r.m_fDespawnMs);
      m_WallDespawn.push_back(fWallMs);
    }
    if (!m_Pending.m_bEventFrame)
      m_WallNonEvent.push_back(fWallMs);
  }
  m_iFrameIdx++;
}

void QbGameState::FinishRun()
{
  if (m_bFinished)
    return;
  m_bFinished = true;
  WriteReport();
  if (!m_bQuitIssued)
  {
    m_bQuitIssued = true;
    RequestQuit("benchmark finished");
  }
}

void QbGameState::TickWork(double& out_fStepMs)
{
  const ezTime tWork0 = ezTime::Now();

  if (!m_bImpulseDone && !m_ImpulseLinks.empty())
  {
    m_bImpulseDone = true;
    for (const JPH::BodyID& id : m_ImpulseLinks)
    {
      const JPH::RVec3 p = m_pBI->GetPosition(id);
      m_ImpulsePos0.push_back(ezVec3((float)p.GetX(), (float)p.GetY(), (float)p.GetZ()));
      m_pBI->AddImpulse(id, JPH::Vec3(2.0f, 0.0f, 0.0f));
    }
  }

  m_iTickGlobal++;
  if (m_bRecording)
  {
    m_iTicksRec++;
    m_fDeltaMin = std::min(m_fDeltaMin, QB_DT);
    m_fDeltaMax = std::max(m_fDeltaMax, QB_DT);
  }

  if (m_bDebris)
  {
    if (m_fVelNextTick < 0.0 && m_fVelAfterSpawn >= 0.0)
      m_fVelNextTick = (double)m_pBI->GetLinearVelocity(m_VelProbe).Length();
    DespawnDue();
    // event k is due at simulated time k / R (first at t = 0), simT = ticks completed before this one / 200
    const double fSimT = (double)(m_iTickGlobal - 1) / (double)QB_PHYS_HZ;
    while ((double)m_iNextEventK / m_Opt.m_fRate <= fSimT + 1e-9)
    {
      SpawnEvent((double)m_iNextEventK / m_Opt.m_fRate);
      m_iNextEventK++;
    }
  }

  if (m_iRayCount > 0)
  {
    int iHits = 0;
    const ezTime tq0 = ezTime::Now();
    if (m_sRayApi == "jolt")
    {
      const JPH::NarrowPhaseQuery& query = m_pSystem->GetNarrowPhaseQuery();
      for (int i = 0; i < m_iRayCount; i++)
      {
        const ezVec3 d = m_RayDir[i] * (float)QB_RAY_LEN;
        const JPH::RRayCast ray(QbToJoltPos(m_RayFrom[i]), JPH::Vec3(d.x, d.y, d.z));
        JPH::RayCastResult hit;
        if (query.CastRay(ray, hit))
          iHits++;
      }
    }
    else
    {
      // ignore id ezInvalidIndex - 1 = "ignore nothing": with the default (ezInvalidIndex) the body filter also skips every body whose JPH collision group is
      // the default invalid group (our bodies have none), so a raycast would never hit them (proved: 0 hits with the default, hits with this value)
      const ezPhysicsQueryParameters params(0, ezPhysicsShapeType::Default, ezInvalidIndex - 1);
      for (int i = 0; i < m_iRayCount; i++)
      {
        ezPhysicsCastResult res;
        if (m_pJolt->Raycast(res, m_RayFrom[i], m_RayDir[i], (float)QB_RAY_LEN, params))
          iHits++;
      }
    }
    m_fQueryMsFrame += (ezTime::Now() - tq0).GetMilliseconds();
    if (m_bRecording)
      m_iHitsTotal += iHits;
  }

  const ezTime tStep0 = ezTime::Now();
  const JPH::EPhysicsUpdateError err = m_pSystem->Update((float)QB_DT, 1, m_pJolt->GetTempAllocator(), ezJoltCore::GetJoltJobSystem());
  const ezTime tStep1 = ezTime::Now();
  if (err != JPH::EPhysicsUpdateError::None)
  {
    m_uiUpdateErrorFlags |= (unsigned)err;
    m_iUpdateErrorCount++;
  }

  out_fStepMs = (tStep1 - tStep0).GetMilliseconds();
  m_fStepMsThisFrame += out_fStepMs;
  m_iTicksThisFrame++;
  if (m_bRecording)
  {
    m_StepList.push_back(out_fStepMs);
    m_ScriptCost.push_back((tStep0 - tWork0).GetMilliseconds());
  }
}

void QbGameState::SpawnEvent(double fTime)
{
  const ezTime t0 = ezTime::Now();
  if (m_iEventsTotal > 0 && m_iEventsTotal % 25 == 0)
    ReshuffleBlocks();
  const int blk = m_BlockOrder[m_iEventsTotal % 25];
  const ezVec3 c((blk / 5 - 2) * 3.0f, 1.0f, (blk % 5 - 2) * 3.0f);
  if (!m_Blocks[blk].m_Id.IsInvalid())
  {
    // remove the intact block
    m_pBI->RemoveBody(m_Blocks[blk].m_Id);
    m_pBI->DestroyBody(m_Blocks[blk].m_Id);
    if (m_bRender)
      DeleteRenderObject(m_Blocks[blk].m_hObj);
    m_Blocks[blk] = QbStatic();
    m_iEventsOnIntact++;
  }
  else
  {
    // the block of this spot is already gone (its earlier debris is still alive): spawn at the same place, nothing to remove
    m_iEventsOnUsed++;
  }
  m_LiveAt[blk]++;

  QbDebrisEvent ev;
  ev.m_fTime = fTime;
  ev.m_iBlock = blk;
  ev.m_Pieces.reserve(64);
  for (int ix = 0; ix < 4; ix++)
  {
    for (int iy = 0; iy < 4; iy++)
    {
      for (int iz = 0; iz < 4; iz++)
      {
        const ezVec3 off((ix - 1.5f) * 0.5f, (iy - 1.5f) * 0.5f, (iz - 1.5f) * 0.5f);
        const ezVec3 pos = c + off;
        QbPiece piece;
        if (m_bPooled && !m_PoolFree.empty())
        {
          piece = m_PoolFree.back();
          m_PoolFree.pop_back();
          m_pBI->SetPositionAndRotation(piece.m_Id, QbToJoltPos(pos), JPH::Quat::sIdentity(), JPH::EActivation::Activate); // wakes it
        }
        else
        {
          if (m_bPooled)
            m_iPoolMisses++;
          piece = MakePiece(pos, false);
        }
        // RNG order per piece: impulse factor, then spin x, y, z
        const double f = m_Rng.DoubleMinMax(-0.2, 0.2);
        const double sx = m_Rng.DoubleMinMax(-3.0, 3.0);
        const double sy = m_Rng.DoubleMinMax(-3.0, 3.0);
        const double sz = m_Rng.DoubleMinMax(-3.0, 3.0);
        // radial impulse from the block centre: 1.5 m/s (impulse 1.5 x mass) times 1 +- 0.2
        const ezVec3 imp = off.GetNormalized() * (float)(1.5 * 0.125 * (1.0 + f));
        m_pBI->AddImpulse(piece.m_Id, JPH::Vec3(imp.x, imp.y, imp.z));
        m_pBI->SetAngularVelocity(piece.m_Id, JPH::Vec3((float)sx, (float)sy, (float)sz));
        ev.m_Pieces.push_back(piece);
      }
    }
  }
  if (!m_bHasProbe && m_bRecording)
  {
    // proof that the impulse reaches the body: speed of a probe piece right after the call and at the next tick
    m_bHasProbe = true;
    m_VelProbe = ev.m_Pieces[63].m_Id;
    m_fVelAfterSpawn = (double)m_pBI->GetLinearVelocity(m_VelProbe).Length();
  }
  m_iLivePieces += 64;
  m_LiveEvents.push_back(std::move(ev));
  m_iEventsTotal++;
  m_fFrameEventMs += (ezTime::Now() - t0).GetMilliseconds();
  m_bFrameEvent = true;
  if (m_bRecording)
    m_iEventsRec++;
}

void QbGameState::DespawnDue()
{
  // lifetime 8 s of simulated time (simT >= event time + 8, simT = ticks completed / 200); all pieces of an event are removed in the same frame
  while (!m_LiveEvents.empty() && (double)(m_iTickGlobal - 1) / (double)QB_PHYS_HZ >= m_LiveEvents.front().m_fTime + 8.0)
  {
    const ezTime t0 = ezTime::Now();
    QbDebrisEvent& ev = m_LiveEvents.front();
    if (m_bPooled)
    {
      // park: zero velocities, teleport below the pit, sleep (stays in the simulation)
      for (QbPiece& p : ev.m_Pieces)
      {
        m_pBI->SetLinearAndAngularVelocity(p.m_Id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
        m_pBI->SetPositionAndRotation(p.m_Id, QbToJoltPos(p.m_vPark), JPH::Quat::sIdentity(), JPH::EActivation::DontActivate);
        m_pBI->DeactivateBody(p.m_Id);
        m_PoolFree.push_back(p);
      }
      if (m_bRender)
      {
        EZ_LOCK(m_pOwnedWorld->GetWriteMarker());
        for (QbPiece& p : ev.m_Pieces)
        {
          ezGameObject* pObj = nullptr;
          if (m_pOwnedWorld->TryGetObject(p.m_hObj, pObj))
          {
            pObj->SetGlobalPosition(p.m_vPark);
            pObj->SetGlobalRotation(ezQuat::MakeIdentity());
          }
        }
      }
    }
    else
    {
      for (QbPiece& p : ev.m_Pieces)
      {
        m_pBI->RemoveBody(p.m_Id);
        m_pBI->DestroyBody(p.m_Id);
        if (m_bRender)
          DeleteRenderObject(p.m_hObj);
      }
    }
    m_iLivePieces -= (long long)ev.m_Pieces.size();
    const int blk = ev.m_iBlock;
    m_LiveAt[blk]--;
    if (m_LiveAt[blk] == 0 && m_Blocks[blk].m_Id.IsInvalid())
    {
      // rebuild the block (intact again) once no debris of this spot is left
      m_Blocks[blk] = AddStaticBox(ezVec3(2.0f, 2.0f, 2.0f), ezVec3((blk / 5 - 2) * 3.0f, 1.0f, (blk % 5 - 2) * 3.0f));
    }
    m_LiveEvents.pop_front();
    m_iRemovalsRec += m_bRecording ? 1 : 0;
    m_fFrameDespawnMs += (ezTime::Now() - t0).GetMilliseconds();
    m_bFrameDespawn = true;
  }
}

int QbGameState::CountAwake()
{
  int n = 0;
  if (m_bDebris)
  {
    for (const QbDebrisEvent& ev : m_LiveEvents)
    {
      for (const QbPiece& p : ev.m_Pieces)
      {
        if (m_pBI->IsActive(p.m_Id))
          n++;
      }
    }
    return n;
  }
  for (const JPH::BodyID& id : m_Dynamic)
  {
    if (m_pBI->IsActive(id))
      n++;
  }
  return n;
}

void QbGameState::SetBodyObject(JPH::BodyID id, ezGameObjectHandle h)
{
  const size_t uiIdx = id.GetIndex();
  if (uiIdx >= m_BodyObj.size())
    m_BodyObj.resize(uiIdx + 1024);
  m_BodyObj[uiIdx] = h;
}

void QbGameState::DeleteRenderObject(ezGameObjectHandle hObj)
{
  EZ_LOCK(m_pOwnedWorld->GetWriteMarker());
  m_pOwnedWorld->DeleteObjectNow(hObj);
}

/// Copies the transforms of the bodies that moved to their game objects. Only active bodies are visited, plus the ones that were active
/// at the previous sync and went to sleep since (they need their final transform once).
void QbGameState::SyncRender()
{
  JPH::BodyIDVector active;
  m_pSystem->GetActiveBodies(JPH::EBodyType::RigidBody, active);

  if (m_SyncStamp.size() < m_BodyObj.size())
    m_SyncStamp.resize(m_BodyObj.size(), 0);
  m_uiSyncGen++;

  EZ_LOCK(m_pOwnedWorld->GetWriteMarker());

  auto sync = [this](const JPH::BodyID& id)
  {
    const size_t uiIdx = id.GetIndex();
    if (uiIdx >= m_BodyObj.size())
      return;
    ezGameObject* pObj = nullptr;
    if (!m_pOwnedWorld->TryGetObject(m_BodyObj[uiIdx], pObj))
      return;
    JPH::RVec3 p;
    JPH::Quat q;
    m_pBI->GetPositionAndRotation(id, p, q);
    pObj->SetGlobalPosition(ezVec3((float)p.GetX(), (float)p.GetY(), (float)p.GetZ()));
    pObj->SetGlobalRotation(ezQuat(q.GetX(), q.GetY(), q.GetZ(), q.GetW()));
  };

  for (const JPH::BodyID& id : active)
  {
    if (id.GetIndex() < m_SyncStamp.size())
      m_SyncStamp[id.GetIndex()] = m_uiSyncGen;
    sync(id);
  }
  for (const JPH::BodyID& id : m_PrevActive)
  {
    if (id.GetIndex() < m_SyncStamp.size() && m_SyncStamp[id.GetIndex()] != m_uiSyncGen)
      sync(id);
  }

  m_PrevActive.assign(active.begin(), active.end());
}
