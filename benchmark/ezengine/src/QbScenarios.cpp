#include <Jolt/Jolt.h>

#include <JoltPlugin/System/JoltCollisionFiltering.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <QbGameState.h>
#include <algorithm>
#include <cmath>

void QbGameState::Bound(const ezVec3& vMin, const ezVec3& vMax)
{
  if (!m_bBoundsValid)
  {
    m_vBMin = vMin;
    m_vBMax = vMax;
    m_bBoundsValid = true;
    return;
  }
  m_vBMin = ezVec3(ezMath::Min(m_vBMin.x, vMin.x), ezMath::Min(m_vBMin.y, vMin.y), ezMath::Min(m_vBMin.z, vMin.z));
  m_vBMax = ezVec3(ezMath::Max(m_vBMax.x, vMax.x), ezMath::Max(m_vBMax.y, vMax.y), ezMath::Max(m_vBMax.z, vMax.z));
}

QbStatic QbGameState::AddStaticBox(const ezVec3& vSize, const ezVec3& vPos, ezUInt32 uiGroup)
{
  const JPH::Vec3 vHalf(vSize.x * 0.5f, vSize.y * 0.5f, vSize.z * 0.5f);
  const float fRadius = ezMath::Min(0.05f, ezMath::Min(vHalf.GetX(), ezMath::Min(vHalf.GetY(), vHalf.GetZ())));
  JPH::BodyCreationSettings s(new JPH::BoxShape(vHalf, fRadius), QbToJoltPos(vPos), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
    ezJoltCollisionFiltering::ConstructObjectLayer(0, ezJoltBroadphaseLayer::Static));
  s.mFriction = 0.5f;
  s.mRestitution = 0.1f;
  if (uiGroup != 0xFFFFFFFFu)
    s.mCollisionGroup = JPH::CollisionGroup(m_pJolt->GetGroupFilter(), uiGroup, 0);

  QbStatic res;
  res.m_Id = m_pBI->CreateAndAddBody(s, JPH::EActivation::DontActivate);
  if (m_bRender)
    res.m_hObj = CreateRenderObject(3, vPos, ezQuat::MakeIdentity(), vSize, false);

  Bound(vPos - vSize * 0.5f, vPos + vSize * 0.5f);
  return res;
}

void QbGameState::AddStaticShape(int iKind, const ezVec3& vPos, const ezQuat& qRot)
{
  const JPH::Shape* pShape = (iKind == 0) ? (const JPH::Shape*)m_ShapeBox.GetPtr() : (iKind == 1) ? (const JPH::Shape*)m_ShapeSphere.GetPtr() : (const JPH::Shape*)m_ShapeCapsule.GetPtr();
  JPH::BodyCreationSettings s(pShape, QbToJoltPos(vPos), QbToJoltQuat(qRot), JPH::EMotionType::Static,
    ezJoltCollisionFiltering::ConstructObjectLayer(0, ezJoltBroadphaseLayer::Static));
  s.mFriction = 0.5f;
  s.mRestitution = 0.1f;
  m_pBI->CreateAndAddBody(s, JPH::EActivation::DontActivate);
  if (m_bRender)
    CreateRenderObject(iKind, vPos, qRot, ezVec3(1.0f), false);
}

void QbGameState::BuildPit(double fInterior, double fWallH)
{
  m_fPitInterior = fInterior;
  const float i = (float)fInterior;
  const float wh = (float)fWallH;
  AddStaticBox(ezVec3(i + 2.0f, 1.0f, i + 2.0f), ezVec3(0.0f, -0.5f, 0.0f));
  const float h = (float)(fInterior / 2.0 + 0.5);
  const float wy = (float)(fWallH / 2.0);
  AddStaticBox(ezVec3(1.0f, wh, i + 2.0f), ezVec3(h, wy, 0.0f));
  AddStaticBox(ezVec3(1.0f, wh, i + 2.0f), ezVec3(-h, wy, 0.0f));
  AddStaticBox(ezVec3(i, wh, 1.0f), ezVec3(0.0f, wy, h));
  AddStaticBox(ezVec3(i, wh, 1.0f), ezVec3(0.0f, wy, -h));
}

