using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Linq;
using System.Text;
using Stride.BepuPhysics;
using Stride.BepuPhysics.Constraints;
using Stride.BepuPhysics.Definitions;
using Stride.BepuPhysics.Definitions.Colliders;
using Stride.Core;
using Stride.Core.Mathematics;
using Stride.Engine;
using Stride.Games;
using Stride.Graphics;
using Stride.Rendering;
using Stride.Rendering.Colors;
using Stride.Rendering.Compositing;
using Stride.Rendering.Lights;
using Stride.Rendering.Materials;
using Stride.Rendering.Materials.ComputeColors;
using Stride.Rendering.ProceduralModels;

namespace StrideBench
{
    public sealed class BenchGame : Game
    {
        private const int Seed = 12345;
        private const float Gravity = -9.81f;

        private struct Row
        {
            public int Frame;
            public double WallMs, PhysicsMs, QueryMs;
            public int Gc0, Gc1, Gc2;
            public double GcPauseMs;
            public int Awake;
            public bool Tick; // at least one Bepu step ran in this Update
            public int Steps;            // Bepu steps that ran in this Update (200 Hz physics under a 60 fps loop: 3 or 4; unpaced: 1)
            public double StepMeanMs;    // physics_step_ms of the row: mean over the steps of this Update (0 when none)
            public double TimeS; // seconds since the start of the measured window
            public double EventMs, DespawnMs; // debris: stopwatch of the fracture events / the removals in this Update, 0 when none
            public bool EventFrame, DespawnFrame;
            public int DebrisLive;
        }

        private readonly Options opts;
        private BepuSimulation sim;
        private const float BodyDamping = 0.1f;
        private float defaultLinearDamping, defaultAngularDamping;
        private BodyComponent ceilingBody; // kinematic, excluded from awake_bodies (Godot's ceiling is static)
        private Scene scene;
        private PhysicsTimerComponent timer;
        private readonly List<BodyComponent> impulseBodies = new List<BodyComponent>();
        private bool impulsesApplied;
        private int staticCount, dynamicCount, jointCount;
        private readonly List<BodyComponent> dynamicBodies = new List<BodyComponent>();

        // render assets
        private Model boxModel, sphereModel, capsuleModel, unitBoxModel;
        private Material material;

        // ray set
        private Vector3[] rayOrigins, rayDirs;
        private long rayHits;

        // measurement
        private readonly List<Row> rows = new List<Row>(1 << 17);
        private long startTs;
        private long lastUpdateTs;
        private bool measuring;
        private long measureStartTs;
        private double gcPauseAtStartMs;
        private int frameIndex;
        private bool finished;
        private string adapterName = "unknown";
        public int ExitCode;

        public BenchGame(Options o)
        {
            opts = o;
            GraphicsDeviceManager.SynchronizeWithVerticalRetrace = o.Render && o.Vsync; // profile game/stress: on for paced render runs, off for unlock/uncapped and physics mode
            if (o.Render)
            {
                GraphicsDeviceManager.PreferredBackBufferWidth = 1280;
                GraphicsDeviceManager.PreferredBackBufferHeight = 720;
            }
            else
            {
                GraphicsDeviceManager.PreferredBackBufferWidth = 320;
                GraphicsDeviceManager.PreferredBackBufferHeight = 180;
            }
            // Fixed 60 Hz display loop (physics runs at opts.PhysicsHz = 200 inside it) (physics mode always; render mode unless --uncapped=1).
            // Scenario E: --scenario=unlock-render-mixed (Unlock) and the unpaced physics variants set IsFixedTimeStep=false (no TargetElapsedTime used).
            // SPEC "Frame rate": the frame loop is unlocked in every mode (render and physics), no TargetElapsedTime; physics is fixed at PhysicsHz inside it.
            IsFixedTimeStep = false;
        }

        protected override void BeginRun()
        {
            base.BeginRun();
            adapterName = GraphicsDevice?.Adapter?.Description ?? "unknown";
            if (!opts.Render && opts.Hidden)
            {
                try { Window.Visible = false; } catch { /* best effort */ }
            }
            InactiveSleepTime = TimeSpan.Zero; // Game default 20 ms sleep per frame while the window is inactive would cap the unlocked loop at about 50 fps
            Window.Title = "StrideBench " + opts.FileScenario + " " + opts.Mode + (opts.Render ? "" : " (physics mode: nothing is drawn by design, use --mode=render to see the scene)");
            if (!opts.Render) Console.Error.WriteLine("StrideBench: physics mode draws nothing (the window stays black by design); use --mode=render to see the scene.");

            var config = Services.GetOrCreate<BepuConfiguration>();
            sim = config.BepuSimulations[0];
            defaultLinearDamping = sim.PoseLinearDamping; defaultAngularDamping = sim.PoseAngularDamping; // plugin defaults, recorded in env.txt
            sim.PoseLinearDamping = BodyDamping; sim.PoseAngularDamping = BodyDamping;   // same as Godot's default 0.1 (SPEC)
            sim.PoseGravity = new Vector3(0, Gravity, 0);
            sim.FixedTimeStep = TimeSpan.FromTicks((long)Math.Round(TimeSpan.TicksPerSecond / opts.PhysicsHz)); // SPEC: 200 Hz physics, display loop stays 60 Hz
            sim.MaxStepPerFrame = 64; // plugin default 3 would drop steps at 200 Hz; with the unlocked loop a frame can need many steps (64 = a 320 ms frame). Dropped steps are checked in the summary (steps_dropped)
            // Unpaced physics: the plugin's PhysicsGameSystem must not step (it would add wall-time steps); this loop calls BepuSimulation.Update(FixedTimeStep) itself, see UnpacedLoop.
            if (opts.Unpaced) sim.Enabled = false;
            if (opts.BepuThreads > 0) sim.ThreadCount = opts.BepuThreads;
            if (opts.RenderThreads > 0) Stride.Core.Threading.Dispatcher.MaxDegreeOfParallelism = opts.RenderThreads;

            scene = new Scene();
            SceneSystem.SceneInstance = new SceneInstance(Services, scene);

            if (opts.Render)
            {
                CreateRenderAssets();
                SetupCameraAndLight();
            }

            var timerEntity = new Entity("physics-timer");
            timer = new PhysicsTimerComponent();
            timer.OnStep = OnStep;
            timerEntity.Add(timer);
            scene.Entities.Add(timerEntity);

            BuildScenario();
            PlaceCamera();

            startTs = Stopwatch.GetTimestamp();
            lastUpdateTs = startTs;
        }

        // ------------------------------------------------------------------ scene building

        private void CreateRenderAssets()
        {
            boxModel = new ProceduralModelDescriptor(new CubeProceduralModel { Size = new Vector3(0.5f) }).GenerateModel(Services);
            sphereModel = new ProceduralModelDescriptor(new SphereProceduralModel { Radius = 0.25f, Tessellation = 16 }).GenerateModel(Services);
            capsuleModel = new ProceduralModelDescriptor(new CapsuleProceduralModel { Length = 0.2f, Radius = 0.15f, Tessellation = 12 }).GenerateModel(Services);
            unitBoxModel = new ProceduralModelDescriptor(new CubeProceduralModel { Size = new Vector3(1f) }).GenerateModel(Services);
            var desc = new MaterialDescriptor
            {
                Attributes =
                {
                    Diffuse = new MaterialDiffuseMapFeature(new ComputeColor(new Color4(1f, 1f, 1f, 1f))),
                    DiffuseModel = new MaterialDiffuseLambertModelFeature(),
                }
            };
            material = Material.New(GraphicsDevice, desc);
        }

        private Entity cameraEntity; // render mode only; logged into env.txt

        // Shared camera rule (SPEC "Camera"): vertical FOV 60, near 0.1, far 2000; look at the centre c of the content bounding sphere (radius R) from u = normalize(0, 1.5, 0.9)
        // at D = 1.15 * R / sin(30 deg). Content = AABB of the scene constants (pit interior floor top to wall top, initial dynamic bodies' spawn volume, ceiling and chains, raycast cloud extents,
        // debris blocks); sphere = centre and half diagonal of that AABB. --camera=sky: same position, looking straight up.
        private Vector3 boundMin = new Vector3(float.MaxValue), boundMax = new Vector3(float.MinValue);
        private Vector3 camPos, camLookAt;
        private float camContentRadius;

        private void Bound(Vector3 lo, Vector3 hi)
        {
            boundMin = Vector3.Min(boundMin, lo); boundMax = Vector3.Max(boundMax, hi);
        }

