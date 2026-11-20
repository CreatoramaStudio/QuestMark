## Main chain matrix (run 1 per backend; determinism = all runs of the case equal)

| case | backend | p50 ms | p99 ms | impact-window p99 ms | joint err max m | pen max m | tunnelled eps | nan eps | max v m/s | max w rad/s | energy gain | det | timing_note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| alternating-120hz-ccdoff-t1 | jolt | 0.0228 | 0.0351 | 0.0358 | 32.43 | 23.05 | 5/6 | 5 | 500 | 47.12 | 953.6 | yes | idle machine, other applications closed, 1 session |
| alternating-120hz-ccdoff-t1 | physx | 0.0167 | 0.0389 | 0.2537 | 2.348e-06 | 0.3069 | 0/6 | 0 | 28.75 | 27.34 | 0.0001087 | yes | idle machine, other applications closed, 1 session |
| alternating-120hz-ccdon-t1 | jolt | 0.0233 | 0.0346 | 0.0317 | 34.67 | 9.783 | 5/6 | 5 | 500 | 47.12 | 953 | yes | idle machine, other applications closed, 1 session |
| alternating-120hz-ccdon-t1 | physx | 0.0198 | 0.0601 | 0.2214 | 2.348e-06 | 0.3069 | 0/6 | 0 | 28.75 | 27.34 | 0.0001087 | yes | idle machine, other applications closed, 1 session |
| alternating-200hz-ccdoff-t1 | jolt | 0.0181 | 0.0300 | 0.0305 | 26.33 | 11.59 | 3/4 | 3 | 500 | 47.12 | 953.9 | yes | idle machine, other applications closed, 1 session |
| alternating-200hz-ccdoff-t1 | physx | 0.0167 | 0.0325 | 0.2450 | 2.379e-06 | 0.02063 | 0/4 | 0 | 28.75 | 23.89 | 3.939e-05 | yes | idle machine, other applications closed, 1 session |
| alternating-200hz-ccdon-t1 | jolt | 0.0173 | 0.0322 | 0.0306 | 29.11 | 14.14 | 3/4 | 3 | 500 | 47.12 | 953.1 | yes | idle machine, other applications closed, 1 session |
| alternating-200hz-ccdon-t1 | physx | 0.0176 | 0.0332 | 0.2322 | 2.379e-06 | 0.02063 | 0/4 | 0 | 28.75 | 23.89 | 3.939e-05 | yes | idle machine, other applications closed, 1 session |
| geometric-100kg-120hz-ccdon-t1 | jolt | 0.0187 | 0.0311 | 0.0351 | 0.1229 | 0.06587 | 0/6 | 0 | 28.75 | 21.1 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-100kg-120hz-ccdon-t1 | physx | 0.0199 | 0.0626 | 0.1768 | 2.299e-06 | 0.1581 | 0/6 | 0 | 28.75 | 13.5 | 0.0001072 | yes | idle machine, other applications closed, 1 session |
| geometric-100kg-200hz-ccdon-t1 | jolt | 0.0187 | 0.0299 | 0.0352 | 0.05074 | 0.06256 | 0/4 | 0 | 28.77 | 21.24 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-100kg-200hz-ccdon-t1 | physx | 0.0184 | 0.0363 | 0.1725 | 2.384e-06 | 0.05868 | 0/4 | 0 | 28.75 | 15.61 | 3.876e-05 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdoff-t1 | jolt | 0.0247 | 0.0388 | 0.0415 | 0.1186 | 5.827 | 6/6 | 0 | 28.75 | 16.44 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdoff-t1 | physx | 0.0195 | 0.2466 | 0.3899 | 2.453e-06 | 0.1452 | 0/6 | 0 | 28.75 | 12.79 | 0.0001068 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdon-t1 | jolt | 0.0187 | 0.0325 | 0.0358 | 0.1171 | 0.0634 | 0/6 | 0 | 28.75 | 47.12 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdon-t1 | physx | 0.0198 | 0.0579 | 0.1750 | 2.453e-06 | 0.1452 | 0/6 | 0 | 28.75 | 12.79 | 0.0001068 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdon-t4 | jolt | 0.0485 | 0.0833 | 0.0993 | 0.1171 | 0.0634 | 0/6 | 0 | 28.75 | 47.12 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-120hz-ccdon-t4 | physx | 0.1445 | 0.2605 | 0.3146 | 2.453e-06 | 0.1452 | 0/6 | 0 | 28.75 | 12.79 | 0.0001068 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdoff-t1 | jolt | 0.0187 | 0.0314 | 0.0335 | 0.01939 | 0.08623 | 0/4 | 0 | 28.77 | 16.44 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdoff-t1 | physx | 0.0179 | 0.0335 | 0.2435 | 2.444e-06 | 0.05267 | 0/4 | 0 | 28.75 | 17.87 | 3.861e-05 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdon-t1 | jolt | 0.0188 | 0.0309 | 0.0329 | 0.04099 | 0.05312 | 0/4 | 0 | 28.77 | 21.36 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdon-t1 | physx | 0.0197 | 0.0356 | 0.1732 | 2.444e-06 | 0.05267 | 0/4 | 0 | 28.75 | 17.87 | 3.861e-05 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdon-t4 | jolt | 0.0466 | 0.0721 | 0.0994 | 0.04099 | 0.05312 | 0/4 | 0 | 28.77 | 21.36 | 0 | yes | idle machine, other applications closed, 1 session |
| geometric-200hz-ccdon-t4 | physx | 0.1309 | 0.1828 | 0.3213 | 2.444e-06 | 0.05267 | 0/4 | 0 | 28.75 | 17.87 | 3.861e-05 | yes | idle machine, other applications closed, 1 session |

