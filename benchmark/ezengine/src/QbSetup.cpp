#include <Jolt/Jolt.h>

#include <Core/Graphics/Geometry.h>
#include <Foundation/Threading/TaskSystem.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <Foundation/Application/Application.h>
#include <Foundation/Configuration/CVar.h>
#include <Core/Utils/Blackboard.h>
#include <Foundation/Logging/Log.h>
#include <JoltPlugin/Components/JoltSettingsComponent.h>
#include <JoltPlugin/Resources/JoltMaterial.h>
#include <JoltPlugin/System/JoltCollisionFiltering.h>
#include <JoltPlugin/System/JoltCore.h>
#include <RendererCore/Lights/AmbientLightComponent.h>
#include <RendererCore/Lights/DirectionalLightComponent.h>
#include <RendererCore/Meshes/MeshComponent.h>
#include <RendererCore/Meshes/MeshResourceDescriptor.h>
#include <RendererFoundation/Device/Device.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <QbGameState.h>
#include <cmath>
#include <cstdio>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(QbGameState, 1, ezRTTIDefaultAllocator<QbGameState>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

// ---- --phase-timing: marker world module and render event handlers (only used when the option is on) ----
// clang-format off
EZ_IMPLEMENT_WORLD_MODULE(QbPhaseModule);

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(QbPhaseModule, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

QbPhaseModule::QbPhaseModule(ezWorld* pWorld)
  : ezWorldModule(pWorld)
{
}

QbPhaseModule::~QbPhaseModule() = default;

void QbPhaseModule::AddMark(const char* szName, ezWorldUpdatePhase::Enum phase, float fPriority, const ezWorldModule::UpdateFunction& fn)
{
  ezWorldModule::UpdateFunctionDesc desc(fn, szName);
  desc.m_Phase = phase;
  desc.m_fPriority = fPriority;
  desc.m_bOnlyUpdateWhenSimulating = false;
  RegisterUpdateFunction(desc);
}

void QbPhaseModule::Initialize()
{
  // Sorting inside a phase: higher priority first, then by name. Jolt: StartSimulation PreAsync -100000, FetchResults PostAsync +100000, every other
  // update function in the engine uses |priority| <= 10000 (grep of m_fPriority in Engine and EnginePlugins). The async phase gets no marker (tasks).
  const float fFirst = 1.0e9f, fLast = -1.0e9f;
  AddMark("QbPhase.0.PreFirst", ezWorldUpdatePhase::PreAsync, fFirst, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<0>, this));
  AddMark("QbPhase.1.PreBeforeJolt", ezWorldUpdatePhase::PreAsync, -99999.0f, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<1>, this));
  AddMark("QbPhase.2.PreLast", ezWorldUpdatePhase::PreAsync, fLast, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<2>, this));
  AddMark("QbPhase.3.PostFirst", ezWorldUpdatePhase::PostAsync, fFirst, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<3>, this));
  AddMark("QbPhase.4.PostAfterFetch", ezWorldUpdatePhase::PostAsync, 99999.0f, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<4>, this));
  AddMark("QbPhase.5.PostLast", ezWorldUpdatePhase::PostAsync, fLast, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<5>, this));
  AddMark("QbPhase.6.TransFirst", ezWorldUpdatePhase::PostTransform, fFirst, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<6>, this));
  AddMark("QbPhase.7.TransLast", ezWorldUpdatePhase::PostTransform, fLast, ezWorldModule::UpdateFunction(&QbPhaseModule::Mark<7>, this));
}

static void QbOnExtractionEvent(const ezRenderWorldExtractionEvent& e)
{
  QbPhaseState& s = QbPh();
  if (e.m_Type == ezRenderWorldExtractionEvent::Type::BeginExtraction)
    s.m_tExtractBegin = ezTime::Now();
  else if (e.m_Type == ezRenderWorldExtractionEvent::Type::EndExtraction)
  {
    s.m_tExtractEnd = ezTime::Now();
    s.m_bSawExtract = true;
  }
}