        private void PlaceCamera()
        {
            var c = (boundMin + boundMax) * 0.5f;
            float r = (boundMax - boundMin).Length() * 0.5f;
            var u = new Vector3(0f, 1.5f, 0.9f); u.Normalize();
            float d = 1.15f * r / (float)Math.Sin(MathUtil.DegreesToRadians(30f));
            camPos = c + u * d; camLookAt = c; camContentRadius = r;
            if (cameraEntity == null) return; // physics mode: values are computed (for env.txt) but there is no camera
            cameraEntity.Transform.Position = camPos;
            // Stride looks down -Z; RotationX(negative) tilts the view down. Look direction -u is atan2(1.5, 0.9) = 59.04 deg down. Sky: +90 = straight up. Yaw 0, roll 0.
            float pitch = opts.CameraSky ? MathUtil.PiOverTwo : -(float)Math.Atan2(u.Y, u.Z);
            cameraEntity.Transform.Rotation = Quaternion.RotationX(pitch);
        }

        private void SetupCameraAndLight()
        {
            var camEntity = new Entity("camera");
            cameraEntity = camEntity;
            var cam = new CameraComponent { NearClipPlane = 0.1f, FarClipPlane = 2000f, VerticalFieldOfView = 60f };
            camEntity.Add(cam);
            // pose is set by PlaceCamera() after BuildScenario (needs the content bounds)
            scene.Entities.Add(camEntity);

            var lightEntity = new Entity("sun");
            var sun = new LightDirectional();
            if (opts.Shadows)
            {
                sun.Shadow.Enabled = true;
                sun.Shadow.Size = LightShadowMapSize.XLarge;
                if (sun.Shadow is LightDirectionalShadowMap dsm) dsm.CascadeCount = LightShadowMapCascadeCount.FourCascades;
            }
            lightEntity.Add(new LightComponent { Type = sun, Intensity = opts.SunIntensity });
            shadowMapSizeText = "size hint " + sun.Shadow.Size + (sun.Shadow is LightDirectionalShadowMap d2 ? ", cascades " + d2.CascadeCount + ", filter " + sun.Shadow.Filter : "");
            lightEntity.Transform.Rotation = Quaternion.RotationYawPitchRoll(MathUtil.DegreesToRadians(30), MathUtil.DegreesToRadians(-50), 0);
            scene.Entities.Add(lightEntity);

            // Ambient fill so the shadow side of walls is not pure black (a render check showed black patches with the sun alone).
            var ambientEntity = new Entity("ambient");
            ambientEntity.Add(new LightComponent { Type = new LightAmbient { Color = new ColorRgbProvider(new Color3(1f, 1f, 1f)) }, Intensity = opts.AmbientIntensity });
            scene.Entities.Add(ambientEntity);

            SceneSystem.GraphicsCompositor = opts.BackgroundCornflower
                ? GraphicsCompositorHelper.CreateDefault(false, camera: cam, clearColor: new Color4(0.392f, 0.584f, 0.929f, 1f))
                : GraphicsCompositorHelper.CreateDefault(false, camera: cam);
            var fwd = FindForward(SceneSystem.GraphicsCompositor.Game);
            if (fwd != null)
            {
                fwd.MSAALevel = opts.Msaa == 1 ? MultisampleCount.None : opts.Msaa == 2 ? MultisampleCount.X2 : opts.Msaa == 4 ? MultisampleCount.X4 : MultisampleCount.X8;
                msaaText = fwd.MSAALevel == MultisampleCount.None ? "off" : fwd.MSAALevel.ToString().Substring(1) + "x";
            }
            else msaaText = "NOT SET (ForwardRenderer not found in the compositor)";
        }

        private string shadowMapSizeText = "n/a", msaaText = "n/a";

        private static ForwardRenderer FindForward(ISceneRenderer r)
        {
            switch (r)
            {
                case ForwardRenderer f: return f;
                case SceneRendererCollection c: foreach (var ch in c.Children) { var x = FindForward(ch); if (x != null) return x; } return null;
                case SceneCameraRenderer cr: return FindForward(cr.Child);
                default: return null;
            }
        }

        private Entity NewEntity(string name, Vector3 pos, Quaternion rot, Model model, Vector3 modelScale)
        {
            var e = new Entity(name);
            e.Transform.Position = pos;
            e.Transform.Rotation = rot;
            if (opts.Render && !opts.NoModels && model != null)
            {
                e.Transform.Scale = modelScale;
                var mc = new ModelComponent(model);
                mc.Materials[0] = material;
                mc.IsShadowCaster = opts.Shadows; // bodies, floor and walls cast shadows (receiving is the default material behaviour)
                e.Add(mc);
            }
            return e;
        }

        private void AddStaticBox(string name, Vector3 center, Vector3 size)
        {
            Bound(center - size * 0.5f, center + size * 0.5f); // every static box (floor, walls, ceiling) is content (camera bounds, same definition as the Godot side)
            var e = NewEntity(name, center, Quaternion.Identity, unitBoxModel, size);
            e.Add(new StaticComponent { Collider = Wrap(new BoxCollider { Size = size }), FrictionCoefficient = 0.5f });
            scene.Entities.Add(e);
            staticCount++;
        }

        private void AddBody(string name, ICollider collider, Model model, Vector3 pos, Quaternion rot)
        {
            var e = NewEntity(name, pos, rot, model, Vector3.One);
            var bc = new BodyComponent { Collider = collider, FrictionCoefficient = 0.5f };
            if (opts.SleepingOff) bc.SleepThreshold = -1f; // negative: the body cannot go to sleep (Stride.BepuPhysics doc)
            dynamicBodies.Add(bc);
            e.Add(bc);
            scene.Entities.Add(e);
            dynamicCount++;
        }

        private static ICollider Wrap(ColliderBase c) { var cc = new CompoundCollider(); cc.Colliders.Add(c); return cc; }
        private static ICollider MkBox() => Wrap(new BoxCollider { Size = new Vector3(0.5f, 0.5f, 0.5f), Mass = 1f });
        private static ICollider MkSphere() => Wrap(new SphereCollider { Radius = 0.25f, Mass = 1f });
        // SPEC: capsule radius 0.15, height 0.5 (read as total height incl. caps, as Godot CapsuleShape3D) -> cylinder length 0.2.
        private static ICollider MkCapsule() => Wrap(new CapsuleCollider { Radius = 0.15f, Length = 0.2f, Mass = 1f });

        private float pitHalf; // interior half size of the pit built last (for the bodies_outside_pit check)

        private void BuildPit(float innerSize, int cols, int skipIdx, Random rng, int count = 2000, float wallH = 6f)
        {
            float half = innerSize * 0.5f;
            pitHalf = half;
            float floorSize = innerSize + 2f;
            AddStaticBox("floor", new Vector3(0, -0.5f, 0), new Vector3(floorSize, 1f, floorSize));
            AddStaticBox("wallE", new Vector3(half + 0.5f, wallH * 0.5f, 0), new Vector3(1f, wallH, floorSize));
            AddStaticBox("wallW", new Vector3(-half - 0.5f, wallH * 0.5f, 0), new Vector3(1f, wallH, floorSize));
            AddStaticBox("wallN", new Vector3(0, wallH * 0.5f, half + 0.5f), new Vector3(innerSize, wallH, 1f));
            AddStaticBox("wallS", new Vector3(0, wallH * 0.5f, -half - 0.5f), new Vector3(innerSize, wallH, 1f));

            const float spacing = 0.6f;
            int placed = 0, lastLayer = 0;
            for (int y = 0; placed < count && y < 100000; y++)
            {
                for (int x = 0; x < cols && placed < count; x++)
                {
                    for (int z = 0; z < cols && placed < count; z++)
                    {
                        float px = (x - (cols - 1) * 0.5f) * spacing;
                        float pz = (z - (cols - 1) * 0.5f) * spacing;
                        // skip |x|<6 and |z|<6, tested on the integer grid index (|idx|<10 <=> |idx*0.6|<6) to avoid float rounding at exactly 6.0
                        if (skipIdx > 0 && Math.Abs(x - (cols - 1) / 2) < skipIdx && Math.Abs(z - (cols - 1) / 2) < skipIdx) continue;
                        float py = 1f + y * spacing;
                        float jx = ((float)rng.NextDouble() * 2f - 1f) * 0.05f;
                        float jy = ((float)rng.NextDouble() * 2f - 1f) * 0.05f;
                        float jz = ((float)rng.NextDouble() * 2f - 1f) * 0.05f;
                        var p = new Vector3(px + jx, py + jy, pz + jz);
                        if ((placed & 1) == 0) AddBody("box" + placed, MkBox(), boxModel, p, Quaternion.Identity);
                        else AddBody("sph" + placed, MkSphere(), sphereModel, p, Quaternion.Identity);
                        placed++;
                    }
                }
                lastLayer = y;
            }
            if (count > 0)
            {
                // spawn volume (camera bounds, same definition as the Godot side): columns within +-((cols - 1) / 2 x 0.6 + 0.3), layers up to y = 1 + (L - 1) x 0.6 + 0.3 (L = layers used)
                float ext = (cols - 1) * 0.5f * spacing + 0.3f;
                Bound(new Vector3(-ext, 0.7f, -ext), new Vector3(ext, 1f + lastLayer * spacing + 0.3f, ext));
            }
        }

