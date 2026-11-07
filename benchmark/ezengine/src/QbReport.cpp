#include <Jolt/Jolt.h>

#include <Foundation/Threading/TaskSystem.h>
#include <JoltPlugin/System/JoltCore.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <QbGameState.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#ifndef QB_EZ_TAG
#  define QB_EZ_TAG "unknown"
#endif

// The Windows headers come last: they define macros (min, max, GetObject, ...) that break the engine headers above.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#define PSAPI_VERSION 2
#include <intrin.h>
#include <windows.h>
#include <psapi.h>

namespace
{
  /// nearest rank on sorted values, as in the Godot and Stride sides
  double QbPct(const std::vector<double>& sorted, double p)
  {
    if (sorted.empty())
      return 0.0;
    int idx = (int)std::ceil(p * (double)sorted.size()) - 1;
    idx = std::max(0, std::min(idx, (int)sorted.size() - 1));
    return sorted[idx];
  }

  std::string QbStats(const std::string& sLabel, std::vector<double> a)
  {
    std::sort(a.begin(), a.end());
    char buf[256];
    snprintf(buf, sizeof(buf), ": median=%.4f p95=%.4f p99=%.4f max=%.4f\n", QbPct(a, 0.5), QbPct(a, 0.95), QbPct(a, 0.99), QbPct(a, 1.0));
    return sLabel + buf;
  }

  void QbWorkingSetMb(double& out_cur, double& out_peak)
  {
    out_cur = 0.0;
    out_peak = 0.0;
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(pmc);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
      out_cur = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
      out_peak = (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
    }
  }

  std::string QbCpuName()
  {
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, (int)0x80000000);
    if ((unsigned)regs[0] < 0x80000004u)
      return "unknown";
    char brand[49] = {};
    for (int i = 0; i < 3; i++)
    {
      __cpuid(regs, (int)(0x80000002 + i));
      memcpy(brand + i * 16, regs, 16);
    }
    std::string s(brand);
    const size_t first = s.find_first_not_of(' ');
    const size_t last = s.find_last_not_of(std::string(" ") + std::string(1, (char)0));
    return first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
  }

  std::string QbOsName()
  {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW vi = {};
    vi.dwOSVersionInfoSize = sizeof(vi);
    HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn fn = hNt ? (RtlGetVersionFn)GetProcAddress(hNt, "RtlGetVersion") : nullptr;
    if (fn != nullptr && fn(&vi) == 0)
    {
      char buf[96];
      snprintf(buf, sizeof(buf), "Windows %lu.%lu build %lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
      return buf;
    }
    return "Windows (version unknown)";
  }

  std::string QbF(const char* szFormat, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0, double e = 0.0, double f = 0.0)
  {
    char buf[1024];
    snprintf(buf, sizeof(buf), szFormat, a, b, c, d, e, f);
    return buf;
  }
} // namespace

std::string QbGameState::FileBase() const
{
  std::string sBackend = "jolt";
  if (!m_Opt.m_sFlavour.empty())
    sBackend += "-" + m_Opt.m_sFlavour;
  const std::string sName = "ezengine-" + sBackend + (m_sProfile == "stress" ? "-stress" : "") + "-" + m_sFileLabel + "-" + m_sFileMode + "-" + m_Opt.m_sRun;
  std::filesystem::path dir(m_Opt.m_sOut);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return (dir / sName).string();
}

