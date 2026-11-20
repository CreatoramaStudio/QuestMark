## Per run (run 1 of 2; determinism column compares the 2 runs)

| case (tag pattern) | size_label | Hz | backend | non-finite step | anchor max m | anchor>0.01 / >0.1 / >1 step | axis max rad | axis>0.1 step | PD err max rad | ground pen m | wall pen m | tunnelled centre/corner | debris below ground / centre in humanoid box | debris awake first/min/last | drift max/final m | torso h ratio min | tilt max rad | det | step ms p50/p99 (NOT VALID FOR TIMING) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| stand (stand-*-120hz-iters100_30) | h85m | 120 | jolt | never | 0.00603 | -/-/- | 0.00931 | - | 0.0349 | 0.0207 | 0 | n/n | 0 / 0 | 0/0/0 | 0.785/0.526 | 0.997 | 0.0177 | yes | 0.205/0.229 |
| stand (stand-*-120hz-iters100_30) | h76m | 120 | jolt | never | 0.00554 | -/-/- | 0.0169 | - | 0.0548 | 0.0212 | 0 | n/n | 0 / 0 | 0/0/0 | 1.2/0.673 | 0.997 | 0.0393 | yes | 0.207/0.315 |
| stand (stand-*-120hz-iters100_30) | h96m | 120 | jolt | never | 0.00553 | -/-/- | 0.00417 | - | 0.0314 | 0.0211 | 0 | n/n | 0 / 0 | 0/0/0 | 0.531/0.531 | 0.998 | 0.0156 | yes | 0.206/0.245 |
| stand (stand-*-120hz-iters100_30) | h182m | 120 | jolt | never | 0.00522 | -/-/- | 0.00381 | - | 0.0173 | 0.0206 | 0 | n/n | 0 / 0 | 0/0/0 | 0.689/0.689 | 0.999 | 0.0106 | yes | 0.205/0.229 |
| stand (stand-*-200hz-iters100_30) | h85m | 200 | jolt | never | 0.00182 | -/-/- | 0.00435 | - | 0.0176 | 0.00867 | 0 | n/n | 0 / 0 | 0/0/0 | 0.771/0.771 | 0.999 | 0.0136 | yes | 0.207/0.347 |
| stand (stand-*-200hz-iters100_30) | h76m | 200 | jolt | never | 0.00171 | -/-/- | 0.00492 | - | 0.022 | 0.00713 | 0 | n/n | 0 / 0 | 0/0/0 | 0.612/0.612 | 0.999 | 0.0152 | yes | 0.206/0.335 |
| stand (stand-*-200hz-iters100_30) | h96m | 200 | jolt | never | 0.00174 | -/-/- | 0.00374 | - | 0.0159 | 0.00919 | 0 | n/n | 0 / 0 | 0/0/0 | 0.818/0.818 | 0.999 | 0.0121 | yes | 0.206/0.232 |
| stand (stand-*-200hz-iters100_30) | h182m | 200 | jolt | never | 0.00233 | -/-/- | 0.00168 | - | 0.00686 | 0.0101 | 0 | n/n | 0 / 0 | 0/0/0 | 0.462/0.462 | 0.999 | 0.00648 | yes | 0.207/0.357 |
| stand (stand-*-480hz-defaults) | h85m | 480 | jolt | never | 0.00529 | -/-/- | 0.0126 | - | 0.0475 | 0.0094 | 0 | n/n | 0 / 0 | 0/0/0 | 1.62/1.62 | 0.998 | 0.0413 | yes | 0.0295/0.0861 |
| stand (stand-*-480hz-defaults) | h76m | 480 | jolt | never | 0.00532 | -/-/- | 0.00499 | - | 0.0341 | 0.00704 | 0 | n/n | 0 / 0 | 0/0/0 | 1.45/1.45 | 0.998 | 0.0547 | yes | 0.0295/0.0532 |
| stand (stand-*-480hz-defaults) | h96m | 480 | jolt | never | 0.0053 | -/-/- | 0.00491 | - | 0.0169 | 0.007 | 0 | n/n | 0 / 0 | 0/0/0 | 1.79/1.79 | 0.998 | 0.0145 | yes | 0.0295/0.0368 |
| stand (stand-*-480hz-defaults) | h182m | 480 | jolt | never | 0.0052 | -/-/- | 0.00227 | - | 0.0102 | 0.00684 | 0 | n/n | 0 / 0 | 0/0/0 | 2.06/2.06 | 0.999 | 0.00525 | yes | 0.0297/0.0739 |
| step+debris 1000:1 (step-debris-*-120hz-iters100_30) | h85m | 120 | jolt | never | 0.00874 | -/-/- | 0.0751 | - | 0.473 | 0.0668 | 0 | n/n | 0 / 2 | 500/0/14 | 18.2/18.2 | 0.578 | 1.08 | yes | 0.277/1.18 |
| step+debris 1000:1 (step-debris-*-120hz-iters100_30) | h76m | 120 | jolt | never | 0.00716 | -/-/- | 0.0703 | - | 0.527 | 0.0668 | 0 | n/n | 1 / 2 | 500/0/2 | 16.8/16.8 | 0.677 | 1.2 | yes | 0.296/1.41 |
| step+debris 1000:1 (step-debris-*-120hz-iters100_30) | h96m | 120 | jolt | never | 0.00363 | -/-/- | 0.0394 | - | 0.354 | 0.0333 | 0 | n/n | 0 / 3 | 500/0/6 | 21.1/21.1 | 0.804 | 0.991 | yes | 0.278/0.854 |
| step+debris 1000:1 (step-debris-*-120hz-iters100_30) | h182m | 120 | jolt | never | 0.00708 | -/-/- | 0.0299 | - | 0.319 | 0.0574 | 0 | n/n | 2 / 2 | 500/2/21 | 45.6/45.6 | 0.875 | 0.302 | yes | 0.305/1.39 |
| step+debris 1000:1 (step-debris-*-200hz-iters100_30) | h85m | 200 | jolt | never | 0.00114 | -/-/- | 0.0157 | - | 0.225 | 0.0309 | 0 | n/n | 0 / 0 | 500/0/15 | 13.2/13.2 | 0.873 | 0.169 | yes | 0.305/1.29 |
| step+debris 1000:1 (step-debris-*-200hz-iters100_30) | h76m | 200 | jolt | never | 0.00166 | -/-/- | 0.0219 | - | 0.247 | 0.0361 | 0 | n/n | 0 / 0 | 500/0/1 | 13/13 | 0.877 | 0.52 | yes | 0.278/1.21 |
| step+debris 1000:1 (step-debris-*-200hz-iters100_30) | h96m | 200 | jolt | never | 0.000759 | -/-/- | 0.0195 | - | 0.161 | 0.0201 | 0 | n/n | 0 / 0 | 500/0/4 | 14/14 | 0.876 | 0.255 | yes | 0.305/0.886 |
| step+debris 1000:1 (step-debris-*-200hz-iters100_30) | h182m | 200 | jolt | never | 0.00456 | -/-/- | 0.00971 | - | 0.394 | 0.0515 | 0 | n/n | 1 / 1 | 500/1/5 | 36.3/36.3 | 0.89 | 0.421 | yes | 0.268/1.53 |
| step+debris 1000:1 (step-debris-*-480hz-defaults) | h85m | 480 | jolt | never | 0.0129 | 2802/-/- | 0.0476 | - | 0.9 | 0.0211 | 0 | n/n | 0 / 0 | 500/0/3 | 17.1/17.1 | 0.668 | 1.21 | yes | 0.101/0.487 |
| step+debris 1000:1 (step-debris-*-480hz-defaults) | h76m | 480 | jolt | never | 0.017 | 2372/-/- | 0.038 | - | 1.5 | 0.0211 | 0 | n/n | 0 / 1 | 500/0/27 | 14.2/14.2 | 0.695 | 1.2 | yes | 0.119/0.484 |
| step+debris 1000:1 (step-debris-*-480hz-defaults) | h96m | 480 | jolt | never | 0.00875 | -/-/- | 0.036 | - | 0.31 | 0.0219 | 0 | n/n | 0 / 0 | 500/0/4 | 19.3/19.3 | 0.877 | 0.768 | yes | 0.114/0.516 |
| step+debris 1000:1 (step-debris-*-480hz-defaults) | h182m | 480 | jolt | never | 0.0121 | 599/-/- | 0.0219 | - | 0.301 | 0.0225 | 0 | n/n | 1 / 1 | 500/0/3 | 38.1/38.1 | 0.879 | 0.443 | yes | 0.0726/0.509 |