        private void BuildRaycastStatics(Random rng)
        {
            Bound(new Vector3(-20.5f, -0.5f, -20.5f), new Vector3(20.5f, 20.5f, 20.5f)); // 40 x 20 x 40 sampling volume of the shape centres plus 0.5 m for the shape extents (camera bounds)
            for (int i = 0; i < 2000; i++)
            {
                var pos = new Vector3((float)rng.NextDouble() * 40f - 20f, (float)rng.NextDouble() * 20f, (float)rng.NextDouble() * 40f - 20f);
                var rot = RandomRotation(rng);
                Entity e;
                switch (i % 3)
                {
                    case 0:
                        e = NewEntity("rbox" + i, pos, rot, boxModel, Vector3.One);
                        e.Add(new StaticComponent { Collider = MkBox(), FrictionCoefficient = 0.5f });
                        break;
                    case 1:
                        e = NewEntity("rcap" + i, pos, rot, capsuleModel, Vector3.One);
                        e.Add(new StaticComponent { Collider = MkCapsule(), FrictionCoefficient = 0.5f });
                        break;
                    default:
                        e = NewEntity("rsph" + i, pos, rot, sphereModel, Vector3.One);
                        e.Add(new StaticComponent { Collider = MkSphere(), FrictionCoefficient = 0.5f });
                        break;
                }
                scene.Entities.Add(e);
                staticCount++;
            }
        }

        private static Quaternion RandomRotation(Random rng)
        {
            // uniform random unit quaternion
            double u1 = rng.NextDouble(), u2 = rng.NextDouble(), u3 = rng.NextDouble();
            double a = Math.Sqrt(1 - u1), b = Math.Sqrt(u1);
            return new Quaternion((float)(a * Math.Sin(2 * Math.PI * u2)), (float)(a * Math.Cos(2 * Math.PI * u2)),
                                  (float)(b * Math.Sin(2 * Math.PI * u3)), (float)(b * Math.Cos(2 * Math.PI * u3)));
        }

        private void BuildChains()
        {
            // Kinematic ceiling bar: constraints in Stride.BepuPhysics take BodyComponents, a StaticComponent cannot be a joint target.
            var barSize = new Vector3(11f, 0.2f, 11f); // 11 x 0.2 x 11 slab, bottom face at y = 12 (same as the Godot side)
            Bound(new Vector3(-4.65f, 7.0f, -4.65f), new Vector3(4.65f, 12.0f, 4.65f)); // links: x, z within +-(4.5 + 0.15), y from 12 down to 12 - 0.25 - 4.5 - 0.25 = 7.0 (camera bounds)
            Bound(new Vector3(-5.5f, 12f, -5.5f), new Vector3(5.5f, 12.2f, 5.5f)); // the slab (camera bounds; Godot adds it through its static-box helper)
            var barEntity = NewEntity("ceiling", new Vector3(0, 12.1f, 0), Quaternion.Identity, unitBoxModel, barSize);
            var bar = new BodyComponent { Collider = Wrap(new BoxCollider { Size = barSize }), Kinematic = true, FrictionCoefficient = 0.5f };
            barEntity.Add(bar);
            scene.Entities.Add(barEntity);
            ceilingBody = bar;
            staticCount++;

            ushort chainId = 0;
            for (int cx = 0; cx < 10; cx++)
            {
                for (int cz = 0; cz < 10; cz++)
                {
                    chainId++;
                    float x = (cx - 4.5f) * 1.0f, z = (cz - 4.5f) * 1.0f;
                    BodyComponent prev = null;
                    for (int l = 0; l < 10; l++)
                    {
                        var pos = new Vector3(x, 12f - 0.25f - 0.5f * l, z);
                        var e = NewEntity("c" + chainId + "_" + l, pos, Quaternion.Identity, capsuleModel, Vector3.One);
                        var body = new BodyComponent { Collider = MkCapsule(), FrictionCoefficient = 0.5f };
                        if (opts.SleepingOff) body.SleepThreshold = -1f;
                        // same chain: neighbours (index diff <= 1) do not collide; different chains have different Ids.
                        body.CollisionGroup = new CollisionGroup { Id = chainId, IndexA = (ushort)l };
                        e.Add(body);
                        var joint = new BallSocketConstraintComponent();
                        if (l == 0)
                        {
                            joint.A = bar;
                            joint.LocalOffsetA = new Vector3(x, -0.1f, z); // anchor on the bottom face (y = 12); bar centre is at x=0,z=0
                        }
                        else
                        {
                            joint.A = prev;
                            joint.LocalOffsetA = new Vector3(0, -0.25f, 0);
                        }
                        joint.B = body;
                        joint.LocalOffsetB = new Vector3(0, 0.25f, 0);
                        e.Add(joint);
                        jointCount++;
                        scene.Entities.Add(e);
                        dynamicCount++;
                        if (l == 9) impulseBodies.Add(body); // "every 10th link" -> the 10th link of each chain
                        prev = body;
                    }
                }
            }
        }

        private void BuildRays(int n, Random rng)
        {
            rayOrigins = new Vector3[n];
            rayDirs = new Vector3[n];
            for (int i = 0; i < n; i++)
            {
                rayOrigins[i] = new Vector3((float)rng.NextDouble() * 40f - 20f, (float)rng.NextDouble() * 20f, (float)rng.NextDouble() * 40f - 20f);
                Vector3 d;
                do
                {
                    d = new Vector3((float)rng.NextDouble() * 2f - 1f, (float)rng.NextDouble() * 2f - 1f, (float)rng.NextDouble() * 2f - 1f);
                } while (d.LengthSquared() < 1e-4f || d.LengthSquared() > 1f);
                d.Normalize();
                rayDirs[i] = d;
            }
        }

        private float pitInner, pitWallH;

        /// <summary>
        /// Bepu grows its buffers on demand, so no limit has to be raised for large N; this only pre-allocates (BepuPhysics.Simulation.EnsureCapacity)
        /// so the scene build does not resize repeatedly. Same formula for every scenario and N (--presize=0 leaves the plugin defaults).
        /// </summary>
        private void PresizeSimulation()
        {
            if (opts.Presize == 0) return;
            int bodies, statics;
            switch (opts.Layout)
            {
                case "pit-2000": bodies = 2000; statics = 5; break;
                case "scale": bodies = opts.Bodies; statics = 5; break;
                case "chains": bodies = 1001; statics = 1; break;
                case "mixed": bodies = 3001; statics = 6; break;
                case "debris": bodies = (int)Math.Ceiling(opts.Rate * 8 + 2) * 64 + 64; statics = 30; break;
                default: bodies = 0; statics = 2001; break; // raycast
            }
            bodies += 64; statics += 64;
            var sizes = new BepuPhysics.SimulationAllocationSizes(bodies, statics, Math.Max(64, bodies / 8), Math.Max(64, bodies / 2), bodies * 4, 0, 8);
            sim.Simulation.EnsureCapacity(sizes);
        }

        private void BuildScenario()
        {
            var rng = new Random(Seed);
            PresizeSimulation();
            switch (opts.Layout)
            {
                case "pit-2000":
                    BuildPit(22f, 12, 0, rng);
                    break;
                case "scale":
                {
                    // Scenario F: Scenario A's bodies with N changed. Same column rule as A (12 x 12 columns centred, 0.6 spacing, layers upward from y = 1).
                    // Pit interior side = max(22, 22 * sqrt(N / 2000)) rounded up to an even number of metres (N <= 2000 gives exactly A's 22 x 22),
                    // so the floor area per body stays at least A's. Wall height = max(6, half the initial column height): bodies spill sideways from the 6.6 m columns.
                    // Whether they stayed inside is measured at the end (summary line bodies_outside_pit), not assumed.
                    int n = opts.Bodies;
                    float inner = Math.Max(22f, (float)(Math.Ceiling(22.0 * Math.Sqrt(n / 2000.0) / 2.0) * 2.0));
                    int layers = (n + 143) / 144;
                    float colTop = 1f + layers * 0.6f;
                    float wallH = Math.Max(6f, (float)Math.Ceiling(colTop * 0.5f));
                    pitInner = inner; pitWallH = wallH;
                    BuildPit(inner, 12, 0, rng, n, wallH);
                    break;
                }
                case "raycast-1000":
                case "raycast-10000":
                    BuildRaycastStatics(rng);
                    BuildRays(opts.Layout == "raycast-1000" ? 1000 : 10000, rng);
                    break;
                case "chains":
                    BuildChains();
                    break;
                case "debris":
                    BuildPit(22f, 12, 0, rng, 0); // Scenario A pit (floor + walls), no pit bodies
                    BuildDebrisWorld();
                    break;
                case "mixed":
                    // SPEC layout rule (same as Godot): pit interior 32 x 32 (floor 34 x 34), A bodies on the 0.6 grid within +-9 m (31x31 columns),
                    // columns with |x|<6 and |z|<6 left free (chain area); B's static shapes are not added.
                    BuildPit(32f, 31, 10, rng);
                    BuildChains();
                    BuildRays(1000, rng);
                    break;
            }
        }