JPH::BodyID QbGameState::AddDynamic(int iKind, const ezVec3& vPos, const ezQuat& qRot, ezUInt32 uiGroup, ezGameObjectHandle& out_hObj, float fMass, bool bAddNow)
{
  const JPH::Shape* pShape = (iKind == 0) ? (const JPH::Shape*)m_ShapeBox.GetPtr() : (iKind == 1) ? (const JPH::Shape*)m_ShapeSphere.GetPtr() : (const JPH::Shape*)m_ShapeCapsule.GetPtr();
  JPH::BodyCreationSettings s(pShape, QbToJoltPos(vPos), QbToJoltQuat(qRot), JPH::EMotionType::Dynamic,
    ezJoltCollisionFiltering::ConstructObjectLayer(0, ezJoltBroadphaseLayer::Dynamic));
  s.mFriction = 0.5f;
  s.mRestitution = 0.1f;
  s.mLinearDamping = 0.1f;
  s.mAngularDamping = 0.1f;
  s.mAllowSleeping = !m_bSleepingOff;
  s.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
  s.mMassPropertiesOverride.mMass = fMass;
  if (uiGroup != 0xFFFFFFFFu)
    s.mCollisionGroup = JPH::CollisionGroup(m_pJolt->GetGroupFilter(), uiGroup, 0);

  JPH::Body* pBody = m_pBI->CreateBody(s);
  const JPH::BodyID id = pBody->GetID();
  if (bAddNow)
    m_pBI->AddBody(id, JPH::EActivation::Activate);
  else
    m_PendingAdd.push_back(id);

  out_hObj = ezGameObjectHandle();
  if (m_bRender)
  {
    // pieces and bodies use the shared 0.5 box mesh for kind 0, see CreateMeshes
    out_hObj = CreateRenderObject(iKind, vPos, qRot, ezVec3(1.0f), true);
    SetBodyObject(id, out_hObj);
  }
  return id;
}

void QbGameState::FlushAdd()
{
  const size_t uiBatch = 4096;
  for (size_t uiStart = 0; uiStart < m_PendingAdd.size(); uiStart += uiBatch)
  {
    const int iCount = (int)std::min(uiBatch, m_PendingAdd.size() - uiStart);
    JPH::BodyID* pIds = m_PendingAdd.data() + uiStart;
    JPH::BodyInterface::AddState state = m_pBI->AddBodiesPrepare(pIds, iCount);
    m_pBI->AddBodiesFinalize(pIds, iCount, state, JPH::EActivation::Activate);
  }
  m_PendingAdd.clear();
}

double QbGameState::Jit()
{
  return m_Rng.DoubleMinMax(-0.05, 0.05);
}

void QbGameState::BuildPitBodies(bool bRing, int iCount)
{
  int n = 0;
  if (!bRing)
  {
    // 12 x 12 columns (6.6 m span, inside the 22 m pit), layers from y = 1 until iCount bodies
    int iLayer = 0;
    while (n < iCount)
    {
      for (int i = 0; i < 12; i++)
      {
        for (int j = 0; j < 12; j++)
        {
          if (n >= iCount)
            break;
          const double jx = Jit();
          const double jy = Jit();
          const double jz = Jit();
          const ezVec3 p((float)((i - 5.5) * QB_SPACING + jx), (float)(1.0 + iLayer * QB_SPACING + jy), (float)((j - 5.5) * QB_SPACING + jz));
          ezGameObjectHandle h;
          m_Dynamic.push_back(AddDynamic(n % 2, p, ezQuat::MakeIdentity(), 0xFFFFFFFFu, h));
          n++;
        }
      }
      iLayer++;
    }
    Bound(ezVec3(-3.6f, 0.7f, -3.6f), ezVec3(3.6f, (float)(1.0 + (iLayer - 1) * QB_SPACING + 0.3), 3.6f));
    return;
  }

  // mixed: columns on a +-9 m grid, skipping |x|<6 and |z|<6 (chain area), layers until iCount bodies
  std::vector<double> colX, colZ;
  const int iSteps = (int)(9.0 / QB_SPACING);
  for (int i = -iSteps; i <= iSteps; i++)
  {
    for (int j = -iSteps; j <= iSteps; j++)
    {
      const double x = i * QB_SPACING;
      const double z = j * QB_SPACING;
      if (std::fabs(x) < 6.0 && std::fabs(z) < 6.0)
        continue;
      colX.push_back(x);
      colZ.push_back(z);
    }
  }
  int iLayer = 0;
  while (n < iCount)
  {
    for (size_t c = 0; c < colX.size(); c++)
    {
      if (n >= iCount)
        break;
      const double jx = Jit();
      const double jy = Jit();
      const double jz = Jit();
      const ezVec3 p((float)(colX[c] + jx), (float)(1.0 + iLayer * QB_SPACING + jy), (float)(colZ[c] + jz));
      ezGameObjectHandle h;
      m_Dynamic.push_back(AddDynamic(n % 2, p, ezQuat::MakeIdentity(), 0xFFFFFFFFu, h));
      n++;
    }
    iLayer++;
  }
  const float e = (float)(iSteps * QB_SPACING + 0.3);
  Bound(ezVec3(-e, 0.7f, -e), ezVec3(e, (float)(1.0 + (iLayer - 1) * QB_SPACING + 0.3), e));
}