static void QbOnRenderEvent(const ezRenderWorldRenderEvent& e)
{
  QbPhaseState& s = QbPh();
  if (e.m_Type == ezRenderWorldRenderEvent::Type::BeginRender)
    s.m_tRenderBegin = ezTime::Now();
  else if (e.m_Type == ezRenderWorldRenderEvent::Type::EndRender)
  {
    s.m_tRenderEnd = ezTime::Now();
    s.m_bSawRender = true;
  }
}

static std::string QbToStd(ezStringView sView)
{
  return std::string(sView.GetStartPointer(), sView.GetElementCount());
}

static void QbSetBoolCVar(const char* szName, bool bValue)
{
  ezCVar* pVar = ezCVar::FindCVarByName(szName);
  if (pVar != nullptr && pVar->GetType() == ezCVarType::Bool)
    *static_cast<ezCVarBool*>(pVar) = bValue;
  else
    ezLog::Warning("cvar '{}' not found or not a bool", szName);
}

static void QbSetIntCVar(const char* szName, int iValue, bool bDelayed)
{
  ezCVar* pVar = ezCVar::FindCVarByName(szName);
  if (pVar != nullptr && pVar->GetType() == ezCVarType::Int)
  {
    *static_cast<ezCVarInt*>(pVar) = iValue;
    if (bDelayed)
      pVar->SetToDelayedSyncValue();
  }
  else
    ezLog::Warning("cvar '{}' not found or not an int", szName);
}

static void QbSetFloatCVar(const char* szName, float fValue)
{
  ezCVar* pVar = ezCVar::FindCVarByName(szName);
  if (pVar != nullptr && pVar->GetType() == ezCVarType::Float)
    *static_cast<ezCVarFloat*>(pVar) = fValue;
  else
    ezLog::Warning("cvar '{}' not found or not a float", szName);
}

static std::string QbFmt(const char* szFormat, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0)
{
  char buf[1024];
  snprintf(buf, sizeof(buf), szFormat, a, b, c, d);
  return buf;
}

QbGameState::QbGameState()
{
  QbParseOptions(m_Opt);
}

QbGameState::~QbGameState() = default;

void QbGameState::OnActivation(ezWorld* pWorld, ezStringView sStartPosition, const ezTransform& startPositionOffset)
{
  (void)pWorld;
  m_tRunStart = ezTime::Now();

  ResolveScenario();
  if (m_bBadScenario)
  {
    if (ezApplication* pApp = ezApplication::GetApplicationInstance())
      pApp->SetReturnCode(2);
    m_bFinished = true;
    m_bQuitIssued = true;
    SUPER::OnActivation(nullptr, sStartPosition, startPositionOffset);
    RequestQuit("bad scenario");
    return;
  }

  ApplyCVars();
  CreateWorldAndJolt();
  BuildScene();

  SUPER::OnActivation(m_pOwnedWorld.Borrow(), sStartPosition, startPositionOffset);

  // the device exists now (Init_SetupGraphicsDevice runs before the game state is activated); read back what is really in use
  if (ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice())
  {
    m_sRenderBackend = QbToStd(pDevice->GetRenderer());
    m_sAdapter = QbToStd(pDevice->GetCapabilities().m_sAdapterName);
  }

  const ezTime tNow = ezTime::Now();
  m_tLastEntry = tNow;
  m_tPhaseStart = tNow;
}

void QbGameState::OnDeactivation()
{
  SUPER::OnDeactivation();

  if (m_bStepCounterOn && m_pSystem != nullptr)
    m_pSystem->RemoveStepListener(&m_StepCounter); // the system still exists here: it dies with the world module in m_pOwnedWorld.Clear() below
  m_bStepCounterOn = false;
  m_ShapeBox = nullptr;
  m_ShapeSphere = nullptr;
  m_ShapeCapsule = nullptr;
  m_pJolt = nullptr;
  m_pSystem = nullptr;
  m_pBI = nullptr;
  m_pOwnedWorld.Clear();
}

