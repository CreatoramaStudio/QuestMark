# full-body: whole articulated humanoid plus debris at 1000:1, Jolt vs PhysX

Part of [physics-articulated](../README.md) (versions, build, skeleton file, size labels, metric definitions and the status of the results are described there). Results come from the original runs; the sources were only renamed for publication. **All timings in the JSON files are not valid (busy machine); this is a correctness and onset test.**

## Question

Does one whole articulated humanoid, scaled to the four sizes, stay closed (anchor error, axis error, no NaN) in Jolt and in PhysX when standing, taking a scripted step, stepping into 500 debris cubes at a nominal 1000:1 or 100:1 mass ratio, being hit by a 2 t cube and swinging an arm into a thin wall? Flag levels (harness choices): anchor error above 0.1 m, axis error above 0.1 rad, any non-finite state; onset 0.01 m; failure 1 m.

## What is built

`humanoid.cpp` (`build_humanoid()`) reads the humanoid skeleton and mesh with ufbx (`--fbx <skeleton.fbx>`, not included; bone names in the parent README) and builds 20 bodies and 19 joints: torso/pelvis (root, free), head (fixed joint), per leg a hip gimbal (abduction hinge about y, pitch hinge about x), thigh, knee hinge, shank, ankle gimbal (pitch hinge about x, roll hinge about y) and foot, per arm a shoulder gimbal (abduction + pitch), upper arm, elbow, forearm, wrist hinge and hand. Hinge-only (no ball joints, no twist); gimbal bodies are small cubes with 10 % of the adjacent link mass. Hinge axes are anatomical candidates, limits +-0.6 rad (abduction, roll) and +-1.5 rad (others) are assumptions. Humanoid links never collide with each other.

Boxes come from the mesh vertices by dominant skin weight, one group per body: torso = bounding box of every vertex not in a limb or head group, feet = flat-soled world-aligned boxes, other links = oriented boxes along joint to joint. Masses: bone-bounding-box volume share of the size's weight, normalised so the sum is the weight (h76m: raw sum 1.763e6 kg against 1.722e6 kg). The largest neighbour ratio in the tree is torso to gimbal cube, about 160:1. PD holds: `kp = I_eff (2 pi f)^2`, `kd = 2 zeta I_eff 2 pi f`, f = 2 Hz, zeta = 1, with `I_eff` from the subtree and the rest of the body (leg joints at least 0.25 x the inertia of the rest); torque cap = max(5 x static gravity torque, 0.25 x sum m g r_perp), leg joints at least 0.5 x M g x hip height (all design choices of this harness).

Backends: `backend_jolt.cpp` (bodies + `HingeConstraint` motors, `FixedConstraint` for the head; default iterations 10 / 2) and `backend_physx.cpp` (one `PxArticulationReducedCoordinate` tree with a free root, 20 links, revolute and fixed joints with twist drives, default iterations 4 / 1; debris, ground, wall and projectile are separate rigid actors in the same scene). Single thread, fixed step, CCD on for humanoid links and projectile (off for debris), library defaults.

| case | what happens | duration |
|---|---|---|
| `stand` | all PD targets 0, humanoid on a ground box | 5 s |
| `step` | scripted joint-angle trajectory, open loop, no balance controller: crouch 12 %, lean over the left foot, lift the right foot 10 %, move it forward, land, lean back | 7 s |
| `step-debris` | the step with 500 debris cubes (300 under the landing spot, 200 around the feet; density 2400 kg/m^3 assumed), debris mass = foot mass / ratio (`--ratio 1000` or `100`) | 7 s |
| `impact` | stand; a 2 t, 1 m cube at 30 m/s hits the torso front | 3 s |
| `wall` | stand; left shoulder pitch undriven, arm launched to a 30 m/s tip speed into a 5 cm slab, 500 debris cubes (1000:1 against the hand link) behind it | 2 s |

## Files

`main.cpp` (cases, scripted targets, metrics, JSON), `humanoid.h` / `humanoid.cpp`, `backend.h`, `backend_jolt.cpp`, `backend_physx.cpp` (also the PhysX articulation limits probe `--probe-physx-limits`), `run_all.sh <build dir> <phase 1|2> [size_label] [note]`, `run_jolt_remedy.sh`, `run_pd_sweep.sh`, `summarize.py` (`python -I summarize.py <results dir>`), `results/` (48 matrix files `<case>-<size>-<Hz>hz.json`, `jolt-remedy/` 24 files, `pd-sweep/` 9 files, `summary.md`, `jolt-remedy/summary.md`). The harness times `step()` itself with `std::chrono` and makes no timing claim; it does not use `common/bench_stats.h`.

## Results

Aggregate over the four sizes, run 1 of 2 (`results/summary.md`); the table with all cases is in the parent README (the 100:1 debris case is only in `summary.md`). In every case of the 48-file matrix the two runs of both libraries gave equal trajectory hashes, no run stopped early, and no state was non-finite (258 runs in the family: matrix 192, remedy set 48, sweep 18).

Jolt remedy set (`results/jolt-remedy/`, Jolt only, `stand` and `step-debris` 1000:1, 2 runs each, 24 cases; a diagnostic, 480 Hz is not one of the two regular rates):