        // ------------------------------------------------------------------ Scenario G: debris

        private const int BlockCount = 25, PiecesPerEvent = 64;
        private const float PieceEdge = 0.5f, PieceMass = 0.125f, DebrisLifetimeS = 8.0f;
        private sealed class DebrisEvent { public double DespawnAt; public int Block; public Entity[] Entities; public BodyComponent[] Bodies; }
        private readonly Entity[] blockEntity = new Entity[BlockCount];
        private readonly bool[] blockIntact = new bool[BlockCount];
        private readonly int[] blockLiveEvents = new int[BlockCount];
        private readonly int[] blockOrder = new int[BlockCount];
        private int blockCursor;
        private Random drng;
        private readonly Queue<DebrisEvent> liveEvents = new Queue<DebrisEvent>();
        private readonly Stack<(Entity, BodyComponent)> piecePool = new Stack<(Entity, BodyComponent)>(); // debris-pooled: parked pieces, asleep, still in the simulation
        private long simSteps, eventsStarted, eventsTotal, eventsMeasured, despawnsMeasured, blockRemovals, blockRebuilds, poolCreated, poolGrown, pieceSeq;
        private int debrisLive, curEventsThisFrame, curDespawnsThisFrame;
        private double curEventMs, curDespawnMs;
        private bool curEventFrame, curDespawnFrame;
        private int peakDebrisLive;

        private static Vector3 BlockPos(int i) => new Vector3(((i % 5) - 2) * 3f, 1f, ((i / 5) - 2) * 3f); // 5 x 5 grid, spacing 3, centred, box centre 1 m above the floor top

        private Entity MakeBlock(int i)
        {
            var size = new Vector3(2f, 2f, 2f);
            var e = NewEntity("block" + i, BlockPos(i), Quaternion.Identity, unitBoxModel, size);
            e.Add(new StaticComponent { Collider = Wrap(new BoxCollider { Size = size }), FrictionCoefficient = 0.5f });
            scene.Entities.Add(e);
            return e;
        }

        private void Reshuffle()
        {
            for (int i = 0; i < BlockCount; i++) blockOrder[i] = i;
            for (int i = BlockCount - 1; i > 0; i--) { int j = drng.Next(i + 1); (blockOrder[i], blockOrder[j]) = (blockOrder[j], blockOrder[i]); }
            blockCursor = 0;
        }

        private void BuildDebrisWorld()
        {
            drng = new Random(Seed);
            Bound(new Vector3(-7f, 0f, -7f), new Vector3(7f, 2f, 7f)); // 5 x 5 block grid, blocks 2 m (camera bounds); the pit is bounded by BuildPit
            for (int i = 0; i < BlockCount; i++) { blockEntity[i] = MakeBlock(i); blockIntact[i] = true; staticCount++; }
            Reshuffle();
            if (opts.Pooled)
            {
                int n = ((int)Math.Ceiling(opts.Rate * DebrisLifetimeS) + 2) * PiecesPerEvent;
                for (int i = 0; i < n; i++) piecePool.Push(NewPiece(true));
                poolCreated = n;
            }
        }

        private (Entity, BodyComponent) NewPiece(bool parked)
        {
            long id = pieceSeq++;
            var pos = parked ? new Vector3((id % 64) * 1.0f, -1000f, (id / 64) * 1.0f) : Vector3.Zero;
            var e = NewEntity("deb" + id, pos, Quaternion.Identity, unitBoxModel, new Vector3(PieceEdge)); // shared unit box model, scaled to 0.5
            var bc = new BodyComponent { Collider = Wrap(new BoxCollider { Size = new Vector3(PieceEdge), Mass = PieceMass }), FrictionCoefficient = 0.5f };
            if (opts.SleepingOff) bc.SleepThreshold = -1f;
            e.Add(bc);
            scene.Entities.Add(e);
            if (parked) { bc.Awake = false; }
            return (e, bc);
        }

        private int LiveBepuBodies()
        {
            var bodies = sim.Simulation.Bodies;
            int total = 0;
            for (int i = 0; i < bodies.Sets.Length; i++) if (bodies.Sets[i].Allocated) total += bodies.Sets[i].Count;
            return total;
        }

        private void DebrisTick(int stepsThisUpdate)
        {
            curEventMs = 0; curDespawnMs = 0; curEventFrame = false; curDespawnFrame = false; curEventsThisFrame = 0; curDespawnsThisFrame = 0;
            simSteps += stepsThisUpdate;
            double simT = simSteps / opts.PhysicsHz; // simulated time: only steps advance it
            while (liveEvents.Count > 0 && liveEvents.Peek().DespawnAt <= simT)
            {
                long t0 = Stopwatch.GetTimestamp();
                Despawn(liveEvents.Dequeue());
                curDespawnMs += Stopwatch.GetElapsedTime(t0).TotalMilliseconds;
                curDespawnFrame = true; curDespawnsThisFrame++;
            }
            while (eventsStarted / opts.Rate <= simT)
            {
                double evT = eventsStarted / opts.Rate;
                long t0 = Stopwatch.GetTimestamp();
                SpawnEvent(evT);
                curEventMs += Stopwatch.GetElapsedTime(t0).TotalMilliseconds;
                curEventFrame = true; curEventsThisFrame++;
                eventsStarted++; eventsTotal++;
            }
            if (debrisLive > peakDebrisLive) peakDebrisLive = debrisLive;
        }

        // One fracture event: remove the intact block (if the chosen slot is still intact), create 64 pieces of a 4 x 4 x 4 grid at the block's place, each with a radial
        // velocity change of 1.5 m/s x (1 +- 20 %) (= impulse 1.5 x mass x factor) and a seeded random spin (uniform in [-3, 3] rad/s per axis).
        private void SpawnEvent(double evT)
        {
            if (blockCursor >= BlockCount) Reshuffle();
            int slot = blockOrder[blockCursor++];
            if (blockIntact[slot]) { scene.Entities.Remove(blockEntity[slot]); blockEntity[slot] = null; blockIntact[slot] = false; blockRemovals++; }
            blockLiveEvents[slot]++;
            var ev = new DebrisEvent { DespawnAt = evT + DebrisLifetimeS, Block = slot, Entities = new Entity[PiecesPerEvent], Bodies = new BodyComponent[PiecesPerEvent] };
            var center = BlockPos(slot);
            int k = 0;
            for (int ix = 0; ix < 4; ix++)
                for (int iy = 0; iy < 4; iy++)
                    for (int iz = 0; iz < 4; iz++)
                    {
                        var off = new Vector3((ix - 1.5f) * PieceEdge, (iy - 1.5f) * PieceEdge, (iz - 1.5f) * PieceEdge);
                        var pos = center + off;
                        float factor = 1f + ((float)drng.NextDouble() * 2f - 1f) * 0.2f;
                        var spin = new Vector3((float)drng.NextDouble() * 2f - 1f, (float)drng.NextDouble() * 2f - 1f, (float)drng.NextDouble() * 2f - 1f) * 3f;
                        var dir = off; dir.Normalize();
                        (Entity e, BodyComponent bc) piece;
                        if (opts.Pooled)
                        {
                            if (piecePool.Count > 0) piece = piecePool.Pop(); else { piece = NewPiece(true); poolGrown++; }
                            piece.bc.Teleport(pos, Quaternion.Identity);
                            piece.bc.Awake = true;
                        }
                        else
                        {
                            piece = NewPiece(false);
                            piece.bc.Teleport(pos, Quaternion.Identity);
                        }
                        piece.bc.LinearVelocity = dir * (1.5f * factor);
                        piece.bc.AngularVelocity = spin;
                        ev.Entities[k] = piece.e; ev.Bodies[k] = piece.bc; k++;
                    }
            debrisLive += PiecesPerEvent;
            liveEvents.Enqueue(ev);
        }

