using System;

namespace StrideBench
{
    internal static class Program
    {
        [System.Runtime.InteropServices.DllImport("winmm.dll")] private static extern uint timeBeginPeriod(uint ms);

        // Native AOT: Stride finds the Bepu SimUpdateProcessor of this component by reflecting over its interfaces (ProcessorManager.IntroduceComponent: GetInterfaces, IComponent<,> type arguments, Activator.CreateInstance).
        // Without this the interface list is trimmed and PhysicsTimerComponent is never hooked (0 steps seen by the timer). No effect in the JIT build.
        [System.Diagnostics.CodeAnalysis.DynamicDependency(System.Diagnostics.CodeAnalysis.DynamicallyAccessedMemberTypes.Interfaces, typeof(PhysicsTimerComponent))]
        private static int Main(string[] args)
        {
            Options opts;
            try { opts = Options.Parse(args); }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.Message);
                Console.Error.WriteLine("usage: StrideBench --scenario=pit-2000|raycast-1000|raycast-10000|chains|mixed|unlock-render-mixed|unlock-physics-mixed|unlock-physics-pit-2000|scale [--bodies=N --unpaced=1]|debris|debris-pooled [--rate=R] --mode=physics|render --run=1 --out=<dir> [--warmup=5 --measure=20 --uncapped=1 --camera=content|sky --profile=game|stress --sleeping=default|off --shadows=on|off --msaa=1|2|4|8 --vsync=on|off --physics-hz=200  --sun-intensity=F --ambient-intensity=F]");
                return 2;
            }
            if (opts.TimerRes1ms) timeBeginPeriod(1);
            using (var game = new BenchGame(opts))
            {
                game.Run();
                return game.ExitCode;
            }
        }
    }
}
