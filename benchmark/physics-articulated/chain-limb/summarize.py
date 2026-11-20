"""chain-limb: read results/*.json and results/limb/*.json and print markdown tables.
Usage (from the repository root): python -I benchmark/physics-articulated/chain-limb/summarize.py benchmark/physics-articulated/chain-limb/results > <out>.md
Nothing is computed beyond max / sum / count over the JSON values.
Table 1: main chain matrix (files in results/ whose tag is not diag-*), run 1 of each backend.
Table 2: limb matrix (results/limb/), worst value over the size labels of a group (limb, case, backend, step rate)."""
import glob
import json
import os
import re
import sys


def num(x, fmt="%.4g"):
    return "null" if x is None else fmt % x


d = sys.argv[1]

print("## Main chain matrix (run 1 per backend; determinism = all runs of the case equal)\n")
print("| case | backend | p50 ms | p99 ms | impact-window p99 ms | joint err max m | pen max m | tunnelled eps | nan eps | max v m/s | max w rad/s | energy gain | det | timing_note |")
print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
for p in sorted(glob.glob(os.path.join(d, "*.json"))):
    j = json.load(open(p))
    if j["tag"].startswith("diag-"):
        continue
    for r in j["results"]:
        x = r["runs"][0]
        print("| %s | %s | %s | %s | %s | %s | %s | %d/%d | %d | %s | %s | %s | %s | %s |" % (
            j["tag"], r["backend_key"], num(x["physics_step_in_play"]["p50_ms"], "%.4f"), num(x["physics_step_in_play"]["p99_ms"], "%.4f"),
            num(x["physics_step_impact_window"]["p99_ms"], "%.4f"), num(x["joint_pos_err_max_m"]), num(x["max_penetration_m"]), x["episodes_tunnelled"],
            x["episodes"], x["episodes_nan"], num(x["max_link_speed_mps"]), num(x["max_link_ang_speed_radps"]), num(x["energy_gain_max_over_ke0"]),
            "yes" if r["deterministic_all_runs_equal"] else "NO", j["timing_note"]))

print("\n## Limb matrix (worst over the size labels of a group)\n")
groups = {}
for p in sorted(glob.glob(os.path.join(d, "limb", "*.json"))):
    j = json.load(open(p))
    m = re.match(r"limb-(leg|arm)-(h\d+m)-(.+)-(\d+)hz(-shortton)?$", j["tag"])
    limb, size, case, hz, short = m.group(1), m.group(2), m.group(3), int(m.group(4)), bool(m.group(5))
    for r in j["results"]:
        key = (limb, case + (" (short ton)" if short else ""), r["backend_key"], hz)
        g = groups.setdefault(key, dict(files=0, anchor=0.0, axis=0.0, lim=0.0, pen=0.0, tun=0, nan=0, o001=0, o1=0, pd=0.0, vmax=0.0, det=True))
        g["files"] += 1
        g["det"] = g["det"] and r["deterministic_all_runs_equal"]
        for x in r["runs"]:
            g["anchor"] = max(g["anchor"], x["joint_pos_err_max_m"] or 0.0)
            g["axis"] = max(g["axis"], x["joint_axis_err_max_rad"] or 0.0)
            g["lim"] = max(g["lim"], x["joint_limit_excess_max_rad"] or 0.0)
            g["pen"] = max(g["pen"], x["max_penetration_m"] or 0.0)
            g["pd"] = max(g["pd"], x["limb_drive_angle_err_max_rad"] or 0.0)
            g["vmax"] = max(g["vmax"], x["max_link_speed_mps"] or 0.0)
            g["tun"] += x["episodes_tunnelled"]
            g["nan"] += x["episodes_nan"]
        x = r["runs"][0]
        g["o001"] += 1 if x["onset_err_gt_0p01m"]["episode"] else 0
        g["o1"] += 1 if x["onset_err_gt_1m"]["episode"] else 0
print("| limb | case | backend | Hz | files | anchor err max m | axis err max rad | limit excess max rad | pen max m | tunnelled episodes | non-finite episodes | files with onset > 0.01 m | files with onset > 1 m | PD angle err max rad | speed max m/s | det |")
print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
for key in sorted(groups):
    g = groups[key]
    print("| %s | %s | %s | %d | %d | %s | %s | %s | %s | %d | %d | %d | %d | %s | %s | %s |" % (
        key[0], key[1], key[2], key[3], g["files"], num(g["anchor"], "%.3g"), num(g["axis"], "%.3g"), num(g["lim"], "%.3g"), num(g["pen"], "%.3g"), g["tun"], g["nan"],
        g["o001"], g["o1"], num(g["pd"], "%.3g"), num(g["vmax"], "%.3g"), "yes" if g["det"] else "NO"))
