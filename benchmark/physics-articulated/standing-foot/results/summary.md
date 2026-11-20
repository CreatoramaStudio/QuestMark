| backend | sweep | cases | non-finite | anchor > 0.01 m | trigger (anchor > 0.1 m or axis > 0.1 rad or NaN) | anchor > 1 m | stays upright | det equal | anchor max m | axis max rad | foot pen max m | foot slip max m |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| jolt | default | 40 | 0 | 32 | 5 | 0 | 12 | 40 | 0.464 | 0.159 | 0.641 | 40.9 |
| jolt | iters100-30 | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 0.00159 | 0.0213 | 0.0108 | 0.123 |
| physx | default | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 4.48e-05 | 0.000936 | 0.00715 | 0.00283 |
| physx | iters100-30 | 40 | 0 | 0 | 0 | 0 | 40 | 40 | 4.35e-05 | 0.000933 | 0.000338 | 0.000627 |
| physx-maximal | default | 40 | 0 | 40 | 23 | 0 | 0 | 40 | 0.272 | 0.24 | 0.013 | 3.32 |
| physx-maximal | iters100-30 | 40 | 0 | 4 | 0 | 0 | 22 | 40 | 0.0166 | 0.083 | 0.00543 | 1.3 |

Per case (same columns, counts out of 8 = 4 sizes x 2 step rates):

| backend | sweep | case | cases | non-finite | trigger | stays upright | anchor max m | axis max rad | foot pen max m | foot slip max m | sag end max m |
|---|---|---|---|---|---|---|---|---|---|---|---|
| jolt | default | 2leg-stand | 8 | 0 | 0 | 2 | 0.0165 | 0.0305 | 0.0226 | 3.56 | 5.28 |
| jolt | default | 2leg-impact | 8 | 0 | 0 | 2 | 0.0166 | 0.0306 | 0.0225 | 3.43 | 5.26 |
| jolt | default | 1leg-stand | 8 | 0 | 0 | 4 | 0.0605 | 0.0814 | 0.058 | 6.66 | 5.7 |
| jolt | default | 1leg-impact | 8 | 0 | 0 | 4 | 0.061 | 0.0877 | 0.0695 | 6.64 | 5.68 |
| jolt | default | 1leg-swing | 8 | 0 | 5 | 0 | 0.464 | 0.159 | 0.641 | 40.9 | 3.05 |
| jolt | iters100-30 | 2leg-stand | 8 | 0 | 0 | 8 | 0.000281 | 0.000684 | 0.000928 | 0.00151 | 0.039 |
| jolt | iters100-30 | 2leg-impact | 8 | 0 | 0 | 8 | 0.000281 | 0.000665 | 0.000988 | 0.00107 | 0.039 |
| jolt | iters100-30 | 1leg-stand | 8 | 0 | 0 | 8 | 0.000523 | 0.00648 | 0.00281 | 0.00214 | 0.163 |
| jolt | iters100-30 | 1leg-impact | 8 | 0 | 0 | 8 | 0.000523 | 0.00645 | 0.00286 | 0.00215 | 0.165 |
| jolt | iters100-30 | 1leg-swing | 8 | 0 | 0 | 8 | 0.00159 | 0.0213 | 0.0108 | 0.123 | 0.135 |
| physx | default | 2leg-stand | 8 | 0 | 0 | 8 | 3.49e-05 | 0.000853 | 0.00254 | 0.000651 | 0.0427 |
| physx | default | 2leg-impact | 8 | 0 | 0 | 8 | 3.53e-05 | 0.000851 | 0.00264 | 0.000653 | 0.0427 |
| physx | default | 1leg-stand | 8 | 0 | 0 | 8 | 4.45e-05 | 0.000936 | 0.00528 | 0.000846 | 0.171 |
| physx | default | 1leg-impact | 8 | 0 | 0 | 8 | 4.27e-05 | 0.000936 | 0.00528 | 0.000846 | 0.171 |
| physx | default | 1leg-swing | 8 | 0 | 0 | 8 | 4.48e-05 | 0.000936 | 0.00715 | 0.00283 | 0.175 |
| physx | iters100-30 | 2leg-stand | 8 | 0 | 0 | 8 | 3.47e-05 | 0.000862 | 0.000111 | 0.00051 | 0.038 |
| physx | iters100-30 | 2leg-impact | 8 | 0 | 0 | 8 | 3.48e-05 | 0.000869 | 0.000109 | 0.000495 | 0.038 |
| physx | iters100-30 | 1leg-stand | 8 | 0 | 0 | 8 | 4.27e-05 | 0.000875 | 0.000119 | 4.73e-05 | 0.159 |
| physx | iters100-30 | 1leg-impact | 8 | 0 | 0 | 8 | 4.35e-05 | 0.000907 | 0.000127 | 6.64e-05 | 0.159 |
| physx | iters100-30 | 1leg-swing | 8 | 0 | 0 | 8 | 4.29e-05 | 0.000933 | 0.000338 | 0.000627 | 0.167 |
| physx-maximal | default | 2leg-stand | 8 | 0 | 1 | 0 | 0.16 | 0.00283 | 0.00652 | 3.32 | 36.2 |
| physx-maximal | default | 2leg-impact | 8 | 0 | 1 | 0 | 0.159 | 0.00415 | 0.00652 | 3.32 | 36.2 |
| physx-maximal | default | 1leg-stand | 8 | 0 | 7 | 0 | 0.23 | 0.239 | 0.013 | 2.72 | 38.5 |
| physx-maximal | default | 1leg-impact | 8 | 0 | 7 | 0 | 0.244 | 0.237 | 0.013 | 2.71 | 38.5 |
| physx-maximal | default | 1leg-swing | 8 | 0 | 7 | 0 | 0.272 | 0.24 | 0.013 | 2.68 | 38.2 |
| physx-maximal | iters100-30 | 2leg-stand | 8 | 0 | 0 | 7 | 0.00272 | 0.00081 | 0.000705 | 0.000279 | 0.366 |
| physx-maximal | iters100-30 | 2leg-impact | 8 | 0 | 0 | 7 | 0.00272 | 0.001 | 0.000705 | 0.000279 | 0.366 |
| physx-maximal | iters100-30 | 1leg-stand | 8 | 0 | 0 | 3 | 0.00612 | 0.0208 | 0.00267 | 0.00163 | 6.61 |
| physx-maximal | iters100-30 | 1leg-impact | 8 | 0 | 0 | 3 | 0.00612 | 0.0207 | 0.00266 | 0.00163 | 6.61 |
| physx-maximal | iters100-30 | 1leg-swing | 8 | 0 | 0 | 2 | 0.0166 | 0.083 | 0.00543 | 1.3 | 6.51 |