## Aggregate over the four sizes (worst case per case, step rate, backend)

| case (tag pattern) | Hz | backend | sizes | non-finite (sizes) | worst anchor m | sizes anchor>0.01 / >0.1 / >1 | worst axis rad | sizes axis>0.1 | worst tilt rad | min torso h ratio | sizes tunnelled centre / corner | worst debris below ground | worst debris centre in humanoid box | all runs deterministic |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| stand-*-120hz-iters100_30 | 120 | jolt | 4 | 0 | 0.00603 | 0 / 0 / 0 | 0.0169 | 0 | 0.0393 | 0.997 | 0 / 0 | 0 | 0 | yes |
| stand-*-200hz-iters100_30 | 200 | jolt | 4 | 0 | 0.00233 | 0 / 0 / 0 | 0.00492 | 0 | 0.0152 | 0.999 | 0 / 0 | 0 | 0 | yes |
| stand-*-480hz-defaults | 480 | jolt | 4 | 0 | 0.00532 | 0 / 0 / 0 | 0.0126 | 0 | 0.0547 | 0.998 | 0 / 0 | 0 | 0 | yes |
| step-debris-*-120hz-iters100_30 | 120 | jolt | 4 | 0 | 0.00874 | 0 / 0 / 0 | 0.0751 | 0 | 1.2 | 0.578 | 0 / 0 | 2 | 3 | yes |
| step-debris-*-200hz-iters100_30 | 200 | jolt | 4 | 0 | 0.00456 | 0 / 0 / 0 | 0.0219 | 0 | 0.52 | 0.873 | 0 / 0 | 1 | 1 | yes |
| step-debris-*-480hz-defaults | 480 | jolt | 4 | 0 | 0.017 | 3 / 0 / 0 | 0.0476 | 0 | 1.21 | 0.668 | 0 / 0 | 1 | 1 | yes |