void QbGameState::BuildRayStatics()
{
  // content volume (SPEC "Camera"): 40 x 20 x 40 volume of the statics plus 0.5 m for the shape extents
  Bound(ezVec3(-20.5f, -0.5f, -20.5f), ezVec3(20.5f, 20.5f, 20.5f));
  for (int i = 0; i < 2000; i++)
  {
    const double px = m_Rng.DoubleMinMax(-20.0, 20.0);
    const double py = m_Rng.DoubleMinMax(0.0, 20.0);
    const double pz = m_Rng.DoubleMinMax(-20.0, 20.0);
    const double ax = m_Rng.DoubleMinMax(-1.0, 1.0);
    const double ay = m_Rng.DoubleMinMax(-1.0, 1.0);
    const double az = m_Rng.DoubleMinMax(-1.0, 1.0);
    ezVec3 axis((float)ax, (float)ay, (float)az);
    if (axis.GetLengthSquared() < 0.0001f)
      axis = ezVec3(0.0f, 1.0f, 0.0f);
    const double angle = m_Rng.DoubleMinMax(0.0, QB_TAU);
    const ezQuat q = ezQuat::MakeFromAxisAndAngle(axis.GetNormalized(), ezAngle::MakeFromRadian((float)angle));
    const int iKind = (i % 3 == 0) ? 0 : (i % 3 == 1) ? 2 : 1; // box, capsule, sphere
    AddStaticShape(iKind, ezVec3((float)px, (float)py, (float)pz), q);
  }
}

void QbGameState::BuildRays()
{
  m_RayFrom.resize(m_iRayCount);
  m_RayDir.resize(m_iRayCount);
  for (int i = 0; i < m_iRayCount; i++)
  {
    const double ox = m_Rng.DoubleMinMax(-20.0, 20.0);
    const double oy = m_Rng.DoubleMinMax(0.0, 20.0);
    const double oz = m_Rng.DoubleMinMax(-20.0, 20.0);
    const double dx = m_Rng.DoubleMinMax(-1.0, 1.0);
    const double dy = m_Rng.DoubleMinMax(-1.0, 1.0);
    const double dz = m_Rng.DoubleMinMax(-1.0, 1.0);
    ezVec3 d((float)dx, (float)dy, (float)dz);
    if (d.GetLengthSquared() < 0.0001f)
      d = ezVec3(0.0f, -1.0f, 0.0f);
    m_RayFrom[i] = ezVec3((float)ox, (float)oy, (float)oz);
    m_RayDir[i] = d.GetNormalized();
  }
}