        // Removal of all pieces of an event in one frame (pooled: parked asleep instead of freed). The block is rebuilt when no debris of its slot is alive any more.
        private void Despawn(DebrisEvent ev)
        {
            for (int i = 0; i < PiecesPerEvent; i++)
            {
                if (opts.Pooled)
                {
                    var bc = ev.Bodies[i];
                    bc.LinearVelocity = Vector3.Zero; bc.AngularVelocity = Vector3.Zero;
                    bc.Teleport(new Vector3((i % 8) * 1.0f, -1000f - (i / 8), 0f), Quaternion.Identity);
                    bc.Awake = false;
                    piecePool.Push((ev.Entities[i], bc));
                }
                else scene.Entities.Remove(ev.Entities[i]);
            }
            debrisLive -= PiecesPerEvent;
            if (--blockLiveEvents[ev.Block] == 0 && !blockIntact[ev.Block]) { blockEntity[ev.Block] = MakeBlock(ev.Block); blockIntact[ev.Block] = true; blockRebuilds++; }
        }

        private void AppendDebrisSummary(StringBuilder sum, CultureInfo inv)
        {
            sum.AppendLine("debris rate_R=" + opts.Rate.ToString("0.###", inv) + " pooled=" + opts.Pooled.ToString() + " events_completed_in_measured_window=" + eventsMeasured.ToString(inv) + " events_total_incl_warmup=" + eventsTotal.ToString(inv) + " removals_in_measured_window=" + despawnsMeasured.ToString(inv));
            var ev = new List<double>(); var dp = new List<double>(); var wEv = new List<double>(); var wDp = new List<double>(); var wNoEv = new List<double>(); var wAfterEv = new List<double>();
            var live = new List<double>(); var aw = new List<double>();
            for (int i = 0; i < rows.Count; i++)
            {
                var r = rows[i];
                if (r.EventFrame) { ev.Add(r.EventMs); wEv.Add(r.WallMs); if (i + 1 < rows.Count) wAfterEv.Add(rows[i + 1].WallMs); } else wNoEv.Add(r.WallMs);
                if (r.DespawnFrame) { dp.Add(r.DespawnMs); wDp.Add(r.WallMs); }
                live.Add(r.DebrisLive); aw.Add(r.Awake);
            }
            sum.AppendLine(Stats("event_ms (event frames only, n=" + ev.Count.ToString(inv) + ")", ev.ToArray()));
            sum.AppendLine(Stats("despawn_ms (removal frames only, n=" + dp.Count.ToString(inv) + ")", dp.ToArray()));
            sum.AppendLine(Stats("wall_ms on event frames (the row of the frame that ran the event)", wEv.ToArray()));
            sum.AppendLine(Stats("wall_ms on the frame after an event frame (the hitch of an Update shows in the next row's wall_ms)", wAfterEv.ToArray()));
            sum.AppendLine(Stats("wall_ms on removal frames", wDp.ToArray()));
            sum.AppendLine(Stats("wall_ms on non-event frames", wNoEv.ToArray()));
            sum.AppendLine("debris_live max=" + (live.Count > 0 ? live.Max() : 0).ToString("F0", inv) + " mean=" + (live.Count > 0 ? live.Average() : 0).ToString("F1", inv) + " peak_incl_warmup=" + peakDebrisLive.ToString(inv) + " last=" + debrisLive.ToString(inv));
            sum.AppendLine("awake_bodies min=" + (aw.Count > 0 ? aw.Min() : 0).ToString("F0", inv) + " max=" + (aw.Count > 0 ? aw.Max() : 0).ToString("F0", inv));
            sum.AppendLine("blocks_removed=" + blockRemovals.ToString(inv) + " blocks_rebuilt=" + blockRebuilds.ToString(inv) + " blocks_intact_at_end=" + blockIntact.Count(b => b).ToString(inv) + " (whole run incl. warm-up; an event on a slot whose block is not intact removes no block)");
            sum.AppendLine("bepu_live_bodies_at_end=" + LiveBepuBodies().ToString(inv) + " bepu_statics_at_end=" + sim.Simulation.Statics.Count.ToString(inv) + " scene_entities_at_end=" + scene.Entities.Count.ToString(inv) + " pool_created=" + poolCreated.ToString(inv) + " pool_grown=" + poolGrown.ToString(inv) + " pool_free_at_end=" + piecePool.Count.ToString(inv));
            sum.AppendLine("render pieces: shared unit box Model scaled 0.5 per entity, one shared Material (no per-piece mesh or material creation)");
        }

        // ------------------------------------------------------------------ frame loop

        protected override void Update(GameTime gameTime)
        {
            long now = Stopwatch.GetTimestamp();
            double wallMs = Stopwatch.GetElapsedTime(lastUpdateTs, now).TotalMilliseconds;
            lastUpdateTs = now;

            timer.AccumulatedMs = 0;
            timer.Steps = 0; timer.QueryMs = 0;
            long bu0 = Stopwatch.GetTimestamp();
            base.Update(gameTime); // runs PhysicsGameSystem (Bepu ticks) and scene processors
            if (measuring) { baseUpdateMs += Stopwatch.GetElapsedTime(bu0).TotalMilliseconds; gameElapsedS += gameTime.Elapsed.TotalSeconds; }
            double physMs = timer.AccumulatedMs;
            if (!opts.Unpaced) { totalSteps += timer.Steps; totalGameS += gameTime.Elapsed.TotalSeconds; if (timer.Steps > peakStepsAnyFrame) peakStepsAnyFrame = timer.Steps; }
            if (measuring) CollectSteps();

            if (finished) return;
            if (opts.Unpaced) { UnpacedLoop(); return; }
            if (opts.Diag && measuring) curUpdStart = Stopwatch.GetElapsedTime(measureStartTs, now).TotalMilliseconds;

            if (opts.Layout == "debris") DebrisTick(timer.Steps);
            ApplyImpulsesOnce();
            double queryMs = timer.QueryMs; // the ray batch runs once per physics step (OnStep hook), summed over the steps of this frame

            if (opts.Diag && measuring) curUpdDur = Stopwatch.GetElapsedTime(now).TotalMilliseconds;
            Record(now, wallMs, physMs, queryMs, timer.Steps);
        }

        private void ApplyImpulsesOnce()
        {
            if (!impulsesApplied && impulseBodies.Count > 0 && impulseBodies[0].Awake)
            {
                foreach (var b in impulseBodies) b.ApplyLinearImpulse(new Vector3(2f, 0f, 0f));
                impulsesApplied = true;
            }
        }

        // Called by PhysicsTimerComponent after every Bepu step (outside the physics_ms interval): SPEC says the ray batch runs per physics step.
        private void OnStep()
        {
            if (rayOrigins == null) return;
            double ms = RayBatch();
            timer.QueryMs += ms;
            if (measuring) raysCast += rayOrigins.Length;
        }
        private long raysCast;

        private double RayBatch()
        {
            if (rayOrigins == null) return 0;
            long q0 = Stopwatch.GetTimestamp();
            long hits = 0;
            var o = rayOrigins; var d = rayDirs;
            for (int i = 0; i < o.Length; i++)
            {
                if (sim.RayCast(in o[i], in d[i], 30f, out _)) hits++;
            }
            double queryMs = Stopwatch.GetElapsedTime(q0).TotalMilliseconds;
            if (measuring) rayHits += hits;
            return queryMs;
        }

        // Scenario E physics variant / scale-unlock-physics: no wall-clock pacing. The plugin's own stepping is off (sim.Enabled = false), so
        // every call of BepuSimulation.Update(FixedTimeStep) is exactly one 1/PhysicsHz (5 ms) step (checked: unpaced_calls_not_one_step in the summary).
        // One CSV row = one step. wall_ms of a row = time from the end of the previous step's row work (or the start of this batch) to the end of this step
        // incl. the ray batch (mixed); the game loop's own overhead between batches (Draw, Present, window messages, ~every 20 ms) is not in wall_ms
        // but is in the elapsed time used for steps_per_s.
        private const double BatchMs = 20.0;
        private long unpacedCalls, unpacedBadCalls;
        private double baseUpdateMs, gapMs, gameElapsedS; private long lastLoopEndTs;
        private Action<TimeSpan> simUpdate; // BepuSimulation.Update(TimeSpan) is internal in 4.4.1: bound once by reflection

        private void UnpacedLoop()
        {
            long batch0 = Stopwatch.GetTimestamp();
            if (measuring && lastLoopEndTs != 0) gapMs += Stopwatch.GetElapsedTime(lastLoopEndTs, batch0).TotalMilliseconds;
            long prev = batch0;
            var dt = sim.FixedTimeStep;
            simUpdate ??= (Action<TimeSpan>)typeof(BepuSimulation).GetMethod("Update", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic, null, new[] { typeof(TimeSpan) }, null).CreateDelegate(typeof(Action<TimeSpan>), sim);
            while (true)
            {
                timer.AccumulatedMs = 0; timer.Steps = 0; timer.QueryMs = 0;
                sim.Enabled = true; simUpdate(dt); sim.Enabled = false; // Update is gated by Enabled (a call with Enabled=false ran 0 steps, proved by the DBG run)
                double physMs = timer.AccumulatedMs;
                int steps = timer.Steps;
                if (measuring) CollectSteps();
                unpacedCalls++;
                if (steps != 1) unpacedBadCalls++;
                ApplyImpulsesOnce();
                double queryMs = timer.QueryMs;
                long now = Stopwatch.GetTimestamp();
                double wallMs = Stopwatch.GetElapsedTime(prev, now).TotalMilliseconds;
                prev = now;
                if (Record(now, wallMs, physMs, queryMs, steps)) return;
                if (Stopwatch.GetElapsedTime(batch0, now).TotalMilliseconds >= BatchMs) { lastLoopEndTs = now; return; }
            }
        }

