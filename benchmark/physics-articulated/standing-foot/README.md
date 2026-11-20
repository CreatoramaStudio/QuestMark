# standing-foot: two-leg and one-leg stance under body weight, Jolt vs PhysX

Part of [physics-articulated](../README.md) (versions, build, skeleton file, size labels, metric definitions and the status of the results are described there). Results come from the original runs; the sources were only renamed for publication. **All timings in the JSON files are not valid (busy machine); this is a correctness and onset test.**

## Question

Does a leg chain from the humanoid skeleton, standing on a ground box under real body weight (the root is not fixed to the world), stay closed and upright in Jolt and in PhysX? Flag levels (harness choices): hinge anchor error above 0.1 m, hinge axis error above 0.1 rad, any non-finite state; onset 0.01 m; failure 1 m. "Stays upright" = pelvis tilt never above 0.1 rad AND pelvis sag at the end below 10 % of the pelvis height AND the run reached its last step.

## Scene (built by `main.cpp`, using `../chain-limb/limb.cpp`)

- Legs: the left-leg chain of the chain-limb limb builder (gimbal with abduction and pitch hinges, thigh, shank, foot; limits +-0.6 / +-1.5 rad; masses by bounding-box volume share), scaled to the size label; the right leg is the mirror about x = 0 (checked: the file's right and left upper-leg bones lie at x = -0.6430 and +0.6430).
- Foot: replaced by an upright flat box (the file foot box is tilted about 29 degrees, which would give a one-edge contact): horizontal extent of the file foot box, sole at the ground, top at the ankle anchor height, mass of the file foot box.
- Pelvis: one free box that carries everything not in the two legs (total mass = body weight; for h85m 2.412e6 kg = pelvis 2.001e6 kg (83.0 %) + 2 x 0.205e6 kg). Width 1.6 x and depth 1.0 x the hip offset are assumptions; the box centre is over the feet (two legs) or over the stance hip (one leg; assumption).
- Ground: static box, friction 0.8 for ground and bodies, restitution 0, CCD on. PD gains by the chain-limb rule (kp = I_eff (2 pi 2 Hz)^2, zeta 1); the torque cap of a stance leg is raised to cover the ground reaction (cap = max(chain-limb cap, 5 x (ground-reaction torque + gravity torque), 0.1 x leg force x hip height), centre of pressure assumed under the ankle; values are in each result file under `scene.hinges`).
- Cases per size and rate: `2leg-stand`, `2leg-impact`, `1leg-stand`, `1leg-impact`, `1leg-swing`, 5 s each. Impact: a 2000 kg, 1 m cube at 30 m/s along +x aimed at the pelvis centre, gravity off for it, reaching the pelvis at t = 1 s. Swing: from t = 1 s the free hip pitch target follows 0.3 rad x (1 - cos(2 pi 0.5 t)) / 2 (0.5 Hz, 2 periods). The transition from two legs to one is not simulated (one-leg cases start on one leg).
- Backends: `jolt` (maximal coordinates, hinge constraints with position motors, default iterations 10 / 2), `physx` (reduced-coordinate articulation whose root is the floating pelvis, default 4 / 1), `physx-maximal` (rigid dynamics + `PxD6Joint` with a twist position drive; a control).
- Sweeps: `default` (library-default iterations) and `iters100-30` (velocity 100, position 30 for every library). `run_ladder.sh` runs a Jolt-only ladder (iterations 20/4, 30/10, 50/15 and gains x 4) on the cases `2leg-stand` and `1leg-swing`.

## Files

`main.cpp`, `stand.h`, `backend_jolt.cpp`, `backend_physx.cpp`, `run_all.sh` (main matrix), `run_ladder.sh` (Jolt ladder), `summarize.py` (`python -I summarize.py <results dir> <table.md> <summary.md> [<ladder.md>]`), `results/` (`stand-<size>-<Hz>hz-<sweep>.json`, `ladder-<size>-<Hz>hz-<variant>.json`, `table.md` = all 240 backend cases, `summary.md`, `ladder.md`). The skeleton reader and limb builder are compiled from `../chain-limb`.

Command shape: `standing_foot.exe --fbx <skeleton.fbx> --size_label h85m --hz 120 --runs 2 [--iters 100,30] [--backend jolt,physx,physx-maximal] [--cases 2leg-stand,...] --tag <tag> --note <text> --out <json>`.

## Results

240 backend cases = 4 sizes x 2 rates x 5 cases x 3 backends x 2 sweeps, 2 runs each (`results/summary.md`; counts are cases):

| backend | sweep | cases | non-finite | anchor > 0.01 m | flagged | anchor > 1 m | stays upright | det equal | anchor max m | axis max rad | foot pen max m | foot slip max m |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| jolt | default | 40 | 0 | 32 | 5 | 0 | 12 | 40 | 0.464 | 0.159 | 0.641 | 40.9 |
| jolt | iters100-30 | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 0.00159 | 0.0213 | 0.0108 | 0.123 |
| physx | default | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 4.48e-05 | 0.000936 | 0.00715 | 0.00283 |
| physx | iters100-30 | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 4.35e-05 | 0.000933 | 0.000338 | 0.000627 |
| physx-maximal | default | 40 | 0 | 40 | 23 | 0 | 0 | 40 | 0.272 | 0.24 | 0.013 | 3.32 |
| physx-maximal | iters100-30 | 40 | 0 | 4 | 0 | 0 | 22 | 40 | 0.0166 | 0.083 | 0.00543 | 1.3 |

Jolt remedy ladder (Jolt only, cases 2leg-stand and 1leg-swing, 4 sizes x 2 rates = 16 runs per variant; `results/ladder.md`):

| variant | non-finite | stays upright | anchor > 0.01 m | flagged | anchor max m | axis max rad | tilt max rad | sag end max m |
|---|---|---|---|---|---|---|---|---|
| default 10/2 | 0 | 2 | 12 | 5 | 0.464 | 0.159 | 0.472 | 5.28 |
| 20/4 | 0 | 8 | 8 | 1 | 0.256 | 0.0622 | 0.275 | 6.45 |
| 30/10 | 0 | 12 | 4 | 0 | 0.0801 | 0.0438 | 0.38 | 3.28 |
| 50/15 | 0 | 16 | 0 | 0 | 0.00893 | 0.0337 | 0.0888 | 0.258 |
| 100/30 | 0 | 16 | 0 | 0 | 0.00159 | 0.0213 | 0.0721 | 0.135 |
| gains x 4 (default iterations) | 0 | 1 | 16 | 6 | 0.583 | 0.295 | 0.563 | 6.39 |
| gains x 4, 50/15 | 0 | 10 | 5 | 1 | 0.176 | 0.0399 | 0.216 | 1.02 |

What the data proves (this scene, builder and gain set; desktop CPU; one machine, one thread; 120 and 200 Hz; 2 runs per case):

- No non-finite state in 240 of 240 backend cases (and none in the 80 ladder runs); no anchor error above 1 m anywhere; equal trajectory hashes in the two runs of every case.
- PhysX articulation (floating pelvis root): no flag in 80 of 80 cases, all stay upright, anchor error at most 4.5e-5 m.
- Jolt with library-default iterations does not hold the stance: only 12 of 40 cases stay upright (200 Hz 10 of 20, 120 Hz 2 of 20), pelvis sag at 5 s up to 9.8 % (two legs) and 10.6 % (one leg) of the pelvis height, foot slip up to 40.9 m; the flag fired in 5 of 40 cases, all `1leg-swing`. With 100 / 30 iterations it holds in all 40 cases; 50 / 15 is enough in the ladder cases; raising the gains x 4 at default iterations makes it worse, so iterations rather than stiffness mattered here.
- The impact is a very small disturbance (2000 kg x 30 m/s = 6e4 N s against a pelvis of 1.4e6 to 5.6e6 kg: an expected speed change of 0.011 to 0.042 m/s; measured peak lateral pelvis speed 0.010 to 0.038 m/s against 0.006 m/s in the matching stand case), so stand and impact cases have the same pass/fail counts for every backend and sweep. This test cannot tell which library resists a real shove.

Hypotheses (not checked): the Jolt failure at default iterations is a solver-convergence effect of a heavy pelvis (up to 5.6e6 kg) on light legs (mass ratios of 60:1 to 400:1 along the chain, 160:1 pelvis to foot); supported by the ladder but not isolated (mass ratios, constraint priorities, body order and inertia multipliers were not varied here). Whether 100/30 iterations are affordable in a larger scene was not measured. The `physx-maximal` one-leg sag may come from a too soft D6 drive at default iterations rather than a limit of maximal coordinates.

## Limits

Geometry and masses are partly assumed (flat foot, pelvis box and mass distribution in one box, centre of pressure, joint limits, stance leg chosen as the left one); the transition from two legs to one, walking, foot roll, hip twist, uneven ground and arm motion are not simulated; 5 s runs; one impact and one swing amplitude and frequency; the sag at 5 s is a snapshot (Jolt at default oscillates; the maximum over the run is stored as `pelvis_sag_max_m`); 2 runs per case give determinism only, not variance.