void QbGameState::BuildChains()
{
  // chain volume (SPEC "Camera"): links hang x, z within +-(4.5 + 0.15), y from 12 down to 7
  Bound(ezVec3(-4.65f, 7.0f, -4.65f), ezVec3(4.65f, 12.0f, 4.65f));

  // Static ceiling slab 11 x 0.2 x 11, bottom face at y = 12. Joined bodies must not collide (the Godot PinJoint3D default):
  // every link and the ceiling get their own filter id, and the pairs of a joint are switched off in the module's group filter.
  m_uiCeilingGroup = m_pJolt->CreateObjectFilterID();
  const QbStatic ceiling = AddStaticBox(ezVec3(11.0f, 0.2f, 11.0f), ezVec3(0.0f, 12.1f, 0.0f), m_uiCeilingGroup);

  struct Joint
  {
    JPH::BodyID m_A, m_B;
    ezVec3 m_vPin;
  };
  std::vector<Joint> joints;
  joints.reserve(1000);

  for (int cx = 0; cx < 10; cx++)
  {
    for (int cz = 0; cz < 10; cz++)
    {
      const float x = (float)((cx - 4.5) * 1.0);
      const float z = (float)((cz - 4.5) * 1.0);
      JPH::BodyID prev = ceiling.m_Id;
      ezUInt32 uiPrevGroup = m_uiCeilingGroup;
      for (int l = 0; l < 10; l++)
      {
        const ezUInt32 uiGroup = m_pJolt->CreateObjectFilterID();
        m_pJolt->EnableJoinedBodiesCollisions(uiPrevGroup, uiGroup, false);
        ezGameObjectHandle h;
        const JPH::BodyID link = AddDynamic(2, ezVec3(x, (float)(12.0 - 0.25 - 0.5 * l), z), ezQuat::MakeIdentity(), uiGroup, h);
        m_Dynamic.push_back(link);
        m_ChainLinks.push_back(link);
        joints.push_back({prev, link, ezVec3(x, (float)(12.0 - 0.5 * l), z)});
        prev = link;
        uiPrevGroup = uiGroup;
        if (l == 9)
          m_ImpulseLinks.push_back(link);
      }
    }
  }

  // constraints need the bodies to exist in the system
  FlushAdd();
  const JPH::BodyLockInterface& locks = m_pSystem->GetBodyLockInterfaceNoLock();
  for (const Joint& j : joints)
  {
    JPH::PointConstraintSettings cs;
    cs.mSpace = JPH::EConstraintSpace::WorldSpace;
    cs.mPoint1 = cs.mPoint2 = QbToJoltPos(j.m_vPin);
    JPH::Body* pA = locks.TryGetBody(j.m_A);
    JPH::Body* pB = locks.TryGetBody(j.m_B);
    m_pSystem->AddConstraint(cs.Create(*pA, *pB));
  }
}

void QbGameState::BuildDebrisWorld()
{
  // 25 intact blocks 2 x 2 x 2, 5 x 5 grid, spacing 3, centred on the pit, standing on the floor (centre y = 1)
  m_Blocks.assign(25, QbStatic());
  m_LiveAt.assign(25, 0);
  for (int i = 0; i < 5; i++)
  {
    for (int j = 0; j < 5; j++)
      m_Blocks[i * 5 + j] = AddStaticBox(ezVec3(2.0f, 2.0f, 2.0f), ezVec3((i - 2) * 3.0f, 1.0f, (j - 2) * 3.0f));
  }
  m_BlockOrder.resize(25);
  ReshuffleBlocks();

  if (m_bPooled)
  {
    // enough pieces for the maximum number of events alive at once (lifetime 8 s) plus slack; parked pieces stay in the simulation, asleep, far below the pit
    const long long iEventsAlive = (long long)std::ceil(m_Opt.m_fRate * 8.0) + 2;
    m_iPoolSize0 = iEventsAlive * 64;
    for (long long i = 0; i < m_iPoolSize0; i++)
    {
      QbPiece piece = MakePiece(ezVec3((float)(i % 64), -1000.0f, (float)(i / 64)), true);
      piece.m_vPark = ezVec3((float)(i % 64), -1000.0f, (float)(i / 64));
      m_PoolFree.push_back(piece);
    }
  }
}

void QbGameState::ReshuffleBlocks()
{
  // seeded permutation of the 25 blocks (Fisher-Yates with the benchmark RNG), reshuffled after each 25 draws
  for (int i = 0; i < 25; i++)
    m_BlockOrder[i] = i;
  for (int i = 24; i > 0; i--)
  {
    const int k = m_Rng.IntMinMax(0, i);
    std::swap(m_BlockOrder[i], m_BlockOrder[k]);
  }
}

QbPiece QbGameState::MakePiece(const ezVec3& vPos, bool bAsleep)
{
  QbPiece piece;
  piece.m_vPark = vPos;
  piece.m_Id = AddDynamic(0, vPos, ezQuat::MakeIdentity(), 0xFFFFFFFFu, piece.m_hObj, 0.125f, true);
  if (bAsleep)
    m_pBI->DeactivateBody(piece.m_Id);
  return piece;
}
