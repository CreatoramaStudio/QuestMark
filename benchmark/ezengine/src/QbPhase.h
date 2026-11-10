#pragma once

// --phase-timing=1: per app frame stopwatch segments of the whole ezGameApplication frame, identical for the raw and the components path.
// Everything here is inert unless QbPh().m_bOn is set (the application overrides in QbApp.cpp, the event handlers and the marker module are only
// installed then), so the default run makes no extra stopwatch call. See NOTES.md "Phase timing".

#include <Core/World/World.h>
#include <Core/World/WorldModule.h>
#include <Foundation/Time/Time.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

#include <string>
#include <vector>

/// Segments of one app frame, in frame order. The chained timestamps make the segments sum to the frame total (QB_PH_FRAME_REST is the remainder).
enum QbPhSeg
{
  QB_PH_PUMP_INPUT,   ///< Run() start .. Run_AcquireImage entry: profiling frame start, message pump (ezWindowManager::Update), update mode, events, ezInputManager, game state ProcessInput
  QB_PH_ACQUIRE,      ///< Run_AcquireImage (swap chain acquire)
  QB_PH_PRE_UPDATE,   ///< AcquireImage exit .. Run_BeforeWorldUpdate entry (Run_WorldUpdateAndRender entry, UPDATE FRAME profile string)
  QB_PH_BEFORE,       ///< Run_BeforeWorldUpdate: QbGameState::BeforeWorldUpdate (raw: benchmark work and the Jolt steps; components: benchmark work and the extra world updates)
  QB_PH_UPD_INIT,     ///< world update (the application's own ezWorld::Update): update entry .. first marker: clock, resource reload, initialize phase, NextFrame messages
  QB_PH_UPD_PRE,      ///< pre-async functions before ezJoltWorldModule::StartSimulation
  QB_PH_UPD_JSTART,   ///< ezJoltWorldModule::StartSimulation (body adds, impulses, constraints, starts the Simulate task); raw path: module does not step (Pause cvar), only the early exit
  QB_PH_UPD_ASYNC,    ///< async phase: task group start and wait (the Jolt Simulate task runs on an ez worker, but is waited for in FetchResults, not here)
  QB_PH_UPD_FETCH,    ///< ezJoltWorldModule::FetchResults: wait for the Simulate task, UpdateDynamicActors (transform copy), contact event processing
  QB_PH_UPD_POST,     ///< other post-async functions
  QB_PH_UPD_XFORM,    ///< local bounds queue, delete dead objects/components, UpdateGlobalTransforms, PostTransform messages
  QB_PH_UPD_PTRANS,   ///< post-transform functions
  QB_PH_UPD_TAIL,     ///< last marker .. Run_AfterWorldUpdate entry: initialize phase 2, allocator swap, lock hand-over, RenderWorldDebugInfos (cannot be separated: non-virtual)
  QB_PH_AFTER,        ///< Run_AfterWorldUpdate (game state AfterWorldUpdate, ConfigureMainCamera, event)
  QB_PH_FPS_CONSOLE,  ///< AfterWorldUpdate exit .. Run_UpdatePlugins entry (RenderFps, RenderConsole)
  QB_PH_PLUGINS,      ///< Run_UpdatePlugins
  QB_PH_PRE_EXTRACT,  ///< UpdatePlugins exit .. BeginExtraction event (UpdateRenderScale, ExtractMainViews preamble)
  QB_PH_EXTRACT,      ///< BeginExtraction .. EndExtraction (view extraction)
  QB_PH_BEGINFRAME,   ///< EndExtraction .. BeginRender event (rest of ExtractMainViews, ezRenderWorld::BeginFrame, start of Render)
  QB_PH_RENDER,       ///< BeginRender .. EndRender (RebuildPipelines, enqueue and execute the render graphs of the empty pipeline)
  QB_PH_PRE_PRESENT,  ///< EndRender .. Run_PresentImage entry (end of Run_WorldUpdateAndRender, app tick and BeforePresent events)
  QB_PH_PRESENT,      ///< Run_PresentImage (screenshot check, PresentImage)
  QB_PH_POST_PRESENT, ///< Present exit .. Run_FinishFrame entry (ezClock update, UpdateFrameTime, AfterPresent event)
  QB_PH_FINISH,       ///< Run_FinishFrame (ezRenderWorld::EndFrame, resource manager, task system, frame allocator swap, log flush)
  QB_PH_FRAME_REST,   ///< Run_FinishFrame exit .. end of Run() (Unattended_CheckTimeout in Dev builds)
  QB_PH_COUNT
};

