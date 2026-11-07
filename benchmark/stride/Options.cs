using System;
using System.Globalization;

namespace StrideBench
{
    public sealed class Options
    {
        public string Scenario = "pit-2000";
        public string Mode = "physics";
        public int Run = 1;
        public string OutDir = "results";
        public double WarmupSeconds = 5.0;
        public double MeasureSeconds = 20.0;
        public bool Uncapped = false; // kept for old command lines; no effect: every render run is unlocked
        public bool TimerRes1ms = true;  // timeBeginPeriod(1) for the process, default ON (15.6 ms default timer granularity caused 40-60 ms stalls); --timer1ms=0 disables
        public bool Diag = false;     // diagnostic: write <csv>.diag.csv with per-frame Update/Draw/Present timing (default off)
        public int BepuThreads = 0;   // experiment: >0 sets BepuSimulation.ThreadCount
        public int RenderThreads = 0; // experiment: >0 sets Dispatcher.MaxDegreeOfParallelism
        public bool NoModels = false; // experiment: render mode without ModelComponents (isolates draw cost)
        public bool Hidden = false;   // physics mode only: hide the window
        public string Layout = "pit-2000"; // scene layout: pit-2000|raycast-1000|raycast-10000|chains|mixed|scale
        public string FileScenario = "pit-2000"; // scenario token used in file names
        public int Bodies = 0;        // scale: number of dynamic bodies
        public bool Unpaced = false;  // physics mode: step back to back, no wall-clock pacing (scenario E physics variant, scale-unlock-physics)
        public bool Unlock = false;   // render mode: fixed time step off, no target elapsed time (scenario E render variant)
        public int Presize = 0;       // 1 = EnsureCapacity of bodies/statics/shapes/constraints before the scene is built (all scenarios); 0 = plugin defaults (default: A/B at N=16000 showed no effect, Bepu grows on demand)
        public string Profile = "game"; // --profile=game|stress: a bundle of the flags below, each of which can also be set alone (explicit flag wins)
        public bool SleepingOff = false; // --sleeping=default|off: off = BodyComponent.SleepThreshold = -1 on every dynamic body (no body can fall asleep); profile stress = off, except chains and debris
        public bool Shadows = true;      // --shadows=on|off (render mode): the directional light casts shadow maps
        public int Msaa = 4;             // --msaa=1|2|4|8 (render mode): ForwardRenderer.MSAALevel
        public bool Vsync = false;       // --vsync=on|off (render mode): default off in every profile (fps unlocked); an explicit --vsync=on is a diagnostic only
        private bool sleepingGiven;
        public double PhysicsHz = 200.0; // --physics-hz=N: Bepu fixed step rate (SPEC: 200 Hz in every scenario); the display loop stays 60 Hz
        public float AmbientIntensity = 0.3f; // --ambient-intensity=x
        public float SunIntensity = 0.55f;  // --sun-intensity=x: directional light intensity (Stride units; brightness-matched to Godot 1.0 white, see NOTES)
        public bool CameraSky = false; // --camera=content|sky (render mode): sky = same position as content, looking straight up
        public bool ModeGiven;
        public bool WarmupGiven;
        public double Rate = 0;       // debris: destruction events per second of simulated time (R in SPEC scenario G)
        public bool Pooled = false;   // debris-pooled: pieces are pre-created and reused

