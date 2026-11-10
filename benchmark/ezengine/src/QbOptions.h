#pragma once

#include <string>
#include <vector>

/// Command line of the benchmark. Same flag names and defaults as the Godot and Stride sides (benchmark/SPEC.md), `--key=value` form only.
/// Extras: --project (project folder override), --ray-api, --mt-render, --max-bodies, --sun-intensity, --ambient-intensity, --background, --slice-ms.
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
  std::string m_sBackground = "cornflower"; ///< --background=cornflower|default: cornflower (default since 2026-11-09) = Godot's (0.392, 0.584, 0.929) via the world blackboard
  bool m_bRawGroup = false;    ///< --raw-group=1: raw path, a unique JPH collision group (module group filter) on every body, as the actor components do; experiment only
  bool m_bRawCompound = false; ///< --raw-compound=1: raw path, every body gets its own shape wrapped in a one-child StaticCompoundShape, as the actor components build it; experiment only
  bool m_bRawUpdateInTask = false; ///< --raw-update-in-task=1: raw path, JPH::PhysicsSystem::Update runs inside an ez task (EarlyThisFrame, like ezJoltWorldModule::Simulate) and the main thread waits for it; experiment only
  bool m_bPhaseTiming = false; ///< --phase-timing=1: per app frame stopwatch segments (pump, update split, extraction, render, present, finish) in the summary (phase_ms lines); both paths use the same segments; needs --mt-render=0; default off adds no stopwatch call
  bool m_bCountJoltSteps = false; ///< --count-jolt-steps=1: components path, registers a JPH::PhysicsStepListener that counts collision steps (jolt_module_steps line); diagnostic only: a registered listener disables Jolt's no-active-bodies early return and adds one job per step, so it changes the timing; default off
  bool m_bChainSample = false; ///< --chain-sample=1: chain scenarios, sample the end-link move once per physics step (chain_peak line; chain_fixed_step is always on, one read per run at QB_CHAIN_FIXED_STEPS); diagnostic only: the sampling (one GetPosition per end link per step) runs inside the timed step work, so default timing runs (both paths) do not sample; default off
  std::string m_sPhysicsPath = "components"; ///< --physics-path=components|raw: components (default since 2026-11-09, the normal way to use the engine, results label "ezEngine") = ezJoltDynamicActorComponent / ezJoltStaticActorComponent / shape components, stepped and synced by ezJoltWorldModule inside ezWorld::Update; raw (results label "ezEngine direct Jolt"; the exe default and every ezEngine run before 2026-11-09) = bodies created and stepped through the Jolt BodyInterface by the benchmark
  bool m_bProfileDump = false; ///< --profile-dump=1: write an ez profiling capture; only Debug and Dev builds have EZ_USE_PROFILING (Foundation/UserConfig.h turns it off for Shipping), so in Shipping the option does nothing
  bool m_bBadOption = false; ///< an option value is invalid (--comp-unpaced): the run ends with an error
  std::string m_sCompUnpaced = "slice"; ///< --comp-unpaced=slice|frame: components path in unpaced scenarios. slice (default) = back-to-back ezWorld::Update calls (one 200 Hz step each, Fixed stepping) in --slice-ms slices, the app frame once per slice, accounted like the raw path; frame = the old behaviour, one whole app frame per step (Variable stepping)
  std::string m_sProjectOverride;
  std::string m_sCmdline; ///< all arguments joined, for env.txt
};

/// Parses ezCommandLineUtils::GetGlobalInstance()->GetCommandLineArray(). Unknown arguments (the engine's own -options) are ignored.
void QbParseOptions(QbOptions& out_opt);