void QbGameState::ApplyCVars()
{
  // the module's own stepping stays off: bodies are stepped by TickWork (PhysicsSystem::Update) so that the step can be timed alone, see NOTES.md
  QbSetBoolCVar("Jolt.Simulation.Pause", !m_bComponents); // components path: the module steps and syncs itself
  QbSetBoolCVar("Rendering.Multithreading", m_Opt.m_bMtRender);
  QbSetBoolCVar("App.VSync", m_bVsync);
  QbSetFloatCVar("App.RenderScale", 1.0f);

  if (m_bRender)
  {
    const int iMsaa = m_Opt.m_iMsaa >= 8 ? 3 : m_Opt.m_iMsaa >= 4 ? 2 : m_Opt.m_iMsaa >= 2 ? 1 : 0; // 0 off, 1 2x, 2 4x, 3 8x
    QbSetIntCVar("Rendering.Pipeline.MSAA", iMsaa, false);
    // SPEC "shadow map size" is the directional atlas size (Godot semantics): 4 cascades share it, so a cascade is a quarter of the area
    QbSetIntCVar("Rendering.Shadows.AtlasSize", m_Opt.m_iShadowMap, true);
    QbSetIntCVar("Rendering.Shadows.MaxShadowMapSize", m_Opt.m_iShadowMap / 2, true);
  }
}

void QbGameState::ResolveScenario()
{
  m_fWarmup = m_Opt.m_fWarmup;
  m_fMeasure = m_Opt.m_fMeasure;

  if (m_Opt.m_sProfile == "game" || m_Opt.m_sProfile == "stress")
    m_sProfile = m_Opt.m_sProfile;
  else
  {
    ezLog::Error("bad --profile value (game|stress): {}", m_Opt.m_sProfile.c_str());
    m_sProfile = "game";
  }

  std::string sSc = m_Opt.m_sScenario;
  std::string sMode = m_Opt.m_sMode;
  bool bUnpaced = m_Opt.m_bUnpaced;
  m_bUnlockRender = false;
  m_bDebris = false;
  m_bPooled = false;
  m_sFileLabel = sSc;

  if (sSc == "unlock-render-mixed")
  {
    sSc = "mixed";
    sMode = "render";
    bUnpaced = false;
    m_bUnlockRender = true;
  }
  else if (sSc == "unlock-physics-mixed")
  {
    sSc = "mixed";
    sMode = "physics";
    bUnpaced = true;
  }
  else if (sSc == "unlock-physics-pit-2000")
  {
    sSc = "pit-2000";
    sMode = "physics";
    bUnpaced = true;
  }
  else if (sSc == "debris" || sSc == "debris-pooled")
  {
    m_bPooled = (sSc == "debris-pooled");
    m_bDebris = true;
    bUnpaced = false;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s-%g", sSc.c_str(), m_Opt.m_fRate);
    m_sFileLabel = buf;
    sSc = "debris";
    if (!m_Opt.m_bWarmupGiven)
      m_fWarmup = 10.0;
  }
  else if (sSc == "scale")
  {
    if (bUnpaced)
      sMode = "physics";
    char buf[64];
    snprintf(buf, sizeof(buf), "scale-%d", m_Opt.m_iBodies);
    m_sFileLabel = buf;
  }
  else if (sSc == "pit-2000" || sSc == "raycast-1000" || sSc == "raycast-10000" || sSc == "chains" || sSc == "mixed")
  {
    bUnpaced = false;
  }
  else
  {
    ezLog::Error("unknown scenario '{}'", m_Opt.m_sScenario.c_str()); // every other bad value logs its own message
    m_bBadScenario = true;
    return;
  }

  m_sBase = sSc;
  m_bUnpaced = bUnpaced;
  m_bComponents = (m_Opt.m_sPhysicsPath == "components");
  if (m_Opt.m_bBadOption)
  {
    ezLog::Error("bad --comp-unpaced value (slice|frame): {}", m_Opt.m_sCompUnpaced.c_str());
    m_bBadScenario = true;
    return;
  }
  m_bCompSlice = m_bComponents && m_bUnpaced && m_Opt.m_sCompUnpaced != "frame"; // --comp-unpaced=slice (default) | frame
  if (m_Opt.m_sPhysicsPath != "raw" && !m_bComponents)
  {
    ezLog::Error("bad --physics-path value (raw|components): {}", m_Opt.m_sPhysicsPath.c_str());
    m_bBadScenario = true;
    return;
  }
  m_bRender = (sMode == "render");
  m_sFileMode = (sSc == "scale" && bUnpaced) ? std::string("unlock-physics") : sMode;

  const bool bThroughput = (sSc == "pit-2000" || sSc == "mixed" || sSc == "scale");
  if (m_Opt.m_sSleeping == "off")
    m_bSleepingOff = true;
  else if (m_Opt.m_sSleeping == "default")
    m_bSleepingOff = false;
  else
    m_bSleepingOff = (m_sProfile == "stress" && bThroughput);

  m_bShadows = (m_Opt.m_sShadows != "off");
  m_bVsync = (m_Opt.m_sVsync == "on");

  if (m_Opt.m_sCamera == "content" || m_Opt.m_sCamera == "sky")
    m_sCamera = m_Opt.m_sCamera;
  else
  {
    ezLog::Error("bad --camera value (content|sky): {}", m_Opt.m_sCamera.c_str());
    m_sCamera = "content";
  }

  m_sRayApi = (m_Opt.m_sRayApi == "jolt") ? "jolt" : "engine";

  // body limit: ez derives contacts (4 x) and pairs (40 x) from it, see NOTES.md
  if (m_Opt.m_iMaxBodies > 0)
    m_iMaxBodies = m_Opt.m_iMaxBodies;
  else if (sSc == "scale" || sSc == "debris")
    m_iMaxBodies = 70000;
  else
    m_iMaxBodies = 10240;
  if (sSc == "scale" && m_iMaxBodies < m_Opt.m_iBodies + 1000)
    m_iMaxBodies = m_Opt.m_iBodies + 1000;

  m_iRayCount = (sSc == "raycast-10000") ? 10000 : (sSc == "raycast-1000" || sSc == "mixed") ? 1000 : 0;
  m_bTickScript = m_bDebris || m_iRayCount > 0 || sSc == "chains" || sSc == "mixed";
}

