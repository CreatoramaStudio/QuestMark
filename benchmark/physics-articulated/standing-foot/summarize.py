"""Summarise results/*.json of the standing-foot test into markdown. Usage: python -I summarize.py <results dir> <out table.md> <out summary.md>"""
import glob
import json
import os
import sys

res, out_table, out_summary = sys.argv[1:4]
rows = []
for f in sorted(glob.glob(os.path.join(res, "stand-*.json"))):
    d = json.load(open(f))
    sweep = "iters100-30" if "iters100-30" in f else "default"
    for c in d["cases"]:
        r = c["runs"]
        a = r[0]
        # worst over the runs (both runs should be identical when deterministic)
        def mx(k):
            v = [x[k] for x in r]
            return None if any(t is None for t in v) else max(v)
        rows.append(dict(size_label=d["size_label"], hz=d["step_hz"], sweep=sweep, backend=c["backend_key"], stance=c["stance"], case=c["case"],
                         nf=a["onset_nonfinite_step"] if a["nonfinite"] else 0, anchor=mx("anchor_err_max_m"), axis=mx("axis_err_max_rad"),
                         lim=mx("joint_limit_excess_max_rad"), slip=mx("foot_slip_max_m"), pen=mx("foot_penetration_max_m"), sag=a["pelvis_sag_end_m"],
                         sagmax=a["pelvis_sag_max_m"], tilt=a["pelvis_tilt_max_rad"], up=a["stays_upright"], det=c["deterministic_all_runs_equal"],
                         onset01=a["onset_anchor_gt_0p1m"], onsetax=a["onset_axis_gt_0p1rad"], z0=a["pelvis_z0_m"], steps=a["steps_done"], planned=a["steps_planned"],
                         dx=a["pelvis_dx_max_m"], fclr=a["free_foot_clearance_min_m"], trk=a["free_hip_track_err_max_rad"], tgt=a["free_hip_target_max_rad"]))

def f(v, fmt):
    return "NaN" if v is None else (fmt % v)

with open(out_table, "w") as o:
    o.write("| size_label | Hz | sweep | backend | case | non-finite step | anchor max m | axis max rad | limit excess rad | foot slip m | foot pen m | sag end m | sag max m | tilt max rad | upright | det |\n")
    o.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for r in rows:
        o.write("| %s | %g | %s | %s | %s-%s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n" % (
            r["size_label"], r["hz"], r["sweep"], r["backend"], r["stance"], r["case"], r["nf"] if r["nf"] else "-", f(r["anchor"], "%.2e"), f(r["axis"], "%.2e"), f(r["lim"], "%.2e"),
            f(r["slip"], "%.3f"), f(r["pen"], "%.4f"), f(r["sag"], "%.4f"), f(r["sagmax"], "%.4f"), f(r["tilt"], "%.4f"), "yes" if r["up"] else "NO", "yes" if r["det"] else "NO"))

def agg(sel):
    n = len(sel)
    nf = sum(1 for r in sel if r["nf"])
    trig = sum(1 for r in sel if (r["anchor"] is None or r["anchor"] > 0.1 or r["axis"] is None or r["axis"] > 0.1))
    a01 = sum(1 for r in sel if r["anchor"] is not None and r["anchor"] > 0.01)
    a1 = sum(1 for r in sel if r["anchor"] is not None and r["anchor"] > 1.0)
    up = sum(1 for r in sel if r["up"])
    det = sum(1 for r in sel if r["det"])
    am = max((r["anchor"] for r in sel if r["anchor"] is not None), default=0)
    xm = max((r["axis"] for r in sel if r["axis"] is not None), default=0)
    pm = max((r["pen"] for r in sel if r["pen"] is not None), default=0)
    sm = max((r["slip"] for r in sel if r["slip"] is not None), default=0)
    return n, nf, a01, trig, a1, up, det, am, xm, pm, sm

