#pragma once

#include <string>
#include <vector>

/// Command line of the benchmark. Same flag names and defaults as the Godot and Stride sides (benchmark/SPEC.md), `--key=value` form only.
/// Extras: --project (project folder override), --ray-api, --mt-render, --max-bodies, --sun-intensity, --ambient-intensity, --slice-ms.
struct QbOptions
{
  std::string m_sScenario = "pit-2000";
  std::string m_sMode = "physics";
  std::string m_sRun = "1";
  std::string m_sOut = "results";
  double m_fWarmup = 5.0;
  bool m_bWarmupGiven = false;
  double m_fMeasure = 20.0;
  std::string m_sFlavour;
  std::string m_sCamera = "content";
  std::string m_sSleeping; ///< "", "off" or "default"
  float m_fSleepVel = 0.0f;  ///< --sleep-vel=<m/s>: Jolt point velocity sleep threshold (0 = 0.03, the Jolt default); experiment only
  float m_fSleepTime = 0.0f; ///< --sleep-time=<s>: Jolt time before sleep (0 = 0.5, the Jolt default); experiment only
  std::string m_sProfile = "game";
  std::string m_sShadows;  ///< "off" disables, anything else is on
  int m_iShadowMap = 2048;
  int m_iMsaa = 4;
  std::string m_sVsync; ///< "on" enables, otherwise off
  int m_iBodies = 2000;
  double m_fRate = 8.0;
  bool m_bUnpaced = false;
  std::string m_sRayApi = "engine"; ///< engine (ezPhysicsWorldModuleInterface::Raycast) | jolt (NarrowPhaseQuery::CastRay)
  bool m_bMtRender = false;         ///< Rendering.Multithreading cvar; off by default so update and render do not overlap
  int m_iMaxBodies = 0;             ///< 0 = scenario default (10240; 70000 for scale and debris)
  double m_fSunIntensity = 2.0;     ///< engine default is 10 (overexposed with the default pipeline, seen in a screenshot); 2 is a visual choice, not fitted
  double m_fAmbientIntensity = 0.5; ///< engine default is 1; 0.5 as in Godot, not fitted
  double m_fSliceMs = 10.0;         ///< unpaced runs: wall time of one batch of back-to-back steps per world update
  std::string m_sProjectOverride;
  std::string m_sCmdline; ///< all arguments joined, for env.txt
};

/// Parses ezCommandLineUtils::GetGlobalInstance()->GetCommandLineArray(). Unknown arguments (the engine's own -options) are ignored.
void QbParseOptions(QbOptions& out_opt);