        public static Options Parse(string[] args)
        {
            var o = new Options();
            foreach (var a in args)
            {
                if (!a.StartsWith("--", StringComparison.Ordinal)) continue;
                var eq = a.IndexOf('=');
                var key = eq < 0 ? a.Substring(2) : a.Substring(2, eq - 2);
                var val = eq < 0 ? "1" : a.Substring(eq + 1);
                switch (key)
                {
                    case "scenario": o.Scenario = val; break;
                    case "mode": o.Mode = val; o.ModeGiven = true; break;
                    case "run": o.Run = int.Parse(val, CultureInfo.InvariantCulture); break;
                    case "out": o.OutDir = val; break;
                    case "warmup": o.WarmupGiven = true; o.WarmupSeconds = double.Parse(val, CultureInfo.InvariantCulture); break;
                    case "measure": o.MeasureSeconds = double.Parse(val, CultureInfo.InvariantCulture); break;
                    case "uncapped": o.Uncapped = val == "1" || val == "true"; break;
                    case "timer1ms": o.TimerRes1ms = val == "1" || val == "true"; break;
                    case "diag": o.Diag = val == "1" || val == "true"; break;
                    case "bepu-threads": o.BepuThreads = int.Parse(val, CultureInfo.InvariantCulture); break;
                    case "render-threads": o.RenderThreads = int.Parse(val, CultureInfo.InvariantCulture); break;
                    case "nomodels": o.NoModels = val == "1" || val == "true"; break;
                    case "hidden": o.Hidden = val == "1" || val == "true"; break;
                    case "bodies": o.Bodies = int.Parse(val, CultureInfo.InvariantCulture); break;
                    case "unpaced": o.Unpaced = val == "1" || val == "true"; break;
                    case "rate": o.Rate = double.Parse(val, CultureInfo.InvariantCulture); break;
                    case "profile":
                        if (val != "game" && val != "stress") throw new ArgumentException("--profile must be game|stress");
                        o.Profile = val; break;
                    case "sleeping":
                        if (val != "default" && val != "off") throw new ArgumentException("--sleeping must be default|off");
                        o.SleepingOff = val == "off"; o.sleepingGiven = true; break;
                    case "shadows":
                        if (val != "on" && val != "off") throw new ArgumentException("--shadows must be on|off");
                        o.Shadows = val == "on"; break;
                    case "msaa":
                        o.Msaa = int.Parse(val, CultureInfo.InvariantCulture);
                        if (o.Msaa != 1 && o.Msaa != 2 && o.Msaa != 4 && o.Msaa != 8) throw new ArgumentException("--msaa must be 1|2|4|8");
                        break;
                    case "vsync":
                        if (val != "on" && val != "off") throw new ArgumentException("--vsync must be on|off");
                        o.Vsync = val == "on"; break;
                    case "physics-hz": o.PhysicsHz = double.Parse(val, CultureInfo.InvariantCulture); break;
                    case "camera":
                        if (val != "content" && val != "sky") throw new ArgumentException("--camera must be content|sky");
                        o.CameraSky = val == "sky"; break;
                    case "sun-intensity": o.SunIntensity = float.Parse(val, CultureInfo.InvariantCulture); break;
                    case "ambient-intensity": o.AmbientIntensity = float.Parse(val, CultureInfo.InvariantCulture); break;
                    case "presize": o.Presize = int.Parse(val, CultureInfo.InvariantCulture); break;
                }
            }
            if (o.Mode != "physics" && o.Mode != "render") throw new ArgumentException("--mode must be physics|render");
            o.Layout = o.Scenario; o.FileScenario = o.Scenario;
            switch (o.Scenario)
            {
                case "pit-2000": case "raycast-1000": case "raycast-10000": case "chains": case "mixed": break;
                case "unlock-render-mixed":
                    o.Layout = "mixed"; RequireMode(o, "render"); o.Unlock = true; break;
                case "unlock-physics-mixed":
                    o.Layout = "mixed"; RequireMode(o, "physics"); o.Unpaced = true; break;
                case "unlock-physics-pit-2000":
                    o.Layout = "pit-2000"; RequireMode(o, "physics"); o.Unpaced = true; break;
                case "scale":
                    if (o.Bodies < 1) throw new ArgumentException("--scenario=scale needs --bodies=N");
                    if (o.Unpaced && o.Mode != "physics") throw new ArgumentException("--unpaced=1 is physics mode only");
                    o.Layout = "scale";
                    o.FileScenario = (o.Unpaced ? "scale-unlock-" : "scale-") + o.Bodies.ToString(CultureInfo.InvariantCulture);
                    break;
                case "debris": case "debris-pooled":
                    if (!(o.Rate > 0)) throw new ArgumentException("--scenario=" + o.Scenario + " needs --rate=R (events per second, > 0)");
                    if (o.Unpaced) throw new ArgumentException("--unpaced=1 is not defined for debris");
                    o.Layout = "debris"; o.Pooled = o.Scenario == "debris-pooled";
                    o.FileScenario = o.Scenario + "-" + o.Rate.ToString("0.###", CultureInfo.InvariantCulture);
                    if (!o.WarmupGiven) o.WarmupSeconds = 10.0; // SPEC G: 10 s warm-up (steady state after about 8 s), 20 s measured
                    break;
                default: throw new ArgumentException("unknown --scenario " + o.Scenario);
            }
            // profile defaults (explicit flags win). game = realistic: sleeping default, shadows, MSAA 4x, vsync on for paced render runs. stress = game with sleeping off.
            // chains (C) and debris (G) keep default sleeping in both profiles (the layouts "chains" and "debris"; mixed contains chains but is scenario D).
            if (!o.sleepingGiven) o.SleepingOff = o.Profile == "stress" && o.Layout != "chains" && o.Layout != "debris";
            if (o.Unpaced && o.Scenario != "scale" && !o.Scenario.StartsWith("unlock-physics", StringComparison.Ordinal))
                throw new ArgumentException("--unpaced=1 only with scale or unlock-physics-*");
            return o;
        }

        private static void RequireMode(Options o, string mode)
        {
            if (o.ModeGiven && o.Mode != mode) throw new ArgumentException("--scenario=" + o.Scenario + " implies --mode=" + mode);
            o.Mode = mode;
        }

        public bool Render => Mode == "render";
    }
}