/// Sub-divisions that overlap with QB_PH_BEFORE (not part of the chain sum).
enum QbPhSub
{
  QB_PS_BEFORE_JSTEP,  ///< raw path: the PhysicsSystem::Update calls (with --raw-update-in-task: task start and wait)
  QB_PS_BEFORE_EXTRA,  ///< components path: the extra world updates made by the benchmark (RunPaced N-1 updates, FlushNewBodies); their update-internal split is in m_Extra
  QB_PS_COUNT
};

struct QbPhaseState
{
  bool m_bOn = false;       ///< --phase-timing=1
  bool m_bFrameRec = false; ///< the current frame lies in the measure window (decided at the start of QbGameState::BeforeWorldUpdate)
  bool m_bExtra = false;    ///< inside an extra world update (markers go to m_ExtraCur)

  // timestamps of the current frame (chain)
  ezTime m_tFrame0, m_tAcqEntry, m_tAcqExit, m_tBeforeEntry, m_tBeforeExit, m_tAfterEntry, m_tAfterExit, m_tPlugEntry, m_tPlugExit;
  ezTime m_tExtractBegin, m_tExtractEnd, m_tRenderBegin, m_tRenderEnd, m_tPresentEntry, m_tPresentExit, m_tFinishEntry, m_tFinishExit;
  bool m_bSawExtract = false, m_bSawRender = false;

  // world update markers
  ezTime m_tUpdEnter;  ///< entry of the update being measured (app update: BeforeWorldUpdate exit; extra update: just before the call)
  ezTime m_tUpdPrev;
  double m_UpdCur[9] = {};   ///< app update, current frame: init, pre, jstart, async, fetch, post, xform, ptrans, tail(unused here)
  double m_ExtraCur[9] = {}; ///< extra updates of the current frame, same layout (tail = last marker .. call return)

  // current frame
  double m_Cur[QB_PH_COUNT] = {};
  double m_CurSub[QB_PS_COUNT] = {};
  long long m_iCurSteps = 0;

  // window totals (frames committed in the measure window)
  double m_Tot[QB_PH_COUNT] = {};
  double m_TotSub[QB_PS_COUNT] = {};
  double m_TotExtra[9] = {};
  double m_TotFrame = 0.0;
  long long m_iFrames = 0, m_iSteps = 0;
  long long m_iMarkerHits[9] = {}; ///< total marker calls in the window per marker index (0..7), for a sanity check: equals app updates plus extra updates

  /// Marker k (0..7) fired in a world update: adds the time since the previous marker (or the update entry) to segment k.
  void OnMarker(int k)
  {
    const ezTime t = ezTime::Now();
    double* pAcc = m_bExtra ? m_ExtraCur : m_UpdCur;
    pAcc[k] += (t - (k == 0 ? m_tUpdEnter : m_tUpdPrev)).GetMilliseconds();
    m_tUpdPrev = t;
    if (m_bFrameRec)
      m_iMarkerHits[k]++;
  }

  void BeginExtra()
  {
    m_bExtra = true;
    m_tUpdEnter = ezTime::Now();
  }

  /// Called right after an extra world update returned.
  void EndExtra()
  {
    m_ExtraCur[8] += (ezTime::Now() - m_tUpdPrev).GetMilliseconds();
    m_bExtra = false;
  }

  /// Start of Run(): clears the per frame values.
  void FrameBegin()
  {
    for (double& d : m_Cur)
      d = 0.0;
    for (double& d : m_CurSub)
      d = 0.0;
    for (double& d : m_UpdCur)
      d = 0.0;
    for (double& d : m_ExtraCur)
      d = 0.0;
    m_iCurSteps = 0;
    m_bFrameRec = false;
    m_bSawExtract = m_bSawRender = false;
    m_tFrame0 = ezTime::Now();
  }

