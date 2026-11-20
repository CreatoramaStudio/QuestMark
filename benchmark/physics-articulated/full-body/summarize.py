"""full-body: read results/*.json and print markdown tables (full per-run table, then the aggregate over sizes).
Usage (from the repository root): python -I benchmark/physics-articulated/full-body/summarize.py benchmark/physics-articulated/full-body/results > <out>.md
Every value is read from the JSON; nothing is computed beyond max/min/count over the files."""
import glob
import json
import os
import sys

d = sys.argv[1]
rows = []
for f in sorted(glob.glob(os.path.join(d, "*.json"))):
    try:
        j = json.load(open(f))
    except Exception as e:  # a file that does not parse is reported, never skipped silently
        print("PARSE ERROR", f, e)
        continue
    sc = j["scenario"]
    case = sc["case"]
    if case == "step-debris":
        case = "step+debris %d:1" % round(sc["mass_ratio_ref_to_debris"])
    for be in j["results"]:
        r = be["runs"][0]
        var = j["tag"].replace(sc["size_label"].replace(" ", "_"), "*")
        rows.append(dict(var=var, case=case, size_label=sc["size_label"], hz=round(sc["step_hz"]), be=be["backend_key"], det=be["deterministic_all_runs_equal"], r=r, tag=j["tag"], iters=sc.get("pd_freq_hz")))

def g(r, k):
    return r[k]

def f(v):
    if v is None:
        return "null"
    if isinstance(v, float):
        return "%.3g" % v
    return str(v)

print("## Per run (run 1 of 2; determinism column compares the 2 runs)\n")
print("| case (tag pattern) | size_label | Hz | backend | non-finite step | anchor max m | anchor>0.01 / >0.1 / >1 step | axis max rad | axis>0.1 step | PD err max rad | ground pen m | wall pen m | tunnelled centre/corner | debris below ground / centre in humanoid box | debris awake first/min/last | drift max/final m | torso h ratio min | tilt max rad | det | step ms p50/p99 (NOT VALID FOR TIMING) |")
print("|" + "---|" * 21)
for x in sorted(rows, key=lambda x: (x["var"], x["size_label"], x["be"])):
    r = x["r"]
    print("| %s | %s | %d | %s | %s | %s | %s/%s/%s | %s | %s | %s | %s | %s | %s/%s | %s / %s | %s/%s/%s | %s/%s | %s | %s | %s | %s/%s |" % (
        x["case"] + " (" + x["var"] + ")", x["size_label"], x["hz"], x["be"], r["onset_step_nonfinite"] or "never", f(r["anchor_err_max_m"]), r["onset_step_anchor_gt_0p01m"] or "-", r["onset_step_anchor_gt_0p1m"] or "-",
        r["onset_step_anchor_gt_1m"] or "-", f(r["axis_err_max_rad"]), r["onset_step_axis_gt_0p1rad"] or "-", f(r["pd_angle_err_max_rad"]), f(r["humanoid_ground_penetration_max_m"]),
        f(r["wall_penetration_max_m"]), "Y" if r["tunnelled"] else "n", "Y" if r["tunnelled_corner"] else "n", r["debris_below_ground_max"], r["debris_centre_inside_humanoid_box_max"],
        r["debris_awake"]["first_step"], r["debris_awake"]["min"], r["debris_awake"]["last_step"], f(r["humanoid_drift_max_m"]), f(r["humanoid_drift_final_m"]), f(r["torso_height_ratio_min"]), f(r["torso_tilt_max_rad"]),
        "yes" if x["det"] else "NO", f(r["physics_step_ms"]["p50"]), f(r["physics_step_ms"]["p99"])))

print("\n## Aggregate over the four sizes (worst case per case, step rate, backend)\n")
print("| case (tag pattern) | Hz | backend | sizes | non-finite (sizes) | worst anchor m | sizes anchor>0.01 / >0.1 / >1 | worst axis rad | sizes axis>0.1 | worst tilt rad | min torso h ratio | sizes tunnelled centre / corner | worst debris below ground | worst debris centre in humanoid box | all runs deterministic |")
print("|" + "---|" * 15)
agg = {}
for x in rows:
    agg.setdefault((x["var"], x["hz"], x["be"]), []).append(x)
for k in sorted(agg):
    xs = agg[k]
    rr = [x["r"] for x in xs]
    n = len(xs)
    print("| %s | %d | %s | %d | %d | %s | %d / %d / %d | %s | %d | %s | %s | %d / %d | %d | %d | %s |" % (
        k[0], k[1], k[2], n, sum(1 for r in rr if r["onset_step_nonfinite"]), f(max(r["anchor_err_max_m"] for r in rr)),
        sum(1 for r in rr if r["anchor_err_max_m"] > 0.01), sum(1 for r in rr if r["anchor_err_max_m"] > 0.1), sum(1 for r in rr if r["anchor_err_max_m"] > 1.0),
        f(max(r["axis_err_max_rad"] for r in rr)), sum(1 for r in rr if r["axis_err_max_rad"] > 0.1), f(max(r["torso_tilt_max_rad"] for r in rr)), f(min(r["torso_height_ratio_min"] for r in rr)),
        sum(1 for r in rr if r["tunnelled"]), sum(1 for r in rr if r["tunnelled_corner"]), max(r["debris_below_ground_max"] for r in rr), max(r["debris_centre_inside_humanoid_box_max"] for r in rr),
        "yes" if all(x["det"] for x in xs) else "NO"))