with open(out_summary, "w") as o:
    o.write("| backend | sweep | cases | non-finite | anchor > 0.01 m | trigger (anchor > 0.1 m or axis > 0.1 rad or NaN) | anchor > 1 m | stays upright | det equal | anchor max m | axis max rad | foot pen max m | foot slip max m |\n")
    o.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for be in ["jolt", "physx", "physx-maximal"]:
        for sw in ["default", "iters100-30"]:
            sel = [r for r in rows if r["backend"] == be and r["sweep"] == sw]
            if not sel:
                continue
            n, nf, a01, trig, a1, up, det, am, xm, pm, sm = agg(sel)
            o.write("| %s | %s | %d | %d | %d | %d | %d | %d | %d | %.3g | %.3g | %.3g | %.3g |\n" % (be, sw, n, nf, a01, trig, a1, up, det, am, xm, pm, sm))
    o.write("\nPer case (same columns, counts out of 8 = 4 sizes x 2 step rates):\n\n")
    o.write("| backend | sweep | case | cases | non-finite | trigger | stays upright | anchor max m | axis max rad | foot pen max m | foot slip max m | sag end max m |\n|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for be in ["jolt", "physx", "physx-maximal"]:
        for sw in ["default", "iters100-30"]:
            for cs in ["2leg-stand", "2leg-impact", "1leg-stand", "1leg-impact", "1leg-swing"]:
                sel = [r for r in rows if r["backend"] == be and r["sweep"] == sw and (r["stance"] + "-" + r["case"]) == cs]
                if not sel:
                    continue
                n, nf, a01, trig, a1, up, det, am, xm, pm, sm = agg(sel)
                sg = max((abs(r["sag"]) for r in sel if r["sag"] is not None), default=0)
                o.write("| %s | %s | %s | %d | %d | %d | %d | %.3g | %.3g | %.3g | %.3g | %.3g |\n" % (be, sw, cs, n, nf, trig, up, am, xm, pm, sm, sg))
print(len(rows), "rows")

# ---- Jolt remedy ladder (optional 4th argument: output file)
if len(sys.argv) > 4:
    lad = {}
    for f in sorted(glob.glob(os.path.join(res, "ladder-*.json"))):
        d = json.load(open(f))
        var = os.path.basename(f).rsplit("-", 1)[1][:-5] if False else os.path.basename(f)[:-5].split("hz-")[1]
        for c in d["cases"]:
            a = c["runs"][0]
            lad.setdefault(var, []).append((d["size_label"], d["step_hz"], c["stance"] + "-" + c["case"], a["nonfinite"], a["anchor_err_max_m"], a["axis_err_max_rad"], a["pelvis_sag_end_m"], a["pelvis_tilt_max_rad"], a["stays_upright"], c["deterministic_all_runs_equal"]))
    with open(sys.argv[4], "w") as o:
        o.write("Jolt remedy ladder: 4 sizes x 2 step rates per row group, cases 2leg-stand and 1leg-swing (16 runs per variant). 'default' = library default iterations (vel 10, pos 2) from the main matrix.\n\n")
        o.write("| variant | runs | non-finite | stays upright | anchor > 0.01 m | trigger (anchor > 0.1 m or axis > 0.1 rad) | anchor max m | axis max rad | tilt max rad | sag end max m (abs) |\n|---|---|---|---|---|---|---|---|---|---|\n")
        base = [r for r in rows if r["backend"] == "jolt" and r["sweep"] == "default" and (r["stance"] + "-" + r["case"]) in ("2leg-stand", "1leg-swing")]
        lad["default"] = [(r["size_label"], r["hz"], r["stance"] + "-" + r["case"], bool(r["nf"]), r["anchor"], r["axis"], r["sag"], r["tilt"], r["up"], r["det"]) for r in base]
        base2 = [r for r in rows if r["backend"] == "jolt" and r["sweep"] == "iters100-30" and (r["stance"] + "-" + r["case"]) in ("2leg-stand", "1leg-swing")]
        lad["it100-30"] = [(r["size_label"], r["hz"], r["stance"] + "-" + r["case"], bool(r["nf"]), r["anchor"], r["axis"], r["sag"], r["tilt"], r["up"], r["det"]) for r in base2]
        for var in ["default", "it20-4", "it30-10", "it50-15", "it100-30", "kp4", "kp4-it50-15"]:
            L = lad.get(var, [])
            if not L:
                continue
            n = len(L)
            o.write("| %s | %d | %d | %d | %d | %d | %.3g | %.3g | %.3g | %.3g |\n" % (var, n, sum(1 for x in L if x[3]), sum(1 for x in L if x[8]), sum(1 for x in L if x[4] is not None and x[4] > 0.01),
                    sum(1 for x in L if (x[4] is None or x[4] > 0.1 or x[5] is None or x[5] > 0.1)), max(x[4] for x in L if x[4] is not None), max(x[5] for x in L if x[5] is not None),
                    max(x[7] for x in L if x[7] is not None), max(abs(x[6]) for x in L if x[6] is not None)))
