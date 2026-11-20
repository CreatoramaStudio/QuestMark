# hip-twist: 3-DOF hip/shoulder (swing + twist), Jolt vs PhysX

Part of [physics-articulated](../README.md) (versions, build, skeleton file, size labels, metric definitions and the status of the results are described there). Results come from the original runs; the sources were only renamed for publication. **All timings in the JSON files are not valid (busy machine); this is a correctness and onset test.**

## Question

The chain-limb limbs model the hip and shoulder as a small gimbal body with two hinges and no twist. Does a real 3-DOF ball joint with swing limits, a twist limit and drives on the three axes fire any of the flag levels (non-finite state, anchor error above 0.1 m, axis error above 0.1 rad; onset 0.01 m; failure 1 m; all harness choices)?

## What is built

- One scene description (`hip.h`, `Spec`: 3 bodies, 2 hinges, 1 ball joint, wall or projectile) that all four variants build. Chain: world (fixed) - ball joint - thigh / upper arm - hinge (knee / elbow) - shank / forearm - hinge (ankle / wrist) - foot / hand. Boxes, masses and hinge axes come from the chain-limb limb builder (left leg standing, left arm hanging); its gimbal body (10 % of the thigh mass) is dropped and its mass not re-added.
- Variants: `jolt-swingtwist` (`SwingTwistConstraint`: cone swing limit, swing motor + twist motor), `jolt-sixdof` (`SixDOFConstraint`: three translations fixed, three rotations limited, one motor per axis), `physx-art-spherical` (reduced-coordinate articulation, `eSPHERICAL` joint, twist/swing1/swing2 all limited with a drive per axis), `physx-d6` (maximal coordinates: `PxD6Joint` ball with swing-twist drives; its knee/ankle are `PxD6Joint`s with only twist free). Knee and ankle are hinges with position motors in the Jolt variants and revolute joints with drives in the PhysX articulation.
- Ball frame (assumption): x = long axis of the thigh / upper arm, y = pitch axis, z = x cross y (abduction). Limits (assumptions): twist +-1.2 rad, swing about y +-1.2 rad, about z +-0.6 rad, elliptic cone. PD gains per ball axis by the chain-limb rule with the inertia of the three downstream bodies; the twist cap is raised to 1.2 x kp; in the `limits` case all caps x 100. Jolt's `SwingTwistConstraint` has one swing motor for both swing axes: the gains of the stiffer axis are used.
- Cases (each reports all four variants; 120 and 200 Hz; CCD on; 1 thread; 2 runs per variant): `hold` (4 s, neutral pose, 3-axis PD), `swing` (8 s, start pose raised 0.9 rad about pitch and rotated 60 degrees about the long axis, twist held, the two swing axes free under gravity), `wall` (2 s, rigid launch about the pitch axis at 30 m/s toward a 5 cm wall placed 0.5 s of flight ahead; the twist drive aims at 1.6 rad, beyond the 1.2 rad limit, so the twist limit is reached on the way), `impact` (2 s, hold plus a 2000 kg 1 m cube at 30 m/s on the last body), `limits` (4 s, all three drives aim beyond the limits).
- Limit excess is measured by the harness (swing x twist decomposition of the thigh rotation in the neutral frame, several definitions are stored per run) and is not each library's own definition; compare libraries with care.

## Files

`main.cpp` (case set-up, metrics, JSON), `hip.h`, `jolt3.cpp`, `physx3.cpp`, `run_all.sh` (`run_all.sh <build dir> [limb ...]`), `summarize.py` (`python -I summarize.py <results dir>`), `results/<limb>-<size>-<case>-<Hz>hz.json` (80 files; 4 variants x 2 runs each, 640 runs), `results/summary.txt` (output of `summarize.py` on the published JSON). Environment variables read by `main.cpp` for diagnostics (`HIP_DIAG_NOTWIST`, `HIP_DIAG_SWINGDRIVE`, `HIP_DIAG_WALL_NOTWIST`, `HIP_DIAG_WALL_TW=<rad>`, `HIP_DBG`) were not set for the published files. The limb builder and skeleton reader are compiled from `../chain-limb`.