void QbGameState::WriteReport()
{
  const std::string sBase = FileBase();
  const bool bDebris = m_bDebris;

  // ---- CSV ----
  {
    std::ofstream f(sBase + ".csv", std::ios::binary);
    if (!f)
    {
      ezLog::Error("cannot open csv: {}.csv", sBase.c_str());
      if (ezApplication* pApp = ezApplication::GetApplicationInstance())
        pApp->SetReturnCode(3);
      return;
    }
    f << "frame,wall_ms,physics_ms,query_ms,gc0,gc1,gc2,gc_pause_ms,awake_bodies" << (bDebris ? ",event_ms,despawn_ms,debris_live" : "") << ",steps_per_frame,physics_step_ms\n";
    char buf[256];
    for (const QbRow& r : m_Rows)
    {
      snprintf(buf, sizeof(buf), "%d,%.4f,%.4f,%.4f,0,0,0,0,%d", r.m_iFrame, r.m_fWallMs, r.m_fStepMs, r.m_fQueryMs, r.m_iAwake);
      if (bDebris)
        snprintf(buf, sizeof(buf), "%d,%.4f,%.4f,%.4f,0,0,0,0,%d,%.4f,%.4f,%d", r.m_iFrame, r.m_fWallMs, r.m_fStepMs, r.m_fQueryMs, r.m_iAwake, r.m_fEventMs, r.m_fDespawnMs, r.m_iLive);
      char buf2[96];
      snprintf(buf2, sizeof(buf2), ",%d,%.4f", r.m_iSteps, r.m_fStepMean);
      f << buf << buf2 << "\n";
    }
  }

  // ---- summary ----
  double m120 = 0.0;
  for (size_t i = 0; i < m_Wall.size() && i < 120; i++)
    m120 = std::max(m120, m_Wall[i]);

  char buf[512];
  std::string sum;
  snprintf(buf, sizeof(buf), "engine=ezEngine %s backend=jolt scenario=%s mode=%s run=%s frames=%d\n", QB_EZ_TAG, m_sFileLabel.c_str(), m_sFileMode.c_str(), m_Opt.m_sRun.c_str(), m_iFrameIdx);
  sum += buf;
  sum += "profile=" + m_sProfile + " sleeping=" + (m_bSleepingOff ? "off" : "default") + " vsync=" + (m_bVsync ? "on" : "off") + "\n";
  sum += QbStats("wall_ms", m_Wall);
  sum += QbStats("physics_ms (own stopwatch: PhysicsSystem::Update only, sum of the steps of one frame; 0 on frames without a tick)", m_Phys);
  sum += "engine_monitor_TIME_PHYSICS_PROCESS_ms: n/a (no such monitor in ezEngine)\n";
  sum += QbStats("physics_ms, tick frames only", m_PhysTicks);
  sum += QbStats("physics_frame_ms (= physics_ms column: sum of the steps in one frame, tick frames only)", m_PhysTicks);
  sum += QbStats("physics_step_ms (stopwatch of every single PhysicsSystem::Update call)", m_StepList);
  sum += QbStats("steps_per_frame (tick frames only)", m_StepsPf);
  sum += std::string("physics_hz: 200") + (m_bUnpaced ? " (simulated step; unpaced loop, see pacing line)" : "") + "\n";
  sum += QbStats("awake_count_cost_ms (native cost of counting awake bodies, tick frames only)", m_AwakeCost);
  sum += QbStats("script_physics_process_ms (benchmark work of one tick before the step: impulses, debris, ray batch)", m_ScriptCost);
  sum += "total_gc_pause_ms: 0 (native C++, no GC)\n";
  snprintf(buf, sizeof(buf), "first_120_frames_max_wall_ms: %.4f\n", m120);
  sum += buf;
  snprintf(buf, sizeof(buf), "ray_hits_total (measured window): %lld\n", m_iHitsTotal);
  sum += buf;
  if (m_iUpdateErrorCount > 0)
  {
    snprintf(buf, sizeof(buf), "WARNING physics_update_errors: %lld steps returned a non-None JPH::EPhysicsUpdateError (flags OR-ed 0x%x: 1 manifold cache full, 2 body pair cache full, 4 contact constraints full)\n", m_iUpdateErrorCount, m_uiUpdateErrorFlags);
    sum += buf;
  }
  else
    sum += "physics_update_errors: 0 (every PhysicsSystem::Update returned None)\n";

  {
    const double fMeasuredMs = m_fCumMs;
    const double fMeasuredS = fMeasuredMs / 1000.0;
    snprintf(buf, sizeof(buf), "measured_s: %.4f\n", fMeasuredS);
    sum += buf;
    snprintf(buf, sizeof(buf), "fps: %.3f\n", fMeasuredS > 0.0 ? (double)m_iFrameIdx / fMeasuredS : 0.0);
    sum += buf;
    snprintf(buf, sizeof(buf), "steps_per_s: %.3f\n", fMeasuredS > 0.0 ? (double)m_iTicksRec / fMeasuredS : 0.0);
    sum += buf;
    snprintf(buf, sizeof(buf), "ticks_counted: %lld (own tick counter over the window; ezEngine has no engine-side physics frame counter used here)\n", m_iTicksRec);
    sum += buf;
    snprintf(buf, sizeof(buf), "frames: %d (%s)\n", m_iFrameIdx, m_bUnpaced ? "one row per step, see pacing line" : "render loop unlocked, no frame cap");
    sum += buf;
    snprintf(buf, sizeof(buf), "paced_steps: %lld (200 Hz x measured_s = %.1f; ratio %.4f)\n", m_iTicksRec, fMeasuredS * 200.0, fMeasuredS > 0.0 ? (double)m_iTicksRec / (fMeasuredS * 200.0) : 0.0);
    sum += buf;
    snprintf(buf, sizeof(buf), "real_time_factor: %.4f (simulated s / wall s at 200 Hz; unpaced runs: see steps_per_s)\n", fMeasuredS > 0.0 ? (double)m_iTicksRec * 0.005 / fMeasuredS : 0.0);
    sum += buf;
    snprintf(buf, sizeof(buf), "dropped_steps_estimate: %.0f (200 Hz x measured_s minus steps run, only meaningful when paced; max steps per frame %d)\n", fMeasuredS * 200.0 > (double)m_iTicksRec ? fMeasuredS * 200.0 - (double)m_iTicksRec : 0.0, QB_MAX_STEPS_PER_FRAME);
    sum += buf;
    snprintf(buf, sizeof(buf), "physics_delta_s: min=%.9f max=%.9f (1/200 = 0.005); simulated_s=%.3f\n", m_iTicksRec > 0 ? m_fDeltaMin : 0.0, m_fDeltaMax, (double)m_iTicksRec * m_fDeltaMax);
    sum += buf;
    if (m_bUnpaced)
    {
      snprintf(buf, sizeof(buf), "pacing: unpaced (back-to-back 1/200 s steps in slices of %.1f ms, one CSV row per step; the window message pump and world update run between slices, %.1f ms in the window)\n", m_Opt.m_fSliceMs, m_fUnpacedOverheadMs);
      sum += buf;
    }
    else
      sum += "pacing: fixed 200 Hz accumulator in ezGameState::BeforeWorldUpdate, vsync " + std::string(m_bVsync ? "on" : "off") + ", no frame cap, max 64 steps per frame (the excess is dropped), steps per frame vary with the frame rate\n";

    std::vector<double> wallTick;
    for (size_t i = 0; i < m_Rows.size(); i++)
    {
      if (m_RowTicked[i])
        wallTick.push_back(m_Rows[i].m_fWallMs);
    }
    sum += QbStats("wall_ms, tick frames only", wallTick);

    std::vector<double> wFirst, wLast, pFirst, pLast;
    for (size_t i = 0; i < m_Rows.size(); i++)
    {
      const double fEnd = m_RowCum[i];
      const double fStart = fEnd - m_Rows[i].m_fWallMs;
      const bool bFirst = fEnd <= 5000.0;
      const bool bLast = fStart >= fMeasuredMs - 5000.0;
      if (bFirst)
      {
        wFirst.push_back(m_Rows[i].m_fWallMs);
        if (m_RowTicked[i])
          pFirst.push_back(m_Rows[i].m_fStepMs);
      }
      if (bLast)
      {
        wLast.push_back(m_Rows[i].m_fWallMs);
        if (m_RowTicked[i])
          pLast.push_back(m_Rows[i].m_fStepMs);
      }
    }
    sum += QbStats("wall_ms first_5s", wFirst);
    sum += QbStats("wall_ms last_5s", wLast);
    sum += QbStats("physics_ms first_5s (tick frames)", pFirst);
    sum += QbStats("physics_ms last_5s (tick frames)", pLast);
    snprintf(buf, sizeof(buf), "window_frames: first_5s=%d last_5s=%d (windows overlap when measure < 10 s)\n", (int)wFirst.size(), (int)wLast.size());
    sum += buf;

    if (m_bRender)
    {
      sum += QbStats("render_sync_ms (copy of the moved bodies to their game objects, per tick frame; inside wall_ms, not in physics_ms)", m_SyncCost);
      snprintf(buf, sizeof(buf), "render_sync_ms_total: %.1f (whole run incl. warm-up)\n", m_fSyncMsTotal);
      sum += buf;
    }

    const int iAwakeEnd = CountAwake();
    snprintf(buf, sizeof(buf), "awake_bodies_at_end: %d of %d dynamic bodies\n", iAwakeEnd, bDebris ? (int)m_iLivePieces : (int)m_Dynamic.size());
    sum += buf;

    if (m_fPitInterior > 0.0)
    {
      const double fHalf = m_fPitInterior / 2.0;
      int iOutside = 0;
      auto check = [&](const JPH::BodyID& id)
      {
        const JPH::RVec3 o = m_pBI->GetPosition(id);
        if (std::fabs((double)o.GetX()) > fHalf || std::fabs((double)o.GetZ()) > fHalf || (double)o.GetY() < -0.5)
          iOutside++;
      };
      if (bDebris)
      {
        for (const QbDebrisEvent& ev : m_LiveEvents)
          for (const QbPiece& p : ev.m_Pieces)
            check(p.m_Id);
      }
      else
      {
        for (const JPH::BodyID& id : m_Dynamic)
          check(id);
      }
      snprintf(buf, sizeof(buf), "bodies_outside_pit_at_end: %d (pit interior %.0f m)\n", iOutside, m_fPitInterior);
      sum += buf;
    }

    if (!m_ImpulseLinks.empty())
    {
      double fMinY = 1e9, fMaxD = 0.0, fSumDx = 0.0;
      for (const JPH::BodyID& id : m_ChainLinks)
        fMinY = std::min(fMinY, (double)m_pBI->GetPosition(id).GetY());
      for (size_t i = 0; i < m_ImpulseLinks.size() && i < m_ImpulsePos0.size(); i++)
      {
        const JPH::RVec3 p = m_pBI->GetPosition(m_ImpulseLinks[i]);
        const ezVec3 d = ezVec3((float)p.GetX(), (float)p.GetY(), (float)p.GetZ()) - m_ImpulsePos0[i];
        fMaxD = std::max(fMaxD, (double)d.GetLength());
        fSumDx += d.x;
      }
      snprintf(buf, sizeof(buf), "chain_check: all_links_min_y=%.3f (start >= 7.5; free fall would go far lower) end_links_max_move=%.3f end_links_mean_dx=%.3f n_end_links=%d\n", fMinY, fMaxD, fSumDx / (double)m_ImpulseLinks.size(), (int)m_ImpulseLinks.size());
      sum += buf;
    }

    if (bDebris)
    {
      sum += std::string("debris_variant: ") + (m_bPooled ? "pooled (pre-created pieces parked asleep at y=-1000 inside the simulation, teleported and woken on reuse; no body or game object creation or destruction)" : "create-and-free (CreateBody + AddBody, RemoveBody + DestroyBody; render mode also creates and deletes a game object per piece)") + "\n";
      snprintf(buf, sizeof(buf), "debris_rate_events_per_s: %g\n", m_Opt.m_fRate);
      sum += buf;
      sum += QbStats("event_ms (event frames only)", m_EventMsList);
      sum += QbStats("despawn_ms (removal frames only)", m_DespawnMsList);
      sum += QbStats("wall_ms on event frames", m_WallEvent);
      sum += QbStats("wall_ms on removal frames", m_WallDespawn);
      sum += QbStats("wall_ms on non-event frames", m_WallNonEvent);
      double fLmax = 0.0, fLsum = 0.0;
      int iAmin = 1 << 30, iAmax = 0;
      for (const QbRow& r : m_Rows)
      {
        fLmax = std::max(fLmax, (double)r.m_iLive);
        fLsum += r.m_iLive;
        iAmin = std::min(iAmin, r.m_iAwake);
        iAmax = std::max(iAmax, r.m_iAwake);
      }
      snprintf(buf, sizeof(buf), "debris_live: max=%.0f mean=%.1f\n", fLmax, m_Rows.empty() ? 0.0 : fLsum / (double)m_Rows.size());
      sum += buf;
      snprintf(buf, sizeof(buf), "awake_bodies: min=%d max=%d\n", m_Rows.empty() ? 0 : iAmin, iAmax);
      sum += buf;
      snprintf(buf, sizeof(buf), "events_completed (measured window): %lld (total incl. warm-up %lld; on intact block %lld, on already-used block %lld); removals (measured window): %lld\n", m_iEventsRec, m_iEventsTotal, m_iEventsOnIntact, m_iEventsOnUsed, m_iRemovalsRec);
      sum += buf;
      snprintf(buf, sizeof(buf), "pool: initial %lld, misses %lld, free at end %d\n", m_iPoolSize0, m_iPoolMisses, (int)m_PoolFree.size());
      sum += buf;
      snprintf(buf, sizeof(buf), "impulse_probe: |v| right after spawn %.4f, at next tick %.4f (a radial piece should read about 1.5 m/s +-20%%)\n", m_fVelAfterSpawn, m_fVelNextTick);
      sum += buf;
      sum += "debris_render_meshes: shared 0.5 box mesh resource, one game object with a mesh component per piece (blocks: shared unit box mesh scaled); physics mode has no meshes\n";
    }

    double fWs, fWsPeak;
    QbWorkingSetMb(fWs, fWsPeak);
    snprintf(buf, sizeof(buf), "working_set_mb: %.1f (peak %.1f)\n", fWs, fWsPeak);
    sum += buf;
    snprintf(buf, sizeof(buf), "bodies: %d scale_N=%d\n", bDebris ? (int)m_iLivePieces : (int)m_Dynamic.size(), m_Opt.m_iBodies);
    sum += buf;
  }

  ezLog::Info("{}", sum.c_str());
  {
    std::ofstream sf(sBase + ".csv.summary.txt", std::ios::binary);
    sf << sum;
  }

  // ---- env.txt ----
  std::ofstream ef(sBase + ".env.txt", std::ios::binary);
  if (!ef)
    return;
  auto line = [&](const std::string& s) { ef << s << "\n"; };
  const JPH::PhysicsSettings ps = m_pSystem->GetPhysicsSettings();

  char camBuf[256];
  line(std::string("engine: ezEngine ") + QB_EZ_TAG + " (commit of the checkout that was built)");
  line("backend: jolt (ezEngine JoltPlugin, the engine's only physics engine; Jolt is bundled in the engine checkout)");
  line("physics_space_state_class (runtime, RTTI type name of the ezPhysicsWorldModuleInterface implementation in the world): " + (m_sSpaceClass.empty() ? std::string("<not captured>") : m_sSpaceClass));
  line("engine_tag: jolt");
  line("flavour: " + m_Opt.m_sFlavour);
  line(std::string("sleeping: ") + (m_bSleepingOff ? "off" : "default"));
  line("camera_mode: " + m_sCamera + (m_bRender ? "" : " (physics mode: no camera used for drawing, values computed from the same rule)"));
  snprintf(camBuf, sizeof(camBuf), "(%.3f, %.3f, %.3f)", m_vCamPos.x, m_vCamPos.y, m_vCamPos.z);
  const std::string sCamPos = camBuf;
  snprintf(camBuf, sizeof(camBuf), "(%.3f, %.3f, %.3f)", m_vCamLookAt.x, m_vCamLookAt.y, m_vCamLookAt.z);
  const std::string sCamLook = camBuf;
  line("camera_position: " + sCamPos);
  line("camera_look_at: " + sCamLook);
  line("camera_fov_v_deg: 60");
  line("camera_near: 0.1");
  line("camera_far: 2000");
  line(QbF("camera_content_radius: %.3f", m_fCamRadius));
  line(QbF("camera_content_aabb: min (%.3f, %.3f, %.3f) max (%.3f, %.3f, %.3f)", m_vBMin.x, m_vBMin.y, m_vBMin.z, m_vBMax.x, m_vBMax.y, m_vBMax.z));

  line("profile: " + m_sProfile);
  line("parity.profile: " + m_sProfile);
  line(std::string("parity.sleeping: ") + (m_bSleepingOff ? "off" : "default"));
  line("parity.camera_position: " + sCamPos);
  line("parity.camera_look_at: " + sCamLook);
  line("parity.fov_v_deg: 60");
  line("parity.near: 0.1");
  line("parity.far: 2000");
  line(QbF("parity.camera_content_radius: %.3f", m_fCamRadius));
  line(std::string("parity.physics_hz: 200") + (m_bUnpaced ? " (simulated step 1/200 s; unpaced loop, see NOTES)" : " (fixed accumulator in the benchmark loop, PhysicsSystem::Update(1/200 s, 1 collision step))"));
  line("physics_hz: 200 (fixed simulated step 1/200 s)");
  line("parity.max_physics_steps_per_frame: 64 (benchmark accumulator cap; the excess is dropped)");
  line(QbF("parity.gravity: (%.3f, %.3f, %.3f) (JPH PhysicsSystem gravity read back)", (double)m_pSystem->GetGravity().GetX(), (double)m_pSystem->GetGravity().GetY(), (double)m_pSystem->GetGravity().GetZ()));
  line("parity.friction: 0.500 (set on the BodyCreationSettings and on the ezJoltCore default material; the ez contact listener combines the shape materials: sqrt(f1 x f2), see NOTES)");
  line("parity.restitution: 0.100 (same; combined as max(r1, r2))");
  line("parity.linear_damping: 0.1 (BodyCreationSettings)");
  line("parity.angular_damping: 0.1 (BodyCreationSettings)");
  line("parity.body_mass: 1 (box, sphere, capsule; debris pieces 0.125); inertia computed by Jolt from the shape and mass (EOverrideMassProperties::CalculateInertia)");
  line("parity.ccd: off (EMotionQuality::Discrete, the BodyCreationSettings default; never set by the benchmark)");
  line("parity.shape_sizes: box 0.5 x 0.5 x 0.5, sphere r 0.25, capsule r 0.15 h 0.5 (JPH BoxShape half extent 0.25, SphereShape 0.25, CapsuleShape half height 0.1 radius 0.15)");
  snprintf(buf, sizeof(buf), "parity.solver: Jolt velocity_steps %d, position_steps %d (JPH PhysicsSettings read back; the ezEngine plugin does not change them)", (int)ps.mNumVelocitySteps, (int)ps.mNumPositionSteps);
  line(buf);
  snprintf(buf, sizeof(buf), "solver_iterations: %d (Jolt velocity_steps; position_steps %d)", (int)ps.mNumVelocitySteps, (int)ps.mNumPositionSteps);
  line(buf);
  line("solver_substeps: n/a (Jolt has no substeps; Bepu solver_substeps is not comparable)");
  {
    ezCVar* pJobCVar = ezCVar::FindCVarByName("Jolt.UseEzTaskSystem");
    const std::string sJob = (pJobCVar != nullptr && pJobCVar->GetType() == ezCVarType::Bool) ? (*static_cast<ezCVarBool*>(pJobCVar) ? "ez task system" : "Jolt thread pool") : "unknown";
    snprintf(buf, sizeof(buf), "parity.physics_threads: Jolt jobs run on the %s; ez worker threads short tasks %u, long tasks %u, logical cores %u; no ez thread-count command line flag found (not set by the benchmark)", sJob.c_str(),
      (unsigned)ezTaskSystem::GetWorkerThreadCount(ezWorkerThreadType::ShortTasks), (unsigned)ezTaskSystem::GetWorkerThreadCount(ezWorkerThreadType::LongTasks), (unsigned)ezSystemInformation::Get().GetCPUCoreCount());
    line(buf);
  }
  if (m_bRender)
  {
    ezWindowBase* pWin = GetMainWindow();
    if (pWin != nullptr)
    {
      const ezSizeU32 sz = pWin->GetClientAreaSize();
      line(QbF("parity.window_size: %.0f x %.0f (client area read back)", (double)sz.width, (double)sz.height));
    }
    else
      line("parity.window_size: unknown (no main window)");
    line("parity.window_mode: windowed (project RuntimeConfigs/Window.ddl: fullscreen off, borderless off)");
    line(std::string("parity.vsync: ") + (m_bVsync ? "on" : "off") + " (App.VSync cvar set; the present mode actually used by the Vulkan swapchain was not read back)");
    line(std::string("vsync: ") + (m_bVsync ? "on" : "off"));
    line(m_sRenderEnv);
  }
  else
  {
    line("parity.window_size: n/a (physics mode)");
    line("parity.window_mode: n/a (physics mode)");
    line("parity.vsync: n/a (physics mode)");
    line("vsync: off (physics mode)");
    line("parity.render: n/a (physics mode, no render settings applied; the main window is still created and an empty world is rendered, see NOTES)");
    line("shadows: n/a (physics mode)");
    line("shadow_map_size: n/a (physics mode)");
    line("msaa: n/a (physics mode)");
  }
  line("render_backend: " + m_sRenderBackend + " (ezGALDevice::GetRenderer() at run time)");
  line("render_adapter: " + m_sAdapter + " (ezGALDevice capabilities adapter name)");
  line("os: " + QbOsName());
  line("cpu: " + QbCpuName());
  line("gpu: " + m_sAdapter);
#if EZ_ENABLED(EZ_COMPILE_FOR_DEBUG)
  line("build: debug (INVALID for results)");
#elif EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
  line(std::string("build: ezEngine Dev configuration (EZ_COMPILE_FOR_DEVELOPMENT; optimised, with development features such as the profiler hooks and asserts of the Dev config), ez says: ") + ezSystemInformation::Get().GetBuildConfiguration());
#else
  line(std::string("build: ezEngine Shipping configuration, ez says: ") + ezSystemInformation::Get().GetBuildConfiguration());
#endif
  line("bench_extension: C++ ezEngine application QuestBench (benchmark/ezengine), game state QbGameState");
#if defined(__clang__)
  line(std::string("bench_extension_compiler: clang ") + __clang_version__);
#elif defined(_MSC_VER)
  line(QbF("bench_extension_compiler: MSVC cl _MSC_VER %.0f", (double)_MSC_VER));
#endif
  line("rendering_method: ezEngine default render pipeline (RendererCore), forward renderer");
  line("physics_ticks_per_second: 200 (benchmark accumulator)");
  snprintf(buf, sizeof(buf), "jolt velocity_steps: %d", (int)ps.mNumVelocitySteps);
  line(buf);
  snprintf(buf, sizeof(buf), "jolt position_steps: %d", (int)ps.mNumPositionSteps);
  line(buf);
  line(std::string("jolt allow_sleep: ") + (ps.mAllowSleeping ? "true" : "false") + " (system level; bodies have mAllowSleeping = " + (m_bSleepingOff ? "false" : "true") + ")");
  snprintf(buf, sizeof(buf), "jolt sleep_velocity_threshold: %.4f", (double)ps.mPointVelocitySleepThreshold);
  line(buf);
  snprintf(buf, sizeof(buf), "jolt sleep_time_threshold: %.4f", (double)ps.mTimeBeforeSleep);
  line(buf);
  snprintf(buf, sizeof(buf), "jolt max_bodies: %u (ezJoltSettingsComponent MaxBodies; the module derives contacts = 4 x and body pairs = 40 x from it, see NOTES)", (unsigned)m_pSystem->GetMaxBodies());
  line(buf);
  snprintf(buf, sizeof(buf), "jolt max_body_pairs: %u (module derived)", (unsigned)(m_pSystem->GetMaxBodies() * 40u));
  line(buf);
  snprintf(buf, sizeof(buf), "jolt max_contact_constraints: %u (module derived)", (unsigned)(m_pSystem->GetMaxBodies() * 4u));
  line(buf);
  line("jolt temporary_memory_buffer_size: growing chunked allocator (ezJoltTempAlloc), not a fixed size");
  line(std::string("jolt_limit_profile_applied_by_extension: n/a (ezEngine: limits come from MaxBodies = ") + std::to_string(m_iMaxBodies) + ")");
  line(std::string("max_fps: 0 (no cap; vsync ") + (m_bVsync ? "on" : "off") + ")");
  line("max_physics_steps_per_frame: 64");
  line("time_scale: 1.0");
  line(std::string("unpaced: ") + (m_bUnpaced ? "1" : "0") + " scale_bodies: " + std::to_string(m_Opt.m_iBodies));
  line(std::string("ray_api: ") + m_sRayApi + (m_sRayApi == "engine" ? " (ezPhysicsWorldModuleInterface::Raycast, Closest hit, with FillCastResult work per hit)" : " (JPH NarrowPhaseQuery::CastRay, first hit by the default collector)"));
  line(std::string("multithreaded_rendering: ") + (m_Opt.m_bMtRender ? "on" : "off") + " (Rendering.Multithreading cvar; off = update and render run one after the other on the main thread)");
  line("render_scale: 1.0 (App.RenderScale cvar set to 1)");
  line(QbF("unpaced_slice_ms: %.1f (unpaced runs only)", m_Opt.m_fSliceMs));
  line("rng: ezRandom seeded with 12345 (draw order as the Godot side; the values differ from Godot's RNG, so the scene layouts differ in detail)");
  line("physics_stepping: JPH::PhysicsSystem::Update called by the benchmark, cvar Jolt.Simulation.Pause = true so the module does not step (see NOTES)");
  line("material: ezJoltCore default material set to friction 0.5 / restitution 0.1 for this process (shapes use the default material)");
  line("cmdline: " + m_Opt.m_sCmdline);
}