void QbGameState::CreateWorldAndJolt()
{
  ezWorldDesc worldDesc("QuestBench");
  m_pOwnedWorld = EZ_DEFAULT_NEW(ezWorld, worldDesc);

  {
    EZ_LOCK(m_pOwnedWorld->GetWriteMarker());

    // the settings component must exist before the Jolt module is created: the module reads gravity and the body limit once, in Initialize()
    ezGameObjectDesc objDesc;
    ezGameObject* pObj = nullptr;
    m_pOwnedWorld->CreateObject(objDesc, pObj);
    ezJoltSettingsComponent* pSettings = nullptr;
    ezJoltSettingsComponent::CreateComponent(pObj, pSettings);
    pSettings->SetObjectGravity(ezVec3(0.0f, -9.81f, 0.0f));
    pSettings->SetMaxBodies((ezUInt32)m_iMaxBodies);
    pSettings->SetSleepVelocityThreshold(m_Opt.m_fSleepVel > 0.0f ? m_Opt.m_fSleepVel : 0.03f); // Jolt default, equal to the Godot Jolt default
    if (m_bComponents)
    {
      // the module steps inside ezWorld::Update: Fixed mode at 200 Hz, the benchmark accumulator drives the world clock, one world update per step (RunPaced)
      if (m_bUnpaced && !m_bCompSlice)
        pSettings->SetSteppingMode(ezJoltSteppingMode::Variable); // one step per world update with the world clock time diff (fixed at 1/200 s below)
      else
        pSettings->SetSteppingMode(ezJoltSteppingMode::Fixed);
      pSettings->SetFixedFrameRate((float)QB_PHYS_HZ);
      pSettings->SetMaxSubSteps(QB_MAX_STEPS_PER_FRAME);
    }

    if (m_bCompSlice)
      SetClockSteps(0); // slice mode: Fixed stepping like the paced path; the benchmark sets 1/200 s + 1 ns per step update (RunUnpacedComp), a 1 ns step makes no module step
    else if (m_bComponents && m_bUnpaced)
      m_pOwnedWorld->GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(QB_DT)); // every update advances the world by exactly one physics step (--comp-unpaced=frame)
    m_pJolt = m_pOwnedWorld->GetOrCreateModule<ezJoltWorldModule>();
    m_pSystem = m_pJolt->GetJoltSystem();
    m_pBI = &m_pSystem->GetBodyInterfaceNoLock();
    if (m_bComponents && m_Opt.m_bCountJoltSteps)
    {
      // --count-jolt-steps=1 only (diagnostic): a registered listener disables Jolt's early return when no body is active and adds one job per step,
      // so default timing runs have no listener. Removed in OnDeactivation before the world (and the system) is destroyed.
      m_pSystem->AddStepListener(&m_StepCounter);
      m_bStepCounterOn = true;
    }
    if (m_Opt.m_bPhaseTiming)
    {
      // marker update functions in every synchronous phase and render event handlers; nothing of this exists without the option
      QbPh().m_bOn = true;
      m_pOwnedWorld->GetOrCreateModule<QbPhaseModule>();
      static ezEventSubscriptionID s_uiExtractionSub = 0, s_uiRenderSub = 0;
      s_uiExtractionSub = ezRenderWorld::GetExtractionEvent().AddEventHandler(ezMakeDelegate(&QbOnExtractionEvent));
      s_uiRenderSub = ezRenderWorld::GetRenderEvent().AddEventHandler(ezMakeDelegate(&QbOnRenderEvent));
      (void)s_uiExtractionSub;
      (void)s_uiRenderSub;
      if (m_Opt.m_bMtRender)
        ezLog::Warning("--phase-timing with --mt-render=1: update, extraction and render overlap, the serial segments are not valid");
    }
    if (m_Opt.m_fSleepTime > 0.0f)
    {
      JPH::PhysicsSettings js = m_pSystem->GetPhysicsSettings();
      js.mTimeBeforeSleep = m_Opt.m_fSleepTime;
      m_pSystem->SetPhysicsSettings(js);
    }
    m_sSpaceClass = QbToStd(m_pOwnedWorld->GetOrCreateModule<ezPhysicsWorldModuleInterface>()->GetDynamicRTTI()->GetTypeName());
  }

  // Friction 0.5 and restitution 0.1 (SPEC): the ez contact listener ignores the body values and combines the ezJoltMaterial of the two shapes
  // (sqrt of the frictions, max of the restitutions). Shapes without a material use the default one, so that one is changed (this process only).
  ezJoltMaterial* pDefaultMat = const_cast<ezJoltMaterial*>(ezJoltCore::GetDefaultMaterial());
  pDefaultMat->m_fFriction = 0.5f;
  pDefaultMat->m_fRestitution = 0.1f;

  m_ShapeBox = new JPH::BoxShape(JPH::Vec3(0.25f, 0.25f, 0.25f));
  m_ShapeSphere = new JPH::SphereShape(0.25f);
  m_ShapeCapsule = new JPH::CapsuleShape(0.1f, 0.15f); // half height of the cylinder part 0.1 (total height 0.5), radius 0.15
}

