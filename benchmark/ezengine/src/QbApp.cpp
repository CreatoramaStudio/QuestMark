#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/GameApplication/GameApplication.h>

#include <QbPhase.h>
#include <string>

// windows.h before the entry point: the entry point macro declares WinMain with the real Windows types only if it was included already
// (the unity build puts QbReport.cpp, which needs it, into the same translation unit)
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

/// The benchmark application. The game state (QbGameState) is found by the engine through reflection: it is the only
/// non-fallback ezGameState in this executable.
class QbApp : public ezGameApplication
{
public:
  QbApp()
    : ezGameApplication("QuestBench", QB_PROJECT_DIR)
  {
  }

  // ---- --phase-timing: stopwatch around the virtual steps of the frame; every override is a plain pass-through while QbPh().m_bOn is false ----
  virtual void Run() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run();
      return;
    }
    s.FrameBegin();
    ezGameApplication::Run();
    s.FrameEnd();
  }

protected:
  virtual void Run_AcquireImage() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_AcquireImage();
      return;
    }
    s.m_tAcqEntry = ezTime::Now();
    ezGameApplication::Run_AcquireImage();
    s.m_tAcqExit = ezTime::Now();
  }

  virtual void Run_BeforeWorldUpdate() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_BeforeWorldUpdate();
      return;
    }
    s.m_tBeforeEntry = ezTime::Now();
    ezGameApplication::Run_BeforeWorldUpdate();
    s.m_tBeforeExit = ezTime::Now();
    s.m_tUpdEnter = s.m_tBeforeExit; // the application's own world update starts here (extra updates of the benchmark ran inside the call above)
    s.m_tUpdPrev = s.m_tBeforeExit;
  }

  virtual void Run_AfterWorldUpdate() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_AfterWorldUpdate();
      return;
    }
    s.m_tAfterEntry = ezTime::Now();
    ezGameApplication::Run_AfterWorldUpdate();
    s.m_tAfterExit = ezTime::Now();
  }

  virtual void Run_UpdatePlugins() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_UpdatePlugins();
      return;
    }
    s.m_tPlugEntry = ezTime::Now();
    ezGameApplication::Run_UpdatePlugins();
    s.m_tPlugExit = ezTime::Now();
  }

  virtual void Run_PresentImage() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_PresentImage();
      return;
    }
    s.m_tPresentEntry = ezTime::Now();
    ezGameApplication::Run_PresentImage();
    s.m_tPresentExit = ezTime::Now();
  }

  virtual void Run_FinishFrame() override
  {
    QbPhaseState& s = QbPh();
    if (!s.m_bOn)
    {
      ezGameApplication::Run_FinishFrame();
      return;
    }
    s.m_tFinishEntry = ezTime::Now();
    ezGameApplication::Run_FinishFrame();
    s.m_tFinishExit = ezTime::Now();
  }

  /// `--project=<folder>` replaces the project folder that was compiled in (needed when the source tree was moved).
  virtual ezResult BeforeCoreSystemsStartup() override
  {
    const ezDynamicArray<ezString>& args = ezCommandLineUtils::GetGlobalInstance()->GetCommandLineArray();
    for (ezUInt32 i = 1; i < args.GetCount(); ++i)
    {
      const std::string a = args[i].GetData();
      if (a.compare(0, 10, "--project=") == 0)
        m_sAppProjectPath = a.substr(10).c_str();
    }
    return ezGameApplication::BeforeCoreSystemsStartup(); // not SUPER: that is the base of ezGameApplication here, which would skip the unattended-mode setup
  }
};

EZ_APPLICATION_ENTRY_POINT(QbApp);