## Limb matrix (worst over the size labels of a group)

| limb | case | backend | Hz | files | anchor err max m | axis err max rad | limit excess max rad | pen max m | tunnelled episodes | non-finite episodes | files with onset > 0.01 m | files with onset > 1 m | PD angle err max rad | speed max m/s | det |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| arm | hold | jolt | 120 | 4 | 0.0014 | 0.00507 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0958 | 2.29 | yes |
| arm | hold | jolt | 200 | 4 | 0.000502 | 0.0013 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0366 | 1.91 | yes |
| arm | hold | physx | 120 | 4 | 1.8e-05 | 0.000818 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00121 | 0.0562 | yes |
| arm | hold | physx | 200 | 4 | 1.81e-05 | 0.000855 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00122 | 0.0541 | yes |
| arm | hold (short ton) | jolt | 120 | 1 | 0.0014 | 0.00457 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0881 | 2.28 | yes |
| arm | hold (short ton) | jolt | 200 | 1 | 0.000502 | 0.00115 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0323 | 1.86 | yes |
| arm | hold (short ton) | physx | 120 | 1 | 8.4e-06 | 0.000771 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00108 | 0.0561 | yes |
| arm | hold (short ton) | physx | 200 | 1 | 8.32e-06 | 0.000807 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00109 | 0.054 | yes |
| arm | hold-worstaxis | jolt | 120 | 4 | 0.00242 | 0.312 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00738 | 8.46 | yes |
| arm | hold-worstaxis | jolt | 200 | 4 | 0.000679 | 0.116 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00551 | 5.08 | yes |
| arm | hold-worstaxis | physx | 120 | 4 | 1.29e-05 | 0.000684 | 0 | 0 | 0 | 0 | 0 | 0 | 6.16e-07 | 7.85e-06 | yes |
| arm | hold-worstaxis | physx | 200 | 4 | 1.29e-05 | 0.000684 | 0 | 0 | 0 | 0 | 0 | 0 | 5.93e-07 | 7.31e-06 | yes |
| arm | impact | jolt | 120 | 4 | 0.0014 | 0.00221 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0169 | 1.57 | yes |
| arm | impact | jolt | 200 | 4 | 0.000502 | 0.00109 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0159 | 1.06 | yes |
| arm | impact | physx | 120 | 4 | 1.79e-05 | 0.000831 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00191 | 0.341 | yes |
| arm | impact | physx | 200 | 4 | 1.81e-05 | 0.000814 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00202 | 0.362 | yes |
| arm | impact (short ton) | jolt | 120 | 1 | 0.0014 | 0.00186 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0121 | 1.58 | yes |
| arm | impact (short ton) | jolt | 200 | 1 | 0.000502 | 0.001 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0156 | 1.02 | yes |
| arm | impact (short ton) | physx | 120 | 1 | 7.43e-06 | 0.000796 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00153 | 0.282 | yes |
| arm | impact (short ton) | physx | 200 | 1 | 9.19e-06 | 0.000803 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0016 | 0.295 | yes |
| arm | swing | jolt | 120 | 4 | 0.0104 | 0.0411 | 0 | 0 | 0 | 0 | 1 | 0 | 0.0459 | 23.8 | yes |
| arm | swing | jolt | 200 | 4 | 0.00415 | 0.0222 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0298 | 23.8 | yes |
| arm | swing | physx | 120 | 4 | 1.85e-05 | 0.000881 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00194 | 23.9 | yes |
| arm | swing | physx | 200 | 4 | 1.93e-05 | 0.000873 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00194 | 23.9 | yes |
| arm | swing (short ton) | jolt | 120 | 1 | 0.00921 | 0.0381 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0422 | 16.3 | yes |
| arm | swing (short ton) | jolt | 200 | 1 | 0.00387 | 0.0203 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0272 | 16.3 | yes |
| arm | swing (short ton) | physx | 120 | 1 | 8.14e-06 | 0.000875 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00173 | 16.3 | yes |
| arm | swing (short ton) | physx | 200 | 1 | 8.91e-06 | 0.00085 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00173 | 16.3 | yes |
| arm | wall | jolt | 120 | 4 | 0.0105 | 0.0178 | 0 | 0.188 | 0 | 0 | 1 | 0 | 0.357 | 24.3 | yes |
| arm | wall | jolt | 200 | 4 | 0.00824 | 0.0176 | 0 | 0.129 | 0 | 0 | 0 | 0 | 0.357 | 24.3 | yes |
| arm | wall | physx | 120 | 4 | 2.04e-05 | 0.0008 | 0 | 0.189 | 0 | 0 | 0 | 0 | 0.435 | 24.3 | yes |
| arm | wall | physx | 200 | 4 | 1.77e-05 | 0.000818 | 0 | 0.0868 | 0 | 0 | 0 | 0 | 0.417 | 24.3 | yes |
| arm | wall (short ton) | jolt | 120 | 1 | 0.0098 | 0.0176 | 0 | 0.0165 | 0 | 0 | 0 | 0 | 0.331 | 24.3 | yes |
| arm | wall (short ton) | jolt | 200 | 1 | 0.00823 | 0.0176 | 0 | 0.0801 | 0 | 0 | 0 | 0 | 0.355 | 24.3 | yes |
| arm | wall (short ton) | physx | 120 | 1 | 7.33e-06 | 0.00082 | 0 | 0.00858 | 0 | 0 | 0 | 0 | 0.368 | 24.3 | yes |
| arm | wall (short ton) | physx | 200 | 1 | 7.69e-06 | 0.000777 | 0 | 0.0597 | 0 | 0 | 0 | 0 | 0.389 | 24.3 | yes |
| leg | hold | jolt | 120 | 4 | 0.000963 | 0.00212 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0237 | 2.09 | yes |
| leg | hold | jolt | 200 | 4 | 0.000341 | 0.00066 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0132 | 0.978 | yes |
| leg | hold | physx | 120 | 4 | 1.14e-05 | 0.000809 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00571 | 0.0975 | yes |
| leg | hold | physx | 200 | 4 | 1.3e-05 | 0.000856 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00571 | 0.0991 | yes |
| leg | hold (short ton) | jolt | 120 | 1 | 0.000963 | 0.00193 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0208 | 2.04 | yes |
| leg | hold (short ton) | jolt | 200 | 1 | 0.000341 | 0.000645 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0118 | 0.977 | yes |
| leg | hold (short ton) | physx | 120 | 1 | 5.82e-06 | 0.000786 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0051 | 0.0975 | yes |
| leg | hold (short ton) | physx | 200 | 1 | 6.9e-06 | 0.000856 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0051 | 0.0991 | yes |
| leg | hold-worstaxis | jolt | 120 | 4 | 0.00104 | 0.00276 | 0 | 0 | 0 | 0 | 0 | 0 | 1.25e-06 | 0.182 | yes |
| leg | hold-worstaxis | jolt | 200 | 4 | 0.000375 | 0.000919 | 0 | 0 | 0 | 0 | 0 | 0 | 1.15e-06 | 0.11 | yes |
| leg | hold-worstaxis | physx | 120 | 4 | 7.72e-06 | 0.000797 | 0 | 0 | 0 | 0 | 0 | 0 | 1.44e-07 | 7.97e-09 | yes |
| leg | hold-worstaxis | physx | 200 | 4 | 7.72e-06 | 0.000797 | 0 | 0 | 0 | 0 | 0 | 0 | 1.44e-07 | 6e-09 | yes |
| leg | impact | jolt | 120 | 4 | 0.00188 | 0.00178 | 0 | 0 | 0 | 0 | 0 | 0 | 0.021 | 1.6 | yes |
| leg | impact | jolt | 200 | 4 | 0.00126 | 0.000626 | 0 | 0 | 0 | 0 | 0 | 0 | 0.019 | 1.21 | yes |
| leg | impact | physx | 120 | 4 | 1.2e-05 | 0.000831 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0069 | 0.995 | yes |
| leg | impact | physx | 200 | 4 | 1.37e-05 | 0.00086 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0069 | 0.977 | yes |
| leg | impact (short ton) | jolt | 120 | 1 | 0.00206 | 0.00156 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0193 | 1.47 | yes |
| leg | impact (short ton) | jolt | 200 | 1 | 0.000429 | 0.000613 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0143 | 1.06 | yes |
| leg | impact (short ton) | physx | 120 | 1 | 5.36e-06 | 0.000808 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00603 | 0.775 | yes |
| leg | impact (short ton) | physx | 200 | 1 | 6.58e-06 | 0.000821 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00598 | 0.751 | yes |
| leg | swing | jolt | 120 | 4 | 0.00714 | 0.000603 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0122 | 26.6 | yes |
| leg | swing | jolt | 200 | 4 | 0.00297 | 0.000658 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0119 | 26.6 | yes |
| leg | swing | physx | 120 | 4 | 1.63e-05 | 0.000862 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0115 | 26.7 | yes |
| leg | swing | physx | 200 | 4 | 1.49e-05 | 0.000864 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0115 | 26.7 | yes |
| leg | swing (short ton) | jolt | 120 | 1 | 0.00647 | 0.000546 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0109 | 18.3 | yes |
| leg | swing (short ton) | jolt | 200 | 1 | 0.00265 | 0.000591 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0107 | 18.3 | yes |
| leg | swing (short ton) | physx | 120 | 1 | 6.83e-06 | 0.000844 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0102 | 18.3 | yes |
| leg | swing (short ton) | physx | 200 | 1 | 7.99e-06 | 0.000864 | 0 | 0 | 0 | 0 | 0 | 0 | 0.0102 | 18.3 | yes |
| leg | wall | jolt | 120 | 4 | 0.038 | 0.00409 | 0 | 0.109 | 0 | 0 | 4 | 0 | 0.159 | 27.5 | yes |
| leg | wall | jolt | 200 | 4 | 0.0171 | 0.00231 | 0 | 0.0486 | 0 | 0 | 4 | 0 | 0.153 | 27.5 | yes |
| leg | wall | physx | 120 | 4 | 1.33e-05 | 0.000844 | 0 | 0.0538 | 0 | 0 | 0 | 0 | 0.149 | 27.5 | yes |
| leg | wall | physx | 200 | 4 | 1.53e-05 | 0.00085 | 0 | 0.0271 | 0 | 0 | 0 | 0 | 0.133 | 27.5 | yes |
| leg | wall (short ton) | jolt | 120 | 1 | 0.038 | 0.000904 | 0 | 0.109 | 0 | 0 | 1 | 0 | 0.0907 | 27.5 | yes |
| leg | wall (short ton) | jolt | 200 | 1 | 0.0167 | 0.00067 | 0 | 0.0486 | 0 | 0 | 1 | 0 | 0.112 | 27.5 | yes |
| leg | wall (short ton) | physx | 120 | 1 | 6.19e-06 | 0.000848 | 0 | 0.00402 | 0 | 0 | 0 | 0 | 0.0695 | 27.5 | yes |
| leg | wall (short ton) | physx | 200 | 1 | 5.79e-06 | 0.000823 | 0 | 0.00221 | 0 | 0 | 0 | 0 | 0.0716 | 27.5 | yes |