void QbGameState::BuildScene()
{
  m_Rng.Initialize(QB_SEED);

  if (m_bRender)
    CreateMeshes();

  if (m_sBase == "pit-2000")
  {
    BuildPit(22.0);
    BuildPitBodies(false);
  }
  else if (m_sBase == "scale")
  {
    const int N = m_Opt.m_iBodies;
    const int iInterior = ezMath::Max(22, (int)std::ceil(22.0 * std::sqrt((double)N / 2000.0) / 2.0) * 2);
    const int iWallH = ezMath::Max(6, (int)std::ceil((1.0 + (double)((N + 143) / 144) * 0.6) * 0.5));
    BuildPit(iInterior, iWallH);
    BuildPitBodies(false, N);
  }
  else if (m_sBase == "debris")
  {
    BuildPit(22.0);
    BuildDebrisWorld();
  }
  else if (m_sBase == "raycast-1000" || m_sBase == "raycast-10000")
  {
    BuildRayStatics();
    BuildRays();
  }
  else if (m_sBase == "chains")
  {
    BuildChains();
  }
  else if (m_sBase == "mixed")
  {
    BuildPit(32.0);
    BuildChains();
    BuildPitBodies(true);
    BuildRays();
  }

  FlushAdd();
  m_pSystem->OptimizeBroadPhase(); // the module does this itself only for bodies that went through its own queue

  ComputeCamera();
  if (m_bRender)
    SetupRenderScene();
}