  /// End of Run(): fills the chained segments and commits the frame when it lies in the measure window.
  void FrameEnd()
  {
    const ezTime tEnd = ezTime::Now();
    if (!m_bFrameRec || m_tAcqEntry < m_tFrame0)
      return; // not in the window (or a frame that skipped the update: no segments)
    auto ms = [](ezTime a, ezTime b) { return (b - a).GetMilliseconds(); };
    m_Cur[QB_PH_PUMP_INPUT] = ms(m_tFrame0, m_tAcqEntry);
    m_Cur[QB_PH_ACQUIRE] = ms(m_tAcqEntry, m_tAcqExit);
    m_Cur[QB_PH_PRE_UPDATE] = ms(m_tAcqExit, m_tBeforeEntry);
    m_Cur[QB_PH_BEFORE] = ms(m_tBeforeEntry, m_tBeforeExit);
    for (int i = 0; i < 8; i++)
      m_Cur[QB_PH_UPD_INIT + i] = m_UpdCur[i];
    m_Cur[QB_PH_UPD_TAIL] = ms(m_tUpdPrev, m_tAfterEntry);
    m_Cur[QB_PH_AFTER] = ms(m_tAfterEntry, m_tAfterExit);
    m_Cur[QB_PH_FPS_CONSOLE] = ms(m_tAfterExit, m_tPlugEntry);
    m_Cur[QB_PH_PLUGINS] = ms(m_tPlugEntry, m_tPlugExit);
    if (m_bSawExtract && m_bSawRender)
    {
      m_Cur[QB_PH_PRE_EXTRACT] = ms(m_tPlugExit, m_tExtractBegin);
      m_Cur[QB_PH_EXTRACT] = ms(m_tExtractBegin, m_tExtractEnd);
      m_Cur[QB_PH_BEGINFRAME] = ms(m_tExtractEnd, m_tRenderBegin);
      m_Cur[QB_PH_RENDER] = ms(m_tRenderBegin, m_tRenderEnd);
      m_Cur[QB_PH_PRE_PRESENT] = ms(m_tRenderEnd, m_tPresentEntry);
    }
    else
    {
      // no render events (no view to extract): the whole stretch up to Present goes to PRE_EXTRACT
      m_Cur[QB_PH_PRE_EXTRACT] = ms(m_tPlugExit, m_tPresentEntry);
    }
    m_Cur[QB_PH_PRESENT] = ms(m_tPresentEntry, m_tPresentExit);
    m_Cur[QB_PH_POST_PRESENT] = ms(m_tPresentExit, m_tFinishEntry);
    m_Cur[QB_PH_FINISH] = ms(m_tFinishEntry, m_tFinishExit);
    m_Cur[QB_PH_FRAME_REST] = ms(m_tFinishExit, tEnd);

    for (int i = 0; i < QB_PH_COUNT; i++)
      m_Tot[i] += m_Cur[i];
    for (int i = 0; i < QB_PS_COUNT; i++)
      m_TotSub[i] += m_CurSub[i];
    for (int i = 0; i < 9; i++)
      m_TotExtra[i] += m_ExtraCur[i];
    m_TotFrame += ms(m_tFrame0, tEnd);
    m_iFrames++;
    m_iSteps += m_iCurSteps;
  }
};

/// The one instance (header-only, one translation unit set).
inline QbPhaseState& QbPh()
{
  static QbPhaseState s_State;
  return s_State;
}

/// Names for the report, in QbPhSeg order.
inline const char* QbPhName(int i)
{
  static const char* const s_Names[QB_PH_COUNT] = {"pump_input", "acquire", "pre_update", "before_world_update", "upd_init", "upd_preasync", "upd_jolt_start", "upd_async",
    "upd_jolt_fetch", "upd_postasync", "upd_transform", "upd_posttransform", "upd_tail_and_debuginfos", "after_world_update", "fps_console", "update_plugins", "pre_extract",
    "extract", "extractend_beginframe", "render", "pre_present", "present", "post_present", "finish_frame", "frame_rest"};
  return s_Names[i];
}

/// A world module whose only purpose is to register marker update functions at the first and last position of the synchronous phases
/// (priority sorts descending; Jolt uses -100000 for StartSimulation in PreAsync and +100000 for FetchResults in PostAsync).
/// Created only with --phase-timing=1. The markers do no work besides one stopwatch read.
class QbPhaseModule : public ezWorldModule
{
  EZ_DECLARE_WORLD_MODULE();
  EZ_ADD_DYNAMIC_REFLECTION(QbPhaseModule, ezWorldModule);

public:
  QbPhaseModule(ezWorld* pWorld);
  ~QbPhaseModule();

  virtual void Initialize() override;

private:
  template <int N>
  void Mark(const ezWorldModule::UpdateContext&)
  {
    QbPh().OnMarker(N);
  }

  void AddMark(const char* szName, ezWorldUpdatePhase::Enum phase, float fPriority, const ezWorldModule::UpdateFunction& fn);
};