        private long measuredSteps, totalSteps;
        private double totalGameS; private int peakStepsAnyFrame;
        private readonly List<double> stepMs = new List<double>(1 << 18); // every measured step individually (physics_step_ms)
        private int minSteps = int.MaxValue, maxSteps;
        private double curStepMean;

        private void CollectSteps()
        {
            int n = Math.Min(timer.Steps, timer.StepMs.Length);
            double sum = 0;
            for (int i = 0; i < n; i++) { stepMs.Add(timer.StepMs[i]); sum += timer.StepMs[i]; }
            curStepMean = n > 0 ? sum / n : 0;
        }
        private long lastRecordTs;

        /// <summary>Warm-up / measured window handling and row recording; returns true when the run finished (results written, Exit requested).</summary>
        private bool Record(long now, double wallMs, double physMs, double queryMs, int steps)
        {
            double elapsedS = Stopwatch.GetElapsedTime(startTs, now).TotalSeconds;
            if (!measuring)
            {
                if (elapsedS >= opts.WarmupSeconds)
                {
                    measuring = true;
                    measureStartTs = now;
                    gcPauseAtStartMs = GC.GetTotalPauseDuration().TotalMilliseconds;
                    frameIndex = 0;
                }
                return false;
            }

            rows.Add(new Row
            {
                Frame = frameIndex++,
                WallMs = wallMs,
                PhysicsMs = physMs,
                QueryMs = queryMs,
                Gc0 = GC.CollectionCount(0),
                Gc1 = GC.CollectionCount(1),
                Gc2 = GC.CollectionCount(2),
                GcPauseMs = GC.GetTotalPauseDuration().TotalMilliseconds,
                Awake = sim.Simulation.Bodies.ActiveSet.Count - ((ceilingBody != null && ceilingBody.Awake) ? 1 : 0),
                Tick = steps > 0, Steps = steps, StepMeanMs = curStepMean,
                TimeS = Stopwatch.GetElapsedTime(measureStartTs, now).TotalSeconds,
                EventMs = curEventMs, DespawnMs = curDespawnMs, EventFrame = curEventFrame, DespawnFrame = curDespawnFrame, DebrisLive = debrisLive,
            });
            eventsMeasured += curEventsThisFrame;
            despawnsMeasured += curDespawnsThisFrame;
            measuredSteps += steps;
            if (steps < minSteps) minSteps = steps;
            if (steps > maxSteps) maxSteps = steps;
            lastRecordTs = now;

            if (Stopwatch.GetElapsedTime(measureStartTs, now).TotalSeconds >= opts.MeasureSeconds)
            {
                finished = true;
                try { WriteResults(); }
                catch (Exception e) { Console.Error.WriteLine("write results failed: " + e); ExitCode = 1; }
                Exit();
                return true;
            }
            return false;
        }

        private readonly List<double> drawMs = new List<double>(1 << 17);

        // --diag=1: per-frame timeline (Update start/duration, Draw start/duration, EndDraw/Present duration). Default off.
        private readonly List<float[]> diagRows = new List<float[]>(1 << 14);
        private double curUpdStart, curUpdDur, curDrawStart, curDrawDur;

        protected override void Draw(GameTime gameTime)
        {
            long d0 = Stopwatch.GetTimestamp();
            base.Draw(gameTime);
            double dd = Stopwatch.GetElapsedTime(d0).TotalMilliseconds;
            if (measuring) drawMs.Add(dd);
            if (opts.Diag && measuring)
            {
                curDrawStart = Stopwatch.GetElapsedTime(measureStartTs, d0).TotalMilliseconds;
                curDrawDur = dd;
            }
        }

        protected override void EndDraw(bool present)
        {
            long p0 = Stopwatch.GetTimestamp();
            base.EndDraw(present);
            if (opts.Diag && measuring)
            {
                double pm = Stopwatch.GetElapsedTime(p0).TotalMilliseconds;
                diagRows.Add(new[] { (float)curUpdStart, (float)curUpdDur, (float)curDrawStart, (float)curDrawDur, (float)pm });
            }
        }

        // ------------------------------------------------------------------ output

        private static double Percentile(double[] sorted, double p)
        {
            if (sorted.Length == 0) return double.NaN;
            double rank = p * (sorted.Length - 1);
            int lo = (int)Math.Floor(rank), hi = (int)Math.Ceiling(rank);
            return sorted[lo] + (sorted[hi] - sorted[lo]) * (rank - lo);
        }

        private static string Stats(string name, double[] v)
        {
            var s = (double[])v.Clone();
            Array.Sort(s);
            return string.Format(CultureInfo.InvariantCulture, "{0}: median={1:F3} p95={2:F3} p99={3:F3} max={4:F3}",
                name, Percentile(s, 0.5), Percentile(s, 0.95), Percentile(s, 0.99), s.Length > 0 ? s[s.Length - 1] : double.NaN);
        }

        private static double Median(List<double> v)
        {
            if (v.Count == 0) return double.NaN;
            var a = v.ToArray(); Array.Sort(a); return Percentile(a, 0.5);
        }