void QbGameState::ComputeCamera()
{
  const ezVec3 c = (m_vBMin + m_vBMax) * 0.5f;
  m_fCamRadius = (double)(m_vBMax - m_vBMin).GetLength() * 0.5;
  const ezVec3 u = ezVec3(0.0f, 1.5f, 0.9f).GetNormalized();
  const double d = 1.15 * m_fCamRadius / std::sin(30.0 * 3.14159265358979323846 / 180.0);
  m_vCamPos = c + u * (float)d;
  m_vCamLookAt = (m_sCamera == "sky") ? m_vCamPos + ezVec3(0.0f, 1.0f, 0.0f) : c;
  m_fShadowDist = std::ceil(m_fCamRadius * 1.15 / std::sin(30.0 * 3.14159265358979323846 / 180.0) + m_fCamRadius);
}

void QbGameState::CreateMeshes()
{
  auto makeMesh = [](const char* szName, ezGeometry& ref_geom) -> ezMeshResourceHandle
  {
    ref_geom.TriangulatePolygons();
    ref_geom.ComputeTangents();

    ezMeshResourceDescriptor desc;
    desc.SetMaterial(0, "Materials/Bench.ezMaterial");
    desc.MeshBufferDesc().AddCommonStreams();
    desc.MeshBufferDesc().AllocateStreamsFromGeometry(ref_geom, ezGALPrimitiveTopology::Triangles);
    desc.AddSubMesh(desc.MeshBufferDesc().GetPrimitiveCount(), 0, 0);
    desc.ComputeBounds();
    return ezResourceManager::GetOrCreateResource<ezMeshResource>(szName, std::move(desc), szName);
  };

  ezGeometry::GeoOptions opt;
  opt.m_MainAxis = ezBasisAxis::PositiveY;

  {
    ezGeometry g;
    g.AddBox(ezVec3(0.5f), false, opt);
    m_hMesh[0] = makeMesh("QbBox05", g);
  }
  {
    ezGeometry g;
    g.AddStackedSphere(0.25f, 32, 16, opt);
    m_hMesh[1] = makeMesh("QbSphere025", g);
  }
  {
    ezGeometry g;
    g.AddCapsule(0.15f, 0.2f, 32, 8, opt); // cylinder part 0.2, total height 0.5
    m_hMesh[2] = makeMesh("QbCapsule", g);
  }
  {
    ezGeometry g;
    g.AddBox(ezVec3(1.0f), false, opt);
    m_hMesh[3] = makeMesh("QbUnitBox", g);
  }
}

ezGameObjectHandle QbGameState::CreateRenderObject(int iMesh, const ezVec3& vPos, const ezQuat& qRot, const ezVec3& vScale, bool bDynamic)
{
  EZ_LOCK(m_pOwnedWorld->GetWriteMarker());
  ezGameObjectDesc desc;
  desc.m_LocalPosition = vPos;
  desc.m_LocalRotation = qRot;
  desc.m_LocalScaling = vScale;
  desc.m_bDynamic = bDynamic;
  ezGameObject* pObj = nullptr;
  const ezGameObjectHandle hObj = m_pOwnedWorld->CreateObject(desc, pObj);
  ezMeshComponent* pMesh = nullptr;
  ezMeshComponent::CreateComponent(pObj, pMesh);
  pMesh->SetMesh(m_hMesh[iMesh]);
  return hObj;
}

