using System.Diagnostics;
using Stride.BepuPhysics;
using Stride.BepuPhysics.Components;
using Stride.Core;
using Stride.Engine;

namespace StrideBench
{
    /// <summary>
    /// Measures the cost of one BepuSimulation tick. BepuSimulation.Update (internal) calls SimulationUpdate before
    /// Simulation.Timestep and AfterSimulationUpdate after Timestep + ContactEvents.Flush + SyncActiveTransformsWithPhysics.
    /// So the measured interval = Timestep + contact event flush + transform sync (+ UpdatePreviousVelocities is before the hook,
    /// i.e. NOT included).
    /// </summary>
    [DataContract]
    public sealed class PhysicsTimerComponent : EntityComponent, ISimulationUpdate
    {
        private long start;
        [DataMemberIgnore] public double AccumulatedMs;
        [DataMemberIgnore] public int Steps;
        [DataMemberIgnore] public double QueryMs;          // ray batch time summed over the steps of this Update (filled by OnStep)
        [DataMemberIgnore] public System.Action OnStep;    // runs after every step, outside the measured physics interval
        [DataMemberIgnore] public readonly double[] StepMs = new double[256]; // duration of each step of this Update (first 256)

        public void SimulationUpdate(BepuSimulation simulation, float simTimeStep)
        {
            start = Stopwatch.GetTimestamp();
        }

        public void AfterSimulationUpdate(BepuSimulation simulation, float simTimeStep)
        {
            double ms = Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            AccumulatedMs += ms;
            if (Steps < StepMs.Length) StepMs[Steps] = ms;
            Steps++;
            OnStep?.Invoke();
        }
    }
}