| variant | cases | non-finite | worst anchor m | sizes anchor > 0.1 | worst axis rad | sizes axis > 0.1 | worst tilt rad |
|---|---|---|---|---|---|---|---|
| stand, 120 Hz, 100 / 30 iterations | 4 | 0 | 0.0060 | 0 | 0.017 | 0 | 0.039 |
| stand, 200 Hz, 100 / 30 | 4 | 0 | 0.0023 | 0 | 0.0049 | 0 | 0.015 |
| stand, 480 Hz, default 10 / 2 | 4 | 0 | 0.0053 | 0 | 0.013 | 0 | 0.055 |
| step + debris, 120 Hz, 100 / 30 | 4 | 0 | 0.0087 | 0 | 0.075 | 0 | 1.20 |
| step + debris, 200 Hz, 100 / 30 | 4 | 0 | 0.0046 | 0 | 0.022 | 0 | 0.52 |
| step + debris, 480 Hz, default | 4 | 0 | 0.017 | 0 | 0.048 | 0 | 1.21 |

(In the 120 Hz step + debris remedy row the closure is fine but the open-loop step still tilts 3 of 4 sizes by 0.99 to 1.2 rad.) PD gain sweep (`results/pd-sweep/`, h76m, `stand`, 120 Hz, 1 run per combination, leg inertia multiplier in {0, 0.02, 0.25} x PD frequency in {1, 2, 4} Hz): Jolt at default iterations has anchor error 0.0096 to 0.23 m, axis error 0.021 to 1.04 rad (anchor above 0.1 m in 7 of 9, axis above 0.1 rad in 7 of 9); PhysX has anchor error at most 1.5e-5 m and axis error at most 0.0013 rad in all nine.

PhysX articulation limits probe (`--probe-physx-limits`, CPU, single thread): a plain revolute chain with 20, 63, 64, 65, 128 and 512 links was created, added to a scene and stepped 10 times; every link was created, there were no library errors and the states were finite (no link-count limit was hit up to 512 links in this build; the probe says nothing about speed or accuracy, and it is a chain of simple boxes without contact).

Proved by the data (these 4 sizes, 6 cases, 120 and 200 Hz, one scripted trajectory, one gain set, one machine, one thread, default iterations unless stated):

1. PhysX articulation: anchor error at most 4.7e-5 m and axis error at most 0.00125 rad in every case (including the step into 500 debris at 1000:1 and 100:1, the 2 t impact and the wall hit); none of the levels was reached. The open-loop step leaves its torso tilted 0.31 to 0.46 rad at the end (a property of the script, not of closure).
2. Jolt with default iterations exceeds the flag levels in the standing, step and step-with-debris cases at 120 Hz (anchor above 0.1 m and axis above 0.1 rad in 4 of 4 sizes; `stand` worst 0.144 m and 1.06 rad) and in the step cases at 200 Hz (anchor above 0.1 m in 1 of 4 sizes, axis above 0.1 rad in 4 of 4; `stand` at 200 Hz: anchor 0 of 4, axis 1 of 4 at 0.375 rad). The humanoid sags and tips (`stand` at 120 Hz: torso tilt up to 1.5 rad, torso height ratio down to 0.63). It does not exceed them in the impact and wall cases (anchor at most 0.031 m, axis at most 0.067 rad), but its anchor error exceeds 0.01 m in 4 of 4 sizes in every Jolt case (PhysX: never).
3. The Jolt collapse depends on solver effort and step rate: with 100 / 30 iterations at 120 Hz or default iterations at 480 Hz the standing humanoid held (anchor at most 0.0060 m). Raising the PD frequency or the leg inertia multiplier did not cure it in the sweep; only the two 1 Hz combinations with a small multiplier stayed below 0.1 m and 0.1 rad, and they still sag.
4. Debris (step-with-debris cases): both libraries have 500 awake at the first step and 0 to 2 at the 0.5 s sample; PhysX never had a debris centre below the ground plane, Jolt up to 4; debris centres inside a humanoid box (deep interpenetration): PhysX at most 2, Jolt at most 4 (the feet).
5. Wall: in all 16 wall cases (4 sizes x 2 rates x 2 libraries, run 1) an arm-box corner was recorded behind the back face of the 5 cm slab (maximum penetration 0.074 to 0.201 m), no link centre went through, the penetration at the end of the run was 0, and no debris crossed the wall.

Hypotheses (not checked): why Jolt's default iterations fail here (solver convergence of a 20-body tree with neighbour mass ratios up to 160:1 under gravity and PD motors; the iteration and step-rate dependence is consistent with it, the soft motor formulation against the articulation drive is an alternative; not separated); whether a balance controller would keep the step bounded; whether 100 / 30 iterations are affordable in a real scene (timing not measured).

## Limits of this sample

One machine and session; one scripted trajectory; one gain set (a 9-point sweep on one size and one case); four sizes of fan-sourced data; boxes only (no compound shapes); debris cubes only; humanoid-humanoid collisions off; hinge-only joints plus a fixed head; 2 runs per case; nominal mass ratios (the effective mass behind a foot is the whole humanoid, about 80:1 to the foot link for h76m); no multithreaded run; the left hand group gives a much wider box than the right (h76m: half x 17.7 m against 5.0 m; the mesh cause was not checked), so the left arm is much heavier than the right. Raised iterations and 480 Hz were run for `stand` and `step-debris` 1000:1 only.