/// Hable filmic curve as in the stock Tonemap.ezShader (FilmicTonemap), used to pre-compensate the clear colour.
static double QbFilmic(double x)
{
  const double A = 0.15, B = 0.35, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

/// Inverse of QbFilmic by bisection (the curve is monotone for x >= 0).
static double QbFilmicInverse(double y)
{
  double lo = 0.0, hi = 1000.0;
  for (int i = 0; i < 200; ++i)
  {
    const double mid = 0.5 * (lo + hi);
    (QbFilmic(mid) < y ? lo : hi) = mid;
  }
  return 0.5 * (lo + hi);
}

void QbGameState::SetupRenderScene()
{
  std::string L;
  {
    EZ_LOCK(m_pOwnedWorld->GetWriteMarker());

    // light direction as in the Godot scene (yaw 30, pitch -50 degrees); an ez directional light shines along its object's +X axis
    const ezVec3 vDir = ezVec3(-0.5f * 0.6427876097f, -0.7660444431f, -0.8660254038f * 0.6427876097f).GetNormalized();
    {
      ezGameObjectDesc desc;
      desc.m_LocalRotation = ezQuat::MakeShortestRotation(ezVec3(1.0f, 0.0f, 0.0f), vDir);
      ezGameObject* pObj = nullptr;
      m_pOwnedWorld->CreateObject(desc, pObj);
      ezDirectionalLightComponent* pLight = nullptr;
      ezDirectionalLightComponent::CreateComponent(pObj, pLight);
      pLight->SetLightColor(ezColorGammaUB(255, 255, 255));
      pLight->SetIntensity((float)m_Opt.m_fSunIntensity);
      pLight->SetCastShadows(m_bShadows);
      pLight->SetNumCascades(4);
      pLight->SetMinShadowRange((float)m_fShadowDist);

      L += std::string("parity.shadows: ") + (pLight->GetCastShadows() ? "on" : "off") + "\n";
      L += std::string("shadows: ") + (pLight->GetCastShadows() ? "on" : "off") + "\n";
      L += "parity.shadow_technique: ezEngine directional cascaded shadow map in a shared shadow atlas (engine default filtering; slope bias " + QbFmt("%.3f", pLight->GetSlopeBias()) + ", constant bias " + QbFmt("%.3f", pLight->GetConstantBias()) + ", penumbra " + QbFmt("%.3f", pLight->GetPenumbraSize()) + ")\n";
      L += "parity.shadow_map_size: atlas " + std::to_string(m_Opt.m_iShadowMap) + ", max shadow map (cascade) " + std::to_string(m_Opt.m_iShadowMap / 2) + " requested via the Rendering.Shadows cvars (the effective cascade size is chosen by the engine and not read back)\n";
      L += "shadow_map_size: " + std::to_string(m_Opt.m_iShadowMap) + "\n";
      L += "parity.shadow_cascades: " + std::to_string(pLight->GetNumCascades()) + "\n";
      L += "parity.shadow_max_distance: " + QbFmt("%.1f", pLight->GetMinShadowRange()) + " (ezDirectionalLightComponent MinShadowRange read back; requested " + QbFmt("%.1f", m_fShadowDist) + "; fade out starts at " + QbFmt("%.2f", pLight->GetFadeOutStart()) + " of the range)\n";
      L += "parity.shadow_bias_normal_bias: not comparable (ez uses slope bias and constant bias, see parity.shadow_technique)\n";
      L += "parity.light_direction: (" + QbFmt("%.4f", vDir.x) + ", " + QbFmt("%.4f", vDir.y) + ", " + QbFmt("%.4f", vDir.z) + ") (object +X rotated onto this direction; not read back)\n";
      L += "parity.light_intensity: " + QbFmt("%.3f", pLight->GetIntensity()) + " (ezDirectionalLightComponent intensity read back; engine default 10 overexposes the default pipeline; the benchmark default is 2, not fitted to the Godot/Stride look)\n";
      L += "parity.light_color: (1.000, 1.000, 1.000) (ezColorGammaUB white)\n";
    }
    {
      ezGameObjectDesc desc;
      ezGameObject* pObj = nullptr;
      m_pOwnedWorld->CreateObject(desc, pObj);
      ezAmbientLightComponent* pAmb = nullptr;
      ezAmbientLightComponent::CreateComponent(pObj, pAmb);
      pAmb->SetTopColor(ezColorGammaUB(255, 255, 255));
      pAmb->SetBottomColor(ezColorGammaUB(255, 255, 255));
      pAmb->SetIntensity((float)m_Opt.m_fAmbientIntensity);
      L += "parity.ambient_color: (1.000, 1.000, 1.000) (top and bottom white)\n";
      L += "parity.ambient_energy: " + QbFmt("%.3f", pAmb->GetIntensity()) + " (ezAmbientLightComponent intensity read back; Godot uses 0.5, engine default 1; benchmark default 0.5, not fitted)\n";
    }
  }

  ezCamera* pCam = GetMainCamera();
  pCam->SetCameraMode(ezCameraMode::PerspectiveFixedFovY, 60.0f, 0.1f, 2000.0f);
  if (m_sCamera == "sky")
    pCam->LookAt(m_vCamPos, m_vCamLookAt, ezVec3(0.0f, 0.0f, -1.0f)); // forward +Y, up -Z (as the Godot control)
  else
    pCam->LookAt(m_vCamPos, m_vCamLookAt, ezVec3(0.0f, 1.0f, 0.0f));

  const ezVec3 vFwd = pCam->GetCenterDirForwards();
  const int iMsaaCvar = m_Opt.m_iMsaa >= 8 ? 3 : m_Opt.m_iMsaa >= 4 ? 2 : m_Opt.m_iMsaa >= 2 ? 1 : 0;
  const int iMsaaX = m_Opt.m_iMsaa >= 8 ? 8 : m_Opt.m_iMsaa >= 4 ? 4 : m_Opt.m_iMsaa >= 2 ? 2 : 0;
  L += "parity.tonemap: ezEngine default render pipeline (tonemapping and any bloom or ambient occlusion of the stock pipeline are not controlled or read back)\n";
  L += "parity.msaa: " + std::to_string(iMsaaX) + "x (Rendering.Pipeline.MSAA cvar set to " + std::to_string(iMsaaCvar) + ", not read back from the pipeline)\n";
  L += "msaa: " + std::to_string(iMsaaX) + "x\n";
  L += "parity.render_scale: 1.0 (App.RenderScale cvar set to 1)\n";
  L += "parity.material: Materials/Bench.ezMaterial on every mesh (project material with engine default shading; not the Godot/Stride default material)\n";
  L += "parity.cast_shadow_meshes: ezMeshComponent default (cast shadows on)\n";
  L += "camera_forward_read_back: (" + QbFmt("%.4f", vFwd.x) + ", " + QbFmt("%.4f", vFwd.y) + ", " + QbFmt("%.4f", vFwd.z) + ")\n";
  if (m_Opt.m_sBackground == "cornflower")
  {
    // The stock MainRenderPipeline clears its colour sources (ezSourcePass "ColorSource0".."ColorSource3") to a near-black blue, and the scene colour then goes
    // through the TonemapPass (Hable filmic curve, white point 11.2, exposure 1, vignette texture) before it is shown. To get Godot's cornflower blue (0.392, 0.584, 0.929)
    // = sRGB bytes (100, 149, 237) on screen, the clear colour is the inverse of the filmic curve at the target linear colour, and the vignette texture is replaced by the
    // engine's white texture (the vignette darkened the screen corners and made the background non-uniform). Pipeline passes are reflected objects: a world blackboard entry
    // "<pass name>.<property>" overrides the property (ezView::RebuildPropertyMappings), so the engine source and the project data stay untouched.
    const double fWhite = QbFilmic(11.2);
    ezColor clr;
    const ezColor target = ezColorGammaUB(100, 149, 237);
    clr.r = (float)QbFilmicInverse((double)target.r * fWhite);
    clr.g = (float)QbFilmicInverse((double)target.g * fWhite);
    clr.b = (float)QbFilmicInverse((double)target.b * fWhite);
    clr.a = 1.0f;
    ezBlackboard* pBoard = m_pOwnedWorld->GetBlackboard().Borrow();
    for (const char* szPass : {"ColorSource0", "ColorSource1", "ColorSource2", "ColorSource3"})
      pBoard->SetEntryValue(ezStringBuilder(szPass, ".ClearColor"), clr);
    pBoard->SetEntryValue("TonemapPass.VignettingTexture", ezVariant(ezString("White.color")));
    L += "background: cornflower (0.392, 0.584, 0.929) = sRGB bytes (100, 149, 237); stock pipeline ColorSource0..3 ClearColor set via world blackboard to the filmic-tonemap inverse (" + QbFmt("%.4f, %.4f, %.4f", clr.r, clr.g, clr.b) + " linear, scene colour before tonemapping; assumes exposure 1, white point 11.2, no bloom contribution) and TonemapPass vignette texture set to White.color; effective colour checked on a screenshot only";
  }
  else
    L += "background: default (not set: the stock pipeline clear colour is used, near-black blue; Godot and Stride use cornflower blue, see --background=cornflower)";
  m_sRenderEnv = L;
}
