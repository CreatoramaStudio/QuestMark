#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/GameApplication/GameApplication.h>

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

protected:
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