        // Scenario E/F summary lines (additional to the SPEC summary; CSV columns unchanged).
        private void AppendScaleSummary(StringBuilder sum, CultureInfo inv)
        {
            double measuredS = rows.Count > 0 ? Stopwatch.GetElapsedTime(measureStartTs, lastRecordTs).TotalSeconds : 0;
            sum.AppendLine("measured_seconds=" + measuredS.ToString("F3", inv) + " (first measured row to last row, wall clock)");
            if (!opts.Unpaced) sum.AppendLine("game_time_elapsed_sum_s=" + gameElapsedS.ToString("F3", inv) + " (sum of GameTime.Elapsed over measured Updates; steps x 1/" + opts.PhysicsHz.ToString("F0", inv) + " = " + (measuredSteps / opts.PhysicsHz).ToString("F3", inv) + " s; a difference of more than one step interval per run is a dropped or extra step)");
            if (!opts.Unpaced) sum.AppendLine("fps=" + (rows.Count / measuredS).ToString("F2", inv) + " (frames / measured_seconds)");
            if (!opts.Unpaced)
            {
                sum.AppendLine("steps_dropped_measured_window=" + Math.Round(gameElapsedS * opts.PhysicsHz - measuredSteps).ToString("F0", inv) + " (GameTime.Elapsed sum x physics_hz minus steps run in the measured window; signed: negative = backlog from the warm-up played back, +-1 = accumulator remainder)");
                sum.AppendLine("steps_dropped_whole_run=" + Math.Round(totalGameS * opts.PhysicsHz - totalSteps).ToString("F0", inv) + " (same over every Update since start; a value above a few steps means the plugin discarded time) peak_steps_in_one_Update=" + peakStepsAnyFrame.ToString(inv) + " max_steps_per_frame_setting=" + sim.MaxStepPerFrame.ToString(inv));
            }
            sum.AppendLine("steps_per_s=" + (measuredSteps / measuredS).ToString("F2", inv) + " (Bepu steps counted by the AfterSimulationUpdate hook: " + measuredSteps.ToString(inv) + " / measured_seconds)" );
            if (opts.Unpaced)
            {
                double physSum = 0, wallSum = 0;
                foreach (var r in rows) { physSum += r.PhysicsMs; wallSum += r.WallMs; }
                sum.AppendLine("steps_per_s_physics_only=" + (measuredSteps / (physSum / 1000.0)).ToString("F2", inv) + " (steps / sum of physics_ms)");
                sum.AppendLine("steps_per_s_in_step_loop=" + (measuredSteps / (wallSum / 1000.0)).ToString("F2", inv) + " (steps / sum of wall_ms: step + ray batch + recording, without the game loop's Draw/Present between 20 ms batches)");
                sum.AppendLine("unpaced_calls=" + unpacedCalls.ToString(inv) + " unpaced_calls_not_one_step=" + unpacedBadCalls.ToString(inv) + " (calls of BepuSimulation.Update(1/PhysicsHz s) that did not run exactly one step, whole run incl. warm-up)");
                sum.AppendLine("base_update_ms_total=" + baseUpdateMs.ToString("F1", inv) + " draw_ms_total=" + drawMs.Sum().ToString("F1", inv) + " gap_between_batches_ms_total=" + gapMs.ToString("F1", inv) + " of measured " + (measuredS * 1000).ToString("F0", inv) + " ms (game-loop work outside the step loop)");
                sum.AppendLine("speed_vs_real_time=" + (measuredSteps / measuredS / opts.PhysicsHz).ToString("F2", inv) + "x");
            }
            else
            {
                sum.AppendLine((IsFixedTimeStep ? "paced_frames=" : "frames=") + rows.Count.ToString(inv) + " (" + (IsFixedTimeStep ? "fixed 60 Hz loop" : "unlocked loop") + ") expected_at_60hz=" + (opts.MeasureSeconds * 60.0).ToString("F0", inv) + " ratio_to_60hz=" + (rows.Count / (opts.MeasureSeconds * 60.0)).ToString("F4", inv));
            }
            {
                var tw = new List<double>();
                for (int i = 0; i < rows.Count; i++) if (rows[i].Tick) tw.Add(rows[i].WallMs);
                sum.AppendLine(Stats("wall_ms (tick frames only)", tw.ToArray()));
            }
            long ws = Process.GetCurrentProcess().WorkingSet64;
            sum.AppendLine("working_set_mb=" + (ws / 1048576.0).ToString("F1", inv) + " (Process.WorkingSet64 at the end of the measured window)");
            sum.AppendLine("gc_heap_mb=" + (GC.GetTotalMemory(false) / 1048576.0).ToString("F1", inv) + " bepu_pool_mb=" + (sim.Simulation.BufferPool.GetTotalAllocatedByteCount() / 1048576.0).ToString("F1", inv));
            var simu = sim.Simulation;
            var sb2 = new StringBuilder();
            for (int t = 0; t < simu.Shapes.RegisteredTypeSpan; t++)
            {
                var b = simu.Shapes[t];
                if (b != null) sb2.Append(' ').Append(t.ToString(inv)).Append(b.Compound ? "(compound)" : "").Append(":cap=").Append(b.Capacity.ToString(inv));
            }
            sum.AppendLine("bepu_body_handle_capacity=" + simu.Bodies.HandleToLocation.Length.ToString(inv) + " presize=" + opts.Presize.ToString(inv) + " shape_batches(typeId:capacity)=" + sb2.ToString().Trim());
            if (opts.Layout == "scale" || opts.Layout == "pit-2000")
            {
                int outside = 0, below = 0;
                foreach (var b in dynamicBodies)
                {
                    if (b.Kinematic) continue;
                    var p = b.Position;
                    if (Math.Abs(p.X) > pitHalf || Math.Abs(p.Z) > pitHalf) outside++;
                    else if (p.Y < -0.5f) below++;
                }
                sum.AppendLine("bodies_outside_pit=" + outside.ToString(inv) + " bodies_below_floor=" + below.ToString(inv) + " (end of run; pit interior half size " + pitHalf.ToString("F1", inv) + " m" + (opts.Layout == "scale" ? ", wall height " + pitWallH.ToString("F0", inv) + " m" : "") + ")");
            }
            // cost while awake (first 5 measured s) vs the last 5 s; windows overlap when --measure is shorter than 10 s
            double win = 5.0;
            foreach (var (name, lo, hi) in new[] { ("first_5s", 0.0, win), ("last_5s", opts.MeasureSeconds - win, double.MaxValue) })
            {
                var w = new List<double>(); var p = new List<double>(); var aw = new List<double>();
                foreach (var r in rows)
                {
                    if (r.TimeS < lo || r.TimeS >= hi) continue;
                    w.Add(r.WallMs); p.Add(r.PhysicsMs); aw.Add(r.Awake);
                }
                sum.AppendLine("window " + name + " rows=" + w.Count.ToString(inv) + " awake_median=" + Median(aw).ToString("F0", inv));
                sum.AppendLine(Stats("window " + name + " wall_ms", w.ToArray()));
                sum.AppendLine(Stats("window " + name + " physics_ms", p.ToArray()));
            }
        }