## Results

Worst value over the 4 sizes, 2 limbs, 2 rates and 2 runs (`results/summary.txt`); anchor error in m, "runs" = 16 per case, variant and rate:

| case | jolt-swingtwist | jolt-sixdof | physx-art-spherical | physx-d6 |
|---|---|---|---|---|
| hold | 0.000251 | 0.000251 | 2.08e-05 | 0.00061 |
| swing | 0.00266 | 0.00266 | 2.48e-05 | 0.00102 |
| wall | 0.35 | 0.384 | 2.55e-05 | 0.361 |
| impact | 0.00191 | 0.00191 | 2.07e-05 | 0.000952 |
| limits | 0.149 | 0.136 | 2.43e-05 | 1.77 |

Runs over 0.1 m anchor error (of 160 per variant): `jolt-swingtwist` 10 and `jolt-sixdof` 10 (arm configurations of the two largest sizes in the wall and limits cases; the two runs of a configuration are identical, so 10 = 5 configurations x 2 runs), `physx-art-spherical` 0 (maximum 2.6e-5 m), `physx-d6` 54 (22 in wall, 32 in limits; over 1 m in 2 runs, 1.77 m at most).

Proved by these files (desktop CPU, one machine, left leg and left arm of one skeleton, 4 sizes, 120 and 200 Hz, one thread, library-default iterations): no non-finite state, no tunnelling and no hinge axis error above 0.1 rad (largest 0.087 rad) in any of the 640 runs; equal trajectory hashes in the two runs of all 320 file-variants; in hold, swing and impact the anchor error stays below 2.7e-3 m for Jolt (both constraint types) and the PhysX D6 variant and below 2.5e-5 m for the PhysX articulation. Where the twist limit is deliberately driven into (wall and limits), Jolt's anchor error grows to 0.33 to 0.38 m (wall, arm, largest size) and 0.10 to 0.15 m (limits), stays finite and below 1 m. In a control on one configuration (arm, h182m, wall, 200 Hz, `jolt-swingtwist`, 1 run each) the anchor error rose with the commanded twist: no twist 0.003 m, 0.6 rad 0.022 m, 0.9 rad 0.076 m, 1.6 rad (limit reached) 0.35 m. The two Jolt variants agree within a few per cent in every column. The libraries do not enforce the same swing limit: in each library's own cone measure the PhysX articulation exceeded the swing cone by 0.76 to 1.03 of the limit in the wall and limits cases (its exponential-map components stayed within 0.084 rad of the limits) and kept moving at the limit (angular velocity RMS up to 2.4 rad/s), while Jolt and `physx-d6` stayed within 0.01 of the cone.

Hypotheses (not checked): the PhysX articulation spherical joint limits each exponential-map component separately (a box, not a cone) and the limit is soft against a strong drive (only the header note on `setJointPosition` was read, not the solver source); the rise of the Jolt anchor error with twist comes from the active twist limit, or from the changed contact geometry of a limb twisted by about 1 rad hitting the wall, or both (not separated); the larger error for the arm than the leg because its boxes are thinner relative to the mass is a guess.

## Limits

Timing was not measured on an idle machine; limits and gains are assumptions (the x100 cap of the `limits` case and the twist cap are choices of this harness); only one reading of the `swing` case (free swing from a raised, twisted pose) was run; no standing foot, no 1000:1 mass ratio inside a chain, no full body, no debris, no several limbs, one skeleton, Jolt iterations only at the default. A harness mapping bug in the Jolt SwingTwist variant (its constraint space is twist axis, plane x twist, plane axis, which swapped pitch and abduction) was found and fixed before the published runs; every published file was produced after the fix.
