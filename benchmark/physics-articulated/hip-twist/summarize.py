import json, sys, glob, os, collections
d = sys.argv[1]
rows = collections.defaultdict(list)  # (case, variant, hz) -> list of (tag, run dict)
dets = collections.defaultdict(list)
files = 0
for p in sorted(glob.glob(os.path.join(d, "*.json"))):
    j = json.load(open(p))
    files += 1
    sc = j["scenario"]
    for v in j["variants"]:
        k = (sc["case"], v["variant"], int(sc["step_hz"]))
        dets[k].append(v["deterministic_all_runs_equal"])
        for r in v["runs"]:
            rows[k].append((j["tag"], r))
def mx(lst, key):
    best = None; tag = None
    for t, r in lst:
        v = r.get(key)
        if v is None: v = float("inf")
        if best is None or v > best: best, tag = v, t
    return best, tag
order = ["hold", "swing", "wall", "impact", "limits"]
vorder = ["jolt-swingtwist", "jolt-sixdof", "physx-art-spherical", "physx-d6"]
fmt = lambda v: "null" if v is None else ("inf" if v == float("inf") else "%.3g" % v)
print("files", files)
print("case | variant | Hz | runs | det(files) | nonfinite runs | anchor max m | hinge axis max rad | twist excess | swing excess (ell) | ori err steady | twist err steady | jitter rms | pen m | tunnelled runs | proj min v")
for c in order:
    for v in vorder:
        for hz in (120, 200):
            k = (c, v, hz)
            if k not in rows: continue
            L = rows[k]
            nf = sum(1 for t, r in L if r["nan"] or r["onset_nonfinite_step"] > 0)
            tun = sum(1 for t, r in L if r["tunnelled"])
            det = "%d/%d" % (sum(dets[k]), len(dets[k]))
            m = lambda key: fmt(mx(L, key)[0])
            print(" | ".join([c, v, str(hz), str(len(L)), det, str(nf), m("anchor_err_max_m"), m("hinge_axis_err_max_rad"), m("limit_excess_twist_rad"), m("limit_excess_swing_ellipse_rad"),
                              m("ball_orientation_err_steady_rad"), m("ball_twist_err_steady_rad"), m("jitter_rms_radps_per_step"), m("max_penetration_m"), str(tun), m("impact_body_min_speed_mps")]))
print()
print("worst configs (anchor, axis) per case/variant")
for c in order:
    for v in vorder:
        L = []
        for hz in (120, 200): L += rows.get((c, v, hz), [])
        if not L: continue
        a, ta = mx(L, "anchor_err_max_m"); b, tb = mx(L, "hinge_axis_err_max_rad")
        print(c, v, "anchor", fmt(a), ta, "| axis", fmt(b), tb)
# trigger count
print()
trig = collections.Counter()
for k, L in rows.items():
    for t, r in L:
        if r["nan"] or r["onset_nonfinite_step"] > 0: trig[(k[0], k[1], "nonfinite")] += 1
        if (r["anchor_err_max_m"] is None) or r["anchor_err_max_m"] > 0.1: trig[(k[0], k[1], "anchor>0.1")] += 1
        if (r["hinge_axis_err_max_rad"] is None) or r["hinge_axis_err_max_rad"] > 0.1: trig[(k[0], k[1], "axis>0.1")] += 1
        if r["anchor_err_max_m"] is not None and r["anchor_err_max_m"] > 1.0: trig[(k[0], k[1], "anchor>1")] += 1
for k in sorted(trig): print("TRIGGER", k, trig[k])