        private void WriteResults()
        {
            Directory.CreateDirectory(opts.OutDir);
            string baseName = string.Format(CultureInfo.InvariantCulture, "stride-bepu-{0}{1}-{2}-{3}", opts.Profile == "stress" ? "stress-" : "", opts.FileScenario, opts.Mode, opts.Run);
            string csv = Path.Combine(opts.OutDir, baseName + ".csv");
            var inv = CultureInfo.InvariantCulture;
            var sb = new StringBuilder();
            sb.Append("frame,wall_ms,physics_ms,query_ms,gc0,gc1,gc2,gc_pause_ms,awake_bodies" + (opts.Layout == "debris" ? ",event_ms,despawn_ms,debris_live" : "") + ",physics_step_ms,physics_frame_ms,steps_per_frame\n");
            foreach (var r in rows)
            {
                sb.Append(r.Frame.ToString(inv)).Append(',')
                  .Append(r.WallMs.ToString("F4", inv)).Append(',')
                  .Append(r.PhysicsMs.ToString("F4", inv)).Append(',')
                  .Append(r.QueryMs.ToString("F4", inv)).Append(',')
                  .Append(r.Gc0.ToString(inv)).Append(',').Append(r.Gc1.ToString(inv)).Append(',').Append(r.Gc2.ToString(inv)).Append(',')
                  .Append(r.GcPauseMs.ToString("F4", inv)).Append(',')
                  .Append(r.Awake.ToString(inv));
                if (opts.Layout == "debris") sb.Append(',').Append(r.EventMs.ToString("F4", inv)).Append(',').Append(r.DespawnMs.ToString("F4", inv)).Append(',').Append(r.DebrisLive.ToString(inv));
                sb.Append(',').Append(r.StepMeanMs.ToString("F4", inv)).Append(',').Append(r.PhysicsMs.ToString("F4", inv)).Append(',').Append(r.Steps.ToString(inv));
                sb.Append('\n');
            }
            File.WriteAllText(csv, sb.ToString());
            if (opts.Diag)
            {
                var db = new StringBuilder();
                db.Append("seq,update_start_ms,update_dur_ms,draw_start_ms,draw_dur_ms,present_ms\n");
                for (int i = 0; i < diagRows.Count; i++)
                {
                    var r = diagRows[i];
                    db.Append(i.ToString(inv));
                    for (int k = 0; k < r.Length; k++) db.Append(',').Append(r[k].ToString("F3", inv));
                    db.Append('\n');
                }
                File.WriteAllText(Path.Combine(opts.OutDir, baseName + ".diag.csv"), db.ToString());
            }

            var wall = new double[rows.Count];
            var phys = new double[rows.Count];
            for (int i = 0; i < rows.Count; i++) { wall[i] = rows[i].WallMs; phys[i] = rows[i].PhysicsMs; }
            double firstMax = 0;
            for (int i = 0; i < Math.Min(120, rows.Count); i++) firstMax = Math.Max(firstMax, wall[i]);
            double gcPause = rows.Count > 0 ? rows[rows.Count - 1].GcPauseMs - gcPauseAtStartMs : 0;

            var sum = new StringBuilder();
            sum.AppendLine("engine=stride backend=bepu scenario=" + opts.FileScenario + " layout=" + opts.Layout + " mode=" + opts.Mode + " run=" + opts.Run.ToString(inv));
            sum.AppendLine("frames=" + rows.Count.ToString(inv));
            sum.AppendLine(Stats("wall_ms", wall));
            sum.AppendLine(Stats("physics_ms", phys));
            sum.AppendLine(Stats("physics_frame_ms (= physics_ms: sum over the steps of the row; the row is one Update, or one step when unpaced)", phys));
            sum.AppendLine(Stats("physics_step_ms (each measured step individually, n=" + stepMs.Count.ToString(inv) + ")", stepMs.ToArray()));
            sum.AppendLine("steps_per_frame min=" + (rows.Count > 0 ? minSteps : 0).ToString(inv) + " max=" + maxSteps.ToString(inv) + " mean=" + (rows.Count > 0 ? (double)measuredSteps / rows.Count : 0).ToString("F3", inv) + " physics_hz=" + opts.PhysicsHz.ToString("F0", inv));
            {
                var tp = new List<double>();
                for (int i = 0; i < rows.Count; i++) if (rows[i].Tick) tp.Add(rows[i].PhysicsMs);
                sum.AppendLine("tick_frames=" + tp.Count.ToString(inv) + " of " + rows.Count.ToString(inv));
                sum.AppendLine(Stats("physics_ms (tick frames only)", tp.ToArray()));
            }
            if (rayOrigins != null)
            {
                var q = new double[rows.Count];
                for (int i = 0; i < rows.Count; i++) q[i] = rows[i].QueryMs;
                sum.AppendLine(Stats("query_ms", q));
                sum.AppendLine("rays_per_step=" + rayOrigins.Length.ToString(inv) + " rays_cast_measured_window=" + raysCast.ToString(inv) + " (= rays_per_step x steps in the window " + measuredSteps.ToString(inv) + "; the batch runs once per physics step, query_ms of a row = sum over its steps)");
                sum.AppendLine("ray_hits_total=" + rayHits.ToString(inv));
            }
            sum.AppendLine(Stats("draw_call_ms (extra, per Game.Draw, not per Update)", drawMs.ToArray()));
            sum.AppendLine("gc_pause_total_ms=" + gcPause.ToString("F3", inv));
            sum.AppendLine("first_120_frames_max_wall_ms=" + firstMax.ToString("F3", inv));
            if (rows.Count > 0) sum.AppendLine("awake_bodies_last=" + rows[rows.Count - 1].Awake.ToString(inv) + " (Bepu ActiveSet.Count minus the kinematic ceiling body in chains/mixed)");
            sum.AppendLine("bodies_dynamic=" + dynamicCount.ToString(inv) + " statics=" + staticCount.ToString(inv) + " joints=" + jointCount.ToString(inv));
            AppendScaleSummary(sum, inv);
            if (opts.Layout == "debris") AppendDebrisSummary(sum, inv);
            string summary = sum.ToString();
            File.WriteAllText(csv + ".summary.txt", summary);
            Console.Write(summary);

            var env = new StringBuilder();
            env.AppendLine("engine=Stride " + typeof(Game).Assembly.GetName().Version);
            env.AppendLine("backend=Stride.BepuPhysics " + typeof(BepuSimulation).Assembly.GetName().Version);
            env.AppendLine("runtime=" + RuntimeInformation.FrameworkDescription);
            env.AppendLine("os=" + RuntimeInformation.OSDescription);
            env.AppendLine("cpu=" + (Environment.GetEnvironmentVariable("PROCESSOR_IDENTIFIER") ?? "unknown") + " logical_cores=" + Environment.ProcessorCount.ToString(inv));
            env.AppendLine("gpu=" + adapterName);
            env.AppendLine("graphics_platform=" + (GraphicsDevice != null ? GraphicsDevice.Platform.ToString() : "n/a"));
            env.AppendLine("build_type=" + BuildType());
            env.AppendLine("server_gc=" + System.Runtime.GCSettings.IsServerGC.ToString() + " gc_latency=" + System.Runtime.GCSettings.LatencyMode);
            env.AppendLine("bepu_thread_count_setting=" + sim.ThreadCount.ToString(inv) + " (-1 = Dispatcher.MaxDegreeOfParallelism=" + Stride.Core.Threading.Dispatcher.MaxDegreeOfParallelism.ToString(inv) + ")");
            env.AppendLine("bepu_solver_velocity_iterations=" + sim.SolverIteration.ToString(inv) + " substeps=" + sim.SolverSubStep.ToString(inv));
            env.AppendLine("bepu_softstart=" + sim.SoftStartDuration + " substep_factor=" + sim.SoftStartSubstepFactor.ToString(inv));
            env.AppendLine("bepu_fixed_timestep_s=" + sim.FixedTimeStepSeconds.ToString("F6", inv) + " max_steps_per_frame=" + sim.MaxStepPerFrame.ToString(inv));
            env.AppendLine("damping_linear=" + sim.PoseLinearDamping.ToString(inv) + " damping_angular=" + sim.PoseAngularDamping.ToString(inv) + " (plugin defaults were " + defaultLinearDamping.ToString(inv) + " / " + defaultAngularDamping.ToString(inv) + ")");
            env.AppendLine("bepu_parallel_update=" + sim.ParallelUpdate.ToString() + " deterministic=" + sim.Deterministic.ToString());
            env.AppendLine("profile: " + opts.Profile);
            env.AppendLine("sleeping: " + (opts.SleepingOff ? "off" : "default"));
            env.AppendLine("shadows: " + (opts.Render ? (opts.Shadows ? "on" : "off") : "n/a (physics mode)"));
            env.AppendLine("shadow_map_size: " + (opts.Render ? (opts.Shadows ? shadowMapSizeText : "n/a (shadows off)") : "n/a (physics mode)"));
            env.AppendLine("msaa: " + (opts.Render ? msaaText : "n/a (physics mode)"));
            env.AppendLine("vsync: " + (opts.Render ? (opts.Vsync ? "on" : "off") : "off (physics mode)"));
            env.AppendLine("max_fps: " + "0 (IsFixedTimeStep=false, no TargetElapsedTime, InactiveSleepTime=0; " + (opts.Render ? "render" : "physics") + " mode)");
            env.AppendLine("physics_hz: " + (1.0 / sim.FixedTimeStep.TotalSeconds).ToString("F3", inv) + " (BepuSimulation.FixedTimeStep = " + sim.FixedTimeStep.Ticks.ToString(inv) + " ticks; max_steps_per_frame " + sim.MaxStepPerFrame.ToString(inv) + ")");
            env.AppendLine("solver_iterations: " + sim.SolverIteration.ToString(inv));
            env.AppendLine("solver_substeps: " + sim.SolverSubStep.ToString(inv));
            env.AppendLine("light_intensity: " + opts.SunIntensity.ToString("0.###", inv) + " (directional, white, Stride units, fitted to Godot 1.0 by mean image luminance; see NOTES)");
            env.AppendLine("ambient_intensity: " + opts.AmbientIntensity.ToString("0.###", inv) + " (ambient white, Stride units, fitted together with the sun; Godot 0.5); material_albedo: 1.0 white; tonemap: Stride default (not read)");
            env.AppendLine("background: " + (opts.Render ? (opts.BackgroundCornflower ? "cornflower (0.392, 0.584, 0.929), set explicitly via GraphicsCompositorHelper.CreateDefault clearColor" : "default (CreateDefault with no clearColor; the helper default, not read back)") : "n/a (physics mode)"));
            env.AppendLine("camera_mode: " + (opts.CameraSky ? "sky" : "content") + (opts.Render ? "" : " (physics mode: no camera, values computed from the same rule)"));
            env.AppendLine("camera_position: " + V3(camPos, inv));
            env.AppendLine("camera_look_at: " + V3(opts.CameraSky ? camPos + new Vector3(0, 1, 0) : camLookAt, inv));
            env.AppendLine("camera_fov_v_deg: 60");
            env.AppendLine("camera_near: 0.1");
            env.AppendLine("camera_far: 2000");
            env.AppendLine("camera_content_radius: " + camContentRadius.ToString("F3", inv));
            env.AppendLine("camera_content_aabb: min " + V3(boundMin, inv) + " max " + V3(boundMax, inv));
            env.AppendLine("loop=" + (IsFixedTimeStep ? "fixed 60 Hz frame loop" : "unlocked frame loop") + ", physics " + opts.PhysicsHz.ToString("F0", inv) + " Hz" + " vsync=" + (opts.Render && opts.Vsync ? "on" : "off") + " max_fps=" + (opts.Render ? "0" : "60 (physics mode)") + " resolution=" + GraphicsDeviceManager.PreferredBackBufferWidth.ToString(inv) + "x" + GraphicsDeviceManager.PreferredBackBufferHeight.ToString(inv));
            if (cameraEntity != null)
            {
                var cc = cameraEntity.Get<CameraComponent>();
                var wm = cameraEntity.Transform.WorldMatrix;
                var q = cameraEntity.Transform.Rotation;
                var bb = GraphicsDevice.Presenter?.BackBuffer;
                env.AppendLine("camera_world_pos="+ wm.TranslationVector.ToString() + " forward=" + wm.Forward.ToString() + " rotation_quat=" + q.ToString()
                    + " projection=" + cc.Projection + " vertical_fov_deg=" + cc.VerticalFieldOfView.ToString(inv) + " near=" + cc.NearClipPlane.ToString(inv) + " far=" + cc.FarClipPlane.ToString(inv)
                    + " custom_aspect=" + cc.UseCustomAspectRatio + " aspect_prop=" + cc.AspectRatio.ToString(inv)
                    + " backbuffer=" + (bb != null ? bb.Width.ToString(inv) + "x" + bb.Height.ToString(inv) : "n/a") + " window_client=" + Window.ClientBounds.Width.ToString(inv) + "x" + Window.ClientBounds.Height.ToString(inv));
            }
            env.AppendLine("command_line=" + Environment.CommandLine);
            File.WriteAllText(Path.Combine(opts.OutDir, baseName + ".env.txt"), env.ToString());
        }

        private static string V3(Vector3 v, CultureInfo inv) => "(" + v.X.ToString("F3", inv) + ", " + v.Y.ToString("F3", inv) + ", " + v.Z.ToString("F3", inv) + ")";

        private static string BuildType()
        {
#if DEBUG
            return "Debug";
#else
            return "Release";
#endif
        }
    }
}
