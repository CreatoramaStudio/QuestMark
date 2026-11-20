#!/usr/bin/env python
"""Writes the charts of the articulated-body tests (charts/*.svg) and charts/DATA.md (the plotted values) from the PUBLISHED result JSON files only.

Usage (from the repository root):  python -I benchmark/physics-articulated/tools/gen-charts.py
Nothing is hand-typed into the figures: every plotted number is a max, min, median or count over values read from the JSON files under
chain-limb/results, standing-foot/results, hip-twist/results and full-body/results. No simulation is run.
Style: transparent background (the page shows through), text, axis and series colours switch with prefers-color-scheme (light / dark GitHub theme).
Series colours are the first three slots of the validated categorical palette, checked with --pairs all in both modes (the aqua slot is below 3:1
contrast on the light surface, so every bar is also labelled or tabulated in DATA.md). Series: blue = Jolt, orange = PhysX articulation, aqua = PhysX maximal / D6.
"""
import glob
import html
import json
import math
import os
import re
import statistics
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
OUT = os.path.join(ROOT, "charts")
os.makedirs(OUT, exist_ok=True)


def P(*a):
    return os.path.join(ROOT, *a)


def load(p):
    with open(p) as f:
        return json.load(f)


def esc(s):
    return html.escape(str(s), quote=True)


DATA = []  # markdown sections of charts/DATA.md


def table(headers, rows):
    out = "| " + " | ".join(headers) + " |\n|" + "---|" * len(headers) + "\n"
    for r in rows:
        out += "| " + " | ".join(str(c) for c in r) + " |\n"
    return out


def g3(v):
    return "none" if v is None else ("%.3g" % v)


# ---------------------------------------------------------------- SVG kit
CSS = (".t{fill:#1f2328}.m{fill:#57606a}.ax{stroke:#8c959f}.g{stroke:#d0d7de}.f{stroke:#57606a;stroke-dasharray:4 3}.ring{fill:none;stroke:#1f2328;stroke-width:2}"
       ".s1{fill:#2a78d6}.s2{fill:#eb6834}.s3{fill:#1baf7a}.best{fill:#1a7f37}"
       "@media (prefers-color-scheme:dark){.t{fill:#e6edf3}.m{fill:#9198a1}.ax{stroke:#6e7681}.g{stroke:#30363d}.f{stroke:#9198a1}.ring{stroke:#e6edf3}"
       ".s1{fill:#3987e5}.s2{fill:#d95926}.s3{fill:#199e70}.best{fill:#3fb950}}")
# short names written next to a green bar when a group has several series, because green hides the series colour (a series-coloured marker left of the axis also shows it)
SHORT = {"jolt": "Jolt", "jolt2": "Jolt six-DOF", "physx": "PhysX art.", "pmax": "PhysX D6"}
DEFS = ('<defs><pattern id="hs1" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
        '<rect width="6" height="6" class="s1" fill-opacity=".35"/><rect width="3" height="6" class="s1"/></pattern></defs>')
SERIES = {  # key -> (css class or fill, legend text)
    "jolt": ("s1", "Jolt"),
    "jolt2": ("hs1", "Jolt (second variant, hatched)"),
    "physx": ("s2", "PhysX articulation"),
    "pmax": ("s3", "PhysX maximal / D6 (control)"),
}


def bar_attr(key):
    c = SERIES[key][0]
    return 'fill="url(#hs1)" class="h"' if c == "hs1" else 'class="%s"' % c


def tickfmt(v):
    if v == 0:
        return "0"
    if 1e-3 <= abs(v) < 1e4:
        return "%g" % v
    e = int(round(math.log10(abs(v))))
    return "1e%d" % e


def best_map(rows, better, none_best, gof):
    """Best value per comparison group. Groups with a single bar have no best (nothing to compare). Ties are all best.
    Returns {group: best} where best None means 'no result' is the best outcome (none_best)."""
    grp = {}
    for i, r in enumerate(rows):
        for b in r[1]:
            grp.setdefault(gof(i, r), []).append(b[1])
    out = {}
    for g, vs in grp.items():
        if len(vs) < 2:
            continue
        real = [v for v in vs if v is not None]
        if none_best and len(real) < len(vs):
            out[g] = None
        elif real:
            out[g] = min(real) if better == "lo" else max(real)
    return out


def is_best(v, best):
    if best is None:
        return v is None
    return v is not None and math.isclose(v, best, rel_tol=1e-9, abs_tol=0.0)


def panel(x, y, w, lw, rows, scale, title=None, flags=(), bar_h=11, gap=2, labels=True, show_val=lambda v: True, valfmt=g3, none_text=None, ax_label=None, pad_r=62,
          better="lo", pct=None, none_best=False, group=None):
    """rows: [(label, [(series_key, value, extra_text)], optional group id)]. scale = ("lin"|"log", lo, hi, ticks).
    better = "lo" or "hi": direction of the metric. The best value in each comparison group (default: the row; group=lambda i, row: id) is drawn green, ties all green;
    a green bar gets a marker in its series colour left of the axis and, when its row has several series, the series name in the value label.
    pct = "lo"/"hi": append ' · NN%' (percentage of best) to each label; only for metrics with a non-zero best. none_best: 'no result' (None) is the best outcome.
    Returns (svg fragment, height)."""
    kind, lo, hi, ticks = scale
    gof = group or (lambda i, r: r[2] if len(r) > 2 else i)
    bestm = best_map(rows, better, none_best, gof)
    px0 = x + lw
    pw = w - lw - pad_r
    n = max(len(r[1]) for r in rows)
    pitch = n * bar_h + (n - 1) * gap + 7

    def sx(v):
        if kind == "log":
            v = max(v, lo)
            return px0 + (math.log10(v) - math.log10(lo)) / (math.log10(hi) - math.log10(lo)) * pw
        return px0 + (v - lo) / (hi - lo) * pw

    s = []
    top = y + (18 if title else 2) + (14 if flags else 0)
    if title:
        s.append('<text x="%g" y="%g" font-size="13" font-weight="600" class="t">%s</text>' % (x, y + 12, esc(title)))
    H = len(rows) * pitch
    for t in ticks:
        xx = sx(t)
        s.append('<line x1="%.1f" y1="%g" x2="%.1f" y2="%g" class="g" stroke-width="1"/>' % (xx, top - 2, xx, top + H))
        s.append('<text x="%.1f" y="%g" font-size="11" text-anchor="middle" class="m">%s</text>' % (xx, top + H + 13, tickfmt(t)))
    s.append('<line x1="%.1f" y1="%g" x2="%.1f" y2="%g" class="ax" stroke-width="1.5"/>' % (px0, top - 2, px0, top + H))
    for fv, ft in flags:
        xx = sx(fv)
        s.append('<line x1="%.1f" y1="%g" x2="%.1f" y2="%g" class="f" stroke-width="1.2"/>' % (xx, top - 3, xx, top + H))
        s.append('<text x="%.1f" y="%g" font-size="10.5" text-anchor="middle" class="m">%s</text>' % (xx, top - 6, esc(ft)))
    for i, r in enumerate(rows):
        lab, bars = r[0], r[1]
        ry = top + i * pitch
        bst = bestm.get(gof(i, r), "nogroup")
        if labels:
            s.append('<text x="%g" y="%.1f" font-size="12" text-anchor="end" class="t">%s</text>' % (px0 - 14, ry + (len(bars) * (bar_h + gap)) / 2 + 3, esc(lab)))
        for j, (key, v, extra) in enumerate(bars):
            by = ry + j * (bar_h + gap)
            green = bst != "nogroup" and is_best(v, bst)
            tag = (" · " + SHORT[key]) if (green and len(bars) > 1) else ""
            if green:  # series marker left of the axis: the bar itself is green, so the series colour is kept here
                s.append('<rect x="%.1f" y="%.1f" width="5" height="%g" %s/>' % (px0 - 9, by, bar_h, bar_attr(key).replace('fill="url(#hs1)" class="h"', 'class="s1"')))
            if v is None:
                if green:
                    s.append('<rect x="%.1f" y="%.1f" width="12" height="%g" rx="2" class="best"/>' % (px0, by, bar_h))
                    s.append('<text x="%.1f" y="%.1f" font-size="11" class="t">%s</text>' % (px0 + 18, by + bar_h - 2, esc((none_text or "none") + " (best)")))
                else:
                    s.append('<text x="%.1f" y="%.1f" font-size="11" class="m">%s</text>' % (px0 + 6, by + bar_h - 2, esc(none_text or "none")))
                continue
            wd = max(sx(v) - px0, 1.5)
            attr = 'class="best"' if green else bar_attr(key)
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%g" rx="2" %s/>' % (px0, by, wd, bar_h, attr))
            if show_val(v):
                ptxt = ""
                if pct and bst != "nogroup" and bst is not None and bst > 0 and v > 0:
                    ptxt = " · %d%%" % int(round((v / bst if pct == "hi" else bst / v) * 100))
                s.append('<text x="%.1f" y="%.1f" font-size="11" class="t">%s<tspan class="m">%s%s</tspan></text>' % (px0 + wd + 5, by + bar_h - 2, esc(valfmt(v) + extra), esc(ptxt), esc(tag)))
    if ax_label:
        s.append('<text x="%.1f" y="%g" font-size="11" text-anchor="middle" class="m">%s</text>' % (px0 + pw / 2, top + H + 27, esc(ax_label)))
    return "\n".join(s), (top + H + (30 if ax_label else 16)) - y


BEST_TEXT = "green = best in its group (ties all green); marker left of the axis = series colour"


def legend(items, x, y, W=860):
    """Legend rows; wraps to a new row when the next entry would pass the right edge. Returns (svg, rows)."""
    s = []
    cx = x
    rows = 1
    for kind, key, text in items:
        wd = 20 + 6.4 * len(text) + 18 + (14 if kind == "best" else 0)
        if cx > x and cx + wd > W - 24:
            cx = x
            y += 20
            rows += 1
        if kind == "bar":
            s.append('<rect x="%g" y="%g" width="14" height="10" rx="2" %s/>' % (cx, y - 9, bar_attr(key)))
        elif kind == "best":
            s.append('<rect x="%g" y="%g" width="5" height="10" class="s1"/><rect x="%g" y="%g" width="14" height="10" rx="2" class="best"/>' % (cx, y - 9, cx + 8, y - 9))
            cx += 14
        else:
            s.append('<circle cx="%g" cy="%g" r="4" class="ring"/>' % (cx + 7, y - 4))
        s.append('<text x="%g" y="%g" font-size="12" class="t">%s</text>' % (cx + 20, y, esc(text)))
        cx += 20 + 6.4 * len(text) + 18
    return "\n".join(s), rows


def svg_doc(name, W, title, subtitle, legend_items, body, body_h, caption, desc, direction):
    """direction: the sentence stating which way is better (drawn bold under the subtitle and put into the accessible description)."""
    sub = textwrap.wrap(subtitle, int((W - 48) / 7.0))
    dirl = textwrap.wrap(direction, int((W - 48) / 7.0))
    legend_items = list(legend_items) + [("best", None, BEST_TEXT)]
    lsvg, lrows = legend(legend_items, 24, 48 + 17 * len(sub) + 17 * len(dirl) + 8, W)
    desc = desc + " " + direction + " Best result in each comparison group drawn green."
    head = 50 + 17 * len(sub) + 17 * len(dirl) + 20 * lrows + 2
    caption = [ln for c in caption for ln in textwrap.wrap(c, int((W - 48) / 6.3))]
    cap_y = head + body_h + 16
    H = cap_y + 16 * len(caption) + 10
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="Segoe UI, Helvetica, Arial, sans-serif" role="img" aria-labelledby="t d">' % (W, H, W, H),
         "<title id=\"t\">%s</title><desc id=\"d\">%s</desc>" % (esc(title), esc(desc)),
         "<style>%s</style>" % CSS, DEFS,
         '<text x="24" y="28" font-size="17" font-weight="600" class="t">%s</text>' % esc(title),
         ]
    for i, ln in enumerate(sub):
        s.append('<text x="24" y="%d" font-size="12.5" class="m">%s</text>' % (47 + 17 * i, esc(ln)))
    for i, ln in enumerate(dirl):
        s.append('<text x="24" y="%d" font-size="12.5" font-weight="600" class="t">%s</text>' % (47 + 17 * len(sub) + 17 * i, esc(ln)))
    s.append(lsvg)
    s.append('<g transform="translate(0,%d)">' % head)
    s.append(body)
    s.append("</g>")
    for i, c in enumerate(caption):
        s.append('<text x="24" y="%d" font-size="11.5" class="m">%s</text>' % (cap_y + 16 * i, esc(c)))
    s.append("</svg>\n")
    with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(s))


def logticks(lo, hi):
    return [10.0 ** k for k in range(int(round(math.log10(lo))), int(round(math.log10(hi))) + 1)]


IDLE = "idle machine"
BACK = {"jolt": "jolt", "physx": "physx", "physx-maximal": "pmax"}
SIZES = ["h76m", "h85m", "h96m", "h182m"]

# ---------------------------------------------------------------- load all result files
limb_files = []  # (limb, size, case, hz, short, json)
for p in sorted(glob.glob(P("chain-limb", "results", "limb", "*.json"))):
    j = load(p)
    m = re.match(r"limb-(leg|arm)-(h\d+m)-(.+)-(\d+)hz(-shortton)?$", j["tag"])
    limb_files.append((m.group(1), m.group(2), m.group(3), int(m.group(4)), bool(m.group(5)), j))
chain_files = [load(p) for p in sorted(glob.glob(P("chain-limb", "results", "*.json")))]
chain_matrix = [j for j in chain_files if not j["tag"].startswith("diag-")]
follow = {os.path.basename(p)[:-5]: load(p) for p in sorted(glob.glob(P("chain-limb", "results", "followup", "*.json")))}
stand_files = [(os.path.basename(p), load(p)) for p in sorted(glob.glob(P("standing-foot", "results", "stand-*.json")))]
ladder_files = [(os.path.basename(p), load(p)) for p in sorted(glob.glob(P("standing-foot", "results", "ladder-*.json")))]
hip_files = [load(p) for p in sorted(glob.glob(P("hip-twist", "results", "*.json")))]
fb_files = [load(p) for p in sorted(glob.glob(P("full-body", "results", "*.json")))]
fb_remedy = [(os.path.basename(p), load(p)) for p in sorted(glob.glob(P("full-body", "results", "jolt-remedy", "*.json")))]
print("files: limb %d, chain %d (matrix %d), followup %d, stand %d, ladder %d, hip %d, full-body %d, remedy %d" % (
    len(limb_files), len(chain_files), len(chain_matrix), len(follow), len(stand_files), len(ladder_files), len(hip_files), len(fb_files), len(fb_remedy)))

CAP_BASE = "One skeleton, boxes only, float32, CPU, one machine; Jolt 5.7.0 tag v5.7.0; PhysX 5.13.0 built from a branch head, not a tag."
THR = "Thresholds (0.01 m, 0.1 m, 0.1 rad) are harness choices, not standards."


# ================================================================ 1. limb anchor error
def chart_limb_anchor():
    cells = {}
    for limb, size, case, hz, short, j in limb_files:
        if short:
            continue
        for r in j["results"]:
            key = (limb, size, hz, r["backend_key"])
            c = cells.setdefault(key, dict(v=0.0, files=0, runs=0))
            c["files"] += 1
            for x in r["runs"]:
                c["v"] = max(c["v"], x["joint_pos_err_max_m"])
                c["runs"] += 1
    rows, trows = [], []
    nfiles = sum(1 for f in limb_files if not f[4])
    for limb in ("leg", "arm"):
        for size in SIZES:
            for hz in (120, 200):
                jv, pv = cells[(limb, size, hz, "jolt")], cells[(limb, size, hz, "physx")]
                rows.append(("%s %s %d Hz" % (limb, size, hz), [("jolt", jv["v"], ""), ("physx", pv["v"], "")]))
                trows.append((limb, size, hz, "%.3g" % jv["v"], "%.3g" % pv["v"], jv["files"], jv["runs"]))
    allj = [r[1][0][1] for r in rows]
    allp = [r[1][1][1] for r in rows]
    sc = ("log", 1e-6, 1e0, logticks(1e-6, 1e0))
    body, h = panel(24, 0, 800, 120, rows, sc, flags=[(0.01, "0.01 m onset"), (0.1, "0.1 m flag")], ax_label="worst joint anchor error over the run (m, log axis)",
                    valfmt=lambda v: "%.2g" % v)
    cap = ["%d limb files (leg and arm, 4 body sizes, 2 step rates, cases hold, swing, wall, impact, worst-axis hold; short-ton h85m files left out), 2 runs each; each bar is the worst over 5 cases and 2 runs." % nfiles,
           "Anchor error is a pose-based metric, independent of machine load. " + THR,
           CAP_BASE]
    svg_doc("limb-anchor-error.svg", 860, "Limb chains: worst joint anchor error per backend", "Leg and arm chains derived from the skeleton, hold, swing, wall, impact and worst-axis hold scenes",
            [("bar", "jolt", "Jolt"), ("bar", "physx", "PhysX articulation")], body, h, cap,
            "Bar chart on a log axis of the worst joint anchor error in metres for Jolt and PhysX articulation on 16 limb configurations. Jolt ranges %.2g to %.2g m, PhysX %.2g to %.2g m." % (min(allj), max(allj), min(allp), max(allp)),
            "Lower is better (anchor error in m: smaller means the joint stays closer to closed). Green = the lower bar of each row (no percent of best: log-scale errors).")
    DATA.append(("limb-anchor-error.svg", "Direction: lower is better; best = lower value per row (limb, size, rate); no percent of best (log-scale error). Worst joint anchor error (m) over the 5 cases and 2 runs per cell; source `chain-limb/results/limb/*.json` (80 of 96 files, short-ton files excluded). Jolt range %.3g to %.3g m, PhysX %.3g to %.3g m." % (min(allj), max(allj), min(allp), max(allp)),
                 table(["limb", "size", "Hz", "Jolt worst m", "PhysX articulation worst m", "files per backend", "runs per backend"], trows)))


# ================================================================ 2. alternating chain onset
REM = [("jolt-base", "Jolt baseline", "jolt"), ("jolt-prio", "Jolt, constraint priority", "jolt"), ("jolt-heavy1", "Jolt, heavier body as body 1", "jolt"),
       ("jolt-imult10", "Jolt, inertia x10 on light links", "jolt"), ("jolt-imult100", "Jolt, inertia x100 on light links", "jolt"),
       ("jolt-thick", "Jolt, thicker light box", "jolt"), ("jolt-maxw1000", "Jolt, max angular velocity 1000", "jolt"),
       ("jolt-combo-thick-prio-heavy1", "Jolt, priority + heavy 1 + thick box", "jolt"), ("physx-artic", "PhysX articulation", "physx"),
       ("physx-maximal", "PhysX maximal (PGS 4/1)", "pmax"), ("physx-maximal-tgs", "PhysX maximal (TGS)", "pmax"),
       ("physx-maximal-thick", "PhysX maximal, thicker light box", "pmax"), ("physx-maximal-it10-4", "PhysX maximal, iterations 10/4", "pmax")]


def chart_onset():
    panels, trows = [], []
    info = {}
    for hz in (120, 200):
        rows = []
        for stem, lab, key in REM:
            j = follow["%s-z-%dhz" % (stem, hz)]
            sc = j["scenario"]
            assert sc["step_hz"] == hz and sc["tip_speed_mps"] == 0.1 and sc["profile"] == "alternating" and sc["hinge_axis"] == "z"
            x = j["results"][0]["runs"][0]
            nf = x["onset_nonfinite"]["step"] if x["onset_nonfinite"]["episode"] else None
            e1 = x["onset_err_gt_1m"]["step"] if x["onset_err_gt_1m"]["episode"] else None
            e01 = x["onset_err_gt_0p01m"]["step"] if x["onset_err_gt_0p01m"]["episode"] else None
            ep = int(round(sc["episode_sec"] * hz))
            info[hz] = ep
            rows.append((lab, [(key, nf, "")], e1))
            trows.append((lab, hz, e01 if e01 else "never", e1 if e1 else "never", nf if nf else "never", "%.3g" % x["joint_pos_err_max_m"], ep))
        panels.append((hz, rows))
    out, ht = [], 0
    for idx, (hz, rows) in enumerate(panels):
        ep = info[hz]
        PW = 200
        x0 = 24 if idx == 0 else 24 + 262 + 200 + 30
        lw = 262 if idx == 0 else 10
        w = lw + PW + 40
        sc = ("lin", 0, ep, [t for t in range(0, ep + 1, 100 if ep > 300 else 80)])
        prow = [(r[0], r[1], "all") for r in rows]
        body, h = panel(x0, 0, w, lw, prow, sc, title="%d Hz (episode = %d steps)" % (hz, ep), bar_h=13, gap=0, labels=(idx == 0), valfmt=lambda v: "%d" % v,
                        none_text="never", ax_label="step index within the 2 s episode", pad_r=40, better="hi", none_best=True)
        out.append(body)
        # ring marker at first joint error above 1 m
        top = 18 + 0
        pitch = 13 + 7
        for i, r in enumerate(rows):
            if r[2]:
                xx = x0 + lw + (r[2] / ep) * PW
                out.append('<circle cx="%.1f" cy="%.1f" r="4" class="ring"/>' % (xx, top + i * pitch + 6.5))
        ht = max(ht, h)
    body = "\n".join(out)
    nj = sum(1 for r in trows if r[0].startswith("Jolt") and r[4] != "never")
    cap = ["Alternating 50 t / 50 kg chain, 12 links, hinge axis parallel to gravity, tip speed 0.1 m/s, CCD on, 1 thread, 1 run per case, 2 episodes of 2 s; onset steps are in episode 1. Jolt went non-finite in %d of 16 Jolt cases." % nj,
           "Onset steps do not depend on machine load (these runs were made while the machine was busy; no timing is used). Ring = first step with anchor error above 1 m.",
           "A missing bar means no non-finite state in the run; PhysX maximal lost closure (ring) without a non-finite state. " + CAP_BASE]
    svg_doc("alternating-chain-onset.svg", 860, "Alternating-mass chain: step of the first non-finite state", "Jolt remedies versus PhysX articulation and PhysX maximal control; bar = first non-finite state, ring = first anchor error above 1 m",
            [("bar", "jolt", "Jolt"), ("bar", "physx", "PhysX articulation"), ("bar", "pmax", "PhysX maximal (control)"), ("ring", None, "anchor error above 1 m")], body, ht, cap,
            "Onset charts for 13 configurations at 120 and 200 Hz. Jolt reaches a non-finite state in %d of 16 cases, between step %d and step %d of the episode; PhysX articulation reaches none." % (
                nj, min(r[4] for r in trows if r[0].startswith("Jolt") and r[4] != "never"), max(r[4] for r in trows if r[0].startswith("Jolt") and r[4] != "never")),
            "Higher (later) is better: a later non-finite step is better, and no non-finite state at all (never) is best. Green = best in each Hz panel (every case with never, else the latest step).")
    DATA.append(("alternating-chain-onset.svg", "Direction: later (higher step) is better and `never` is best; best = `never` if any case in the Hz panel never goes non-finite, else the latest step; no percent of best (step index, best can be `never`). First step (inside the episode) with anchor error above 0.01 m, above 1 m, and with a non-finite state, `never` = not reached; source `chain-limb/results/followup/<variant>-z-<Hz>hz.json` (1 run each, busy machine, onset steps only).",
                 table(["case", "Hz", "first step > 0.01 m", "first step > 1 m", "first non-finite step", "max anchor error m", "steps per episode"], trows)))


# ================================================================ 3. standing foot
def chart_standing():
    cells = []  # dict(be, sweep, up, trig, nf)
    for fn, j in stand_files:
        sweep = "100/30" if "iters100-30" in fn else "default"
        for c in j["cases"]:
            r = c["runs"]
            a = r[0]

            def mx(k):
                v = [x[k] for x in r]
                return None if any(t is None for t in v) else max(v)
            an, ax = mx("anchor_err_max_m"), mx("axis_err_max_rad")
            trig = an is None or an > 0.1 or ax is None or ax > 0.1
            cells.append(dict(be=c["backend_key"], sweep=sweep, up=bool(a["stays_upright"]), trig=trig, nf=bool(a["nonfinite"]), stance=c["stance"], case=c["case"],
                              size=j["size_label"], hz=j["step_hz"]))
    cfg = [("jolt", "default", "Jolt, default iterations (10/2)"), ("jolt", "100/30", "Jolt, 100/30 iterations"), ("physx", "default", "PhysX articulation, default"),
           ("physx", "100/30", "PhysX articulation, 100/30"), ("physx-maximal", "default", "PhysX maximal + D6, default"), ("physx-maximal", "100/30", "PhysX maximal + D6, 100/30")]
    A, B, trows = [], [], []
    for be, sw, lab in cfg:
        sel = [c for c in cells if c["be"] == be and c["sweep"] == sw]
        up = sum(c["up"] for c in sel)
        tr = sum(c["trig"] for c in sel)
        A.append((lab, [(BACK[be], up, "")]))
        B.append((lab, [(BACK[be], tr, "")]))
        trows.append((lab, len(sel), up, tr, sum(c["nf"] for c in sel)))
    # ladder
    lad = {}
    for fn, j in ladder_files:
        var = fn[:-5].split("hz-")[1]
        for c in j["cases"]:
            a = c["runs"][0]
            lad.setdefault(var, []).append(a["stays_upright"])
    for sw, name in (("default", "default"), ("100/30", "it100-30")):
        lad[name] = [c["up"] for c in cells if c["be"] == "jolt" and c["sweep"] == sw and (c["stance"] + "-" + c["case"]) in ("2leg-stand", "1leg-swing")]
    lorder = [("default", "default (10/2)"), ("it20-4", "20/4"), ("it30-10", "30/10"), ("it50-15", "50/15"), ("it100-30", "100/30"), ("kp4", "all gains x4, default iterations"), ("kp4-it50-15", "all gains x4, 50/15")]
    C, lrows = [], []
    for var, lab in lorder:
        L = lad[var]
        C.append((lab, [("jolt", sum(1 for v in L if v), "")]))
        lrows.append((lab, len(L), sum(1 for v in L if v)))
    out, y = [], 0
    for title, rows, mx_, ax in (("Cases where the stance stays upright (of 40 per row; higher is better)", A, 40, "cases (4 sizes x 2 rates x 5 scenes, 2 runs each)"),
                                 ("Cases where the flag fires: anchor > 0.1 m, axis > 0.1 rad or non-finite (of 40; lower is better)", B, 40, "cases"),
                                 ("Jolt iteration ladder: runs that stay upright (of 16; higher is better)", C, 16, "runs (scenes 2leg-stand, 1leg-swing; 4 sizes, 2 rates; velocity/position iterations)")):
        body, h = panel(24, y, 800, 250, rows, ("lin", 0, mx_, [0, mx_ // 4, mx_ // 2, 3 * mx_ // 4, mx_]), title=title, valfmt=lambda v: "%d" % v, ax_label=ax, bar_h=12, gap=0,
                        better="lo" if rows is B else "hi", pct=None if rows is B else "hi", group=lambda i, r: 0)
        out.append(body)
        y += h + 10
    cap = ["240 backend cases (4 sizes x 2 rates x 5 scenes x 3 backends x 2 solver settings), 2 runs each; a case counts by its worst run. 'Stays upright' = tilt <= 0.1 rad, end sag < 10 % of pelvis height, last step reached.",
           "Counts do not depend on machine load (timings of this family are invalid and unused). The 2 t impact is a very small disturbance for these pelvis masses. " + THR,
           CAP_BASE]
    svg_doc("standing-foot.svg", 860, "Standing on a ground box: stance and flag counts per backend and solver setting", "Two-leg and one-leg stance, impact and one-leg swing; library-default versus raised solver iterations",
            [("bar", "jolt", "Jolt"), ("bar", "physx", "PhysX articulation"), ("bar", "pmax", "PhysX maximal + D6 (control)")], "\n".join(out), y, cap,
            "Three bar panels. Jolt with default iterations stays upright in %d of 40 cases and with 100/30 iterations in %d of 40; PhysX articulation in %d of 40 in both settings." % (trows[0][2], trows[1][2], trows[2][2]),
            "Direction differs by panel: upright counts (first and third panel) higher is better; flag-fires count (second panel) lower is better. Green = best bar of its panel (ties all green).")
    DATA.append(("standing-foot.svg", "Direction: first and third panel (upright) higher is better, label percent = value / best; second panel (flag fires) lower is better, no percent because the best can be 0. Best = best row of the panel, ties all green. Counts over cases (worst of 2 runs); source `standing-foot/results/stand-*.json` and `ladder-*.json`. Ladder rows 'default' and '100/30' are taken from the main matrix as in `standing-foot/summarize.py`.",
                 table(["backend, setting", "cases", "stays upright", "flag fires", "non-finite"], trows) + "\nJolt iteration ladder:\n\n" + table(["variant", "runs", "stays upright"], lrows)))


# ================================================================ 4. hip twist
HV = [("jolt-swingtwist", "jolt", "Jolt swing-twist"), ("jolt-sixdof", "jolt2", "Jolt six-DOF"), ("physx-art-spherical", "physx", "PhysX articulation spherical"), ("physx-d6", "pmax", "PhysX D6 (maximal)")]
HCASES = [("hold", "hold"), ("swing", "swing (60 deg twist)"), ("wall", "wall (twist into its limit)"), ("impact", "impact (2 t)"), ("limits", "limits (drives beyond limits)")]


def hip_runs():
    runs = {}
    for j in hip_files:
        sc = j["scenario"]
        for v in j["variants"]:
            for r in v["runs"]:
                runs.setdefault((sc["case"], v["variant"]), []).append(r)
    return runs


def chart_hip():
    runs = hip_runs()
    rows, trows = [], []
    tot = {}
    for cs, lab in HCASES:
        bars = []
        for vk, key, vl in HV:
            L = runs[(cs, vk)]
            n = sum(1 for r in L if r["anchor_err_max_m"] is None or r["anchor_err_max_m"] > 0.1)
            bars.append((key, n, ""))
            tot[vk] = tot.get(vk, 0) + n
            trows.append((lab, vl, len(L), n))
        rows.append((lab, bars))
    nrun = len(runs[("hold", "jolt-swingtwist")])
    body, h = panel(24, 0, 800, 190, rows, ("lin", 0, 32, [0, 8, 16, 24, 32]), valfmt=lambda v: "%d" % v, ax_label="runs with joint anchor error above 0.1 m (of %d per case and variant)" % nrun, bar_h=11, gap=1)
    cap = ["80 files (4 sizes x leg/arm x 5 cases x 2 rates), 4 joint variants, 2 runs each: %d runs per case and variant, 160 per variant over the 5 cases (above 0.1 m in all cases: Jolt swing-twist %d, Jolt six-DOF %d, PhysX articulation %d, PhysX D6 %d)." % (
        nrun, tot["jolt-swingtwist"], tot["jolt-sixdof"], tot["physx-art-spherical"], tot["physx-d6"]),
           "Counts do not depend on machine load (all timings of this family are invalid and unused). The libraries do not enforce the same swing limit. " + THR, CAP_BASE]
    svg_doc("hip-twist-anchor.svg", 860, "3-DOF ball joint: runs with anchor error above 0.1 m", "Hip/shoulder swing-twist joint with hinge chains, four joint variants",
            [("bar", "jolt", "Jolt swing-twist"), ("bar", "jolt2", "Jolt six-DOF"), ("bar", "physx", "PhysX articulation spherical"), ("bar", "pmax", "PhysX D6 (maximal)")], body, h, cap,
            "Grouped bars: runs of 32 per case with anchor error above 0.1 m for four joint variants over five cases. Totals over the five cases: Jolt swing-twist %d, Jolt six-DOF %d, PhysX articulation %d, PhysX D6 %d." % (tot["jolt-swingtwist"], tot["jolt-sixdof"], tot["physx-art-spherical"], tot["physx-d6"]),
            "Lower is better (fewer runs with anchor error above 0.1 m). Green = the lowest count of each case, ties all green (no percent of best: the best is often 0).")
    DATA.append(("hip-twist-anchor.svg", "Direction: lower is better; best = lowest count per case (ties all green, so a case where all four variants have 0 is all green); no percent of best (best is often 0). Runs with anchor error above 0.1 m (or null); source `hip-twist/results/*.json` (80 files; every file holds the 4 variants with 2 runs each).",
                 table(["case", "variant", "runs", "runs with anchor > 0.1 m"], trows)))


# ================================================================ 5-7. full body
FBC = [("stand", "stand"), ("step", "step"), ("step-debris100", "step+debris 100:1"), ("step-debris1000", "step+debris 1000:1"), ("impact", "impact (2 t)"), ("wall", "wall (arm 30 m/s)")]


def fb_cells():
    out = {}  # (case, hz, size, backend) -> run 1 dict
    for j in fb_files:
        sc = j["scenario"]
        case = sc["case"]
        if case == "step-debris":
            case = "step-debris%d" % round(sc["mass_ratio_ref_to_debris"])
        for r in j["results"]:
            out[(case, round(sc["step_hz"]), sc["size_label"], r["backend_key"])] = r["runs"][0]
    return out


def fb_grid(name, metric, title, subtitle, lo, hi, flag, flagtxt, axlab, descr, direction):
    cells = fb_cells()
    out, trows = [], []
    ph = 0
    pw = 440
    for idx, size in enumerate(SIZES):
        rows = []
        for cs, lab in FBC:
            for hz in (120, 200):
                jv = cells[(cs, hz, size, "jolt")][metric]
                pv = cells[(cs, hz, size, "physx")][metric]
                rows.append(("%s %d Hz" % (lab, hz), [("jolt", jv, ""), ("physx", pv, "")]))
                trows.append((size, lab, hz, "%.3g" % jv, "%.3g" % pv))
        x = 24 + (idx % 2) * pw
        y = (idx // 2) * (ph or 0)
        body, h = panel(x, y, pw - 10, 170, rows, ("log", lo, hi, logticks(lo, hi)), title=size, flags=[(flag, flagtxt)], bar_h=8, gap=1, show_val=lambda v: v > flag, valfmt=lambda v: "%.2g" % v,
                        ax_label=axlab, pad_r=44)
        out.append(body)
        ph = h + 14
        if idx == 1:
            # second row starts below first row
            pass
    # recompute: place panels with a fixed second-row offset
    out = []
    for idx, size in enumerate(SIZES):
        rows = []
        for cs, lab in FBC:
            for hz in (120, 200):
                rows.append(("%s %d Hz" % (lab, hz), [("jolt", cells[(cs, hz, size, "jolt")][metric], ""), ("physx", cells[(cs, hz, size, "physx")][metric], "")]))
        x = 24 + (idx % 2) * pw
        y = (idx // 2) * ph
        body, h = panel(x, y, pw - 10, 170, rows, ("log", lo, hi, logticks(lo, hi)), title=size, flags=[(flag, flagtxt)], bar_h=8, gap=1, show_val=lambda v: v > flag, valfmt=lambda v: "%.2g" % v,
                        ax_label=axlab, pad_r=44)
        out.append(body)
    pv_all = [cells[k][metric] for k in cells if k[3] == "physx"]
    jv_all = [cells[k][metric] for k in cells if k[3] == "jolt"]
    nj = sum(1 for v in jv_all if v > flag)
    npx = sum(1 for v in pv_all if v > flag)
    cap = ["48 files (6 scenes x 4 sizes x 2 rates), run 1 of 2 per backend; labelled values are above the flag level (%g). Of %d Jolt cells %d are above it, of %d PhysX cells %d." % (flag, len(jv_all), nj, len(pv_all), npx),
           "PhysX range %.2g to %.2g; Jolt range %.2g to %.2g. The scenes use library-default solver iterations; no machine-load dependence (timings unused). " % (min(pv_all), max(pv_all), min(jv_all), max(jv_all)) + THR,
           CAP_BASE]
    svg_doc(name, 912, title, subtitle, [("bar", "jolt", "Jolt (bodies and hinge constraints)"), ("bar", "physx", "PhysX articulation")], "\n".join(out), 2 * ph - 14, cap, descr.format(jmax=max(jv_all), pmin=min(pv_all), pmax=max(pv_all)), direction)
    return trows, (min(jv_all), max(jv_all), min(pv_all), max(pv_all), nj, npx)


def chart_fb_anchor():
    t, s = fb_grid("full-body-anchor.svg", "anchor_err_max_m", "Whole humanoid (20 bodies, 19 joints): worst joint anchor error", "Per body size, scene and step rate; log axis",
                   1e-5, 1e0, 0.1, "0.1 m flag", "worst joint anchor error (m, log axis)",
                   "Four small-multiple panels, one per body size, of worst joint anchor error in metres for Jolt and PhysX articulation over six scenes at 120 and 200 Hz. PhysX values are {pmin:.2g} to {pmax:.2g} m; Jolt reaches {jmax:.3g} m.", "Lower is better (anchor error in m). Green = the lower bar of each row (no percent of best: log-scale errors).")
    DATA.append(("full-body-anchor.svg", "Direction: lower is better; best = lower value per row (size, scene, rate); no percent of best (log-scale error). Worst joint anchor error (m), run 1 of 2; source `full-body/results/<scene>-<size>-<Hz>hz.json` (48 files).", table(["size", "scene", "Hz", "Jolt m", "PhysX articulation m"], t)))


def chart_fb_axis():
    t, s = fb_grid("full-body-axis.svg", "axis_err_max_rad", "Whole humanoid: worst hinge axis error", "Per body size, scene and step rate; log axis",
                   1e-4, 1e1, 0.1, "0.1 rad flag", "worst hinge axis error (rad, log axis)",
                   "Four small-multiple panels, one per body size, of worst hinge axis error in radians for Jolt and PhysX articulation over six scenes at 120 and 200 Hz. PhysX values are {pmin:.2g} to {pmax:.2g} rad; Jolt reaches {jmax:.3g} rad.", "Lower is better (axis error in rad). Green = the lower bar of each row (no percent of best: log-scale errors).")
    DATA.append(("full-body-axis.svg", "Direction: lower is better; best = lower value per row (size, scene, rate); no percent of best (log-scale error). Worst hinge axis error (rad), run 1 of 2; source `full-body/results/<scene>-<size>-<Hz>hz.json` (48 files).", table(["size", "scene", "Hz", "Jolt rad", "PhysX articulation rad"], t)))


def chart_fb_tilt():
    cells = fb_cells()
    rows, trows = [], []
    for cs, lab in FBC:
        for hz in (120, 200):
            jv = max(cells[(cs, hz, s, "jolt")]["torso_tilt_max_rad"] for s in SIZES)
            pv = max(cells[(cs, hz, s, "physx")]["torso_tilt_max_rad"] for s in SIZES)
            rows.append(("%s %d Hz" % (lab, hz), [("jolt", jv, ""), ("physx", pv, "")]))
            trows.append((lab, hz, "%.3g" % jv, "%.3g" % pv))
    mx = max(max(r[1][0][1], r[1][1][1]) for r in rows)
    hi = math.ceil(mx * 2) / 2
    body, h = panel(24, 0, 800, 190, rows, ("lin", 0, hi, [0, 0.5, 1.0, 1.5, 2.0][: int(hi / 0.5) + 1]), valfmt=lambda v: "%.2g" % v, ax_label="largest torso tilt during the run (rad), worst of 4 sizes", bar_h=11, gap=1)
    pst = [float(r[3]) for r in trows if r[0].startswith("step")]
    cap = ["Worst of the 4 body sizes per bar; run 1 of 2; 48 files. The scripted step has no balance controller, so the PhysX torso tilts %.2g to %.2g rad in the three step scenes (open-loop script)." % (min(pst), max(pst)),
           "Tilt is pose-based and independent of machine load. " + CAP_BASE]
    svg_doc("full-body-tilt.svg", 860, "Whole humanoid: largest torso tilt", "Worst over the four body sizes, per scene and step rate",
            [("bar", "jolt", "Jolt"), ("bar", "physx", "PhysX articulation")], body, h, cap,
            "Grouped bars of the largest torso tilt in radians for Jolt and PhysX articulation over six scenes at 120 and 200 Hz.",
            "Lower is better (a smaller tilt means the torso stays closer to upright). Green = the lower bar of each row (no percent of best: tilt is an angle).")
    DATA.append(("full-body-tilt.svg", "Direction: lower is better; best = lower value per row (scene, rate); no percent of best (angle). Largest torso tilt (rad), max over the 4 sizes, run 1; source `full-body/results/*.json`.", table(["scene", "Hz", "Jolt rad", "PhysX articulation rad"], trows)))


# ================================================================ 8. idle step time
def chart_timing():
    def stats(x):
        return x["physics_step_in_play"]["p50_ms"], x["physics_step_in_play"]["p99_ms"]
    chain_rows = []
    trows = []
    for j in chain_matrix:
        assert j["timing_note"].startswith(IDLE), j["tag"]
        sc = j["scenario"]
        lab = j["tag"].replace("-t1", " 1 thread").replace("-t4", " 4 threads")
        b = {}
        for r in j["results"]:
            p50, p99 = stats(r["runs"][0])
            b[r["backend_key"]] = (p50, p99)
        chain_rows.append((lab, b))
        trows.append(("chain matrix", lab, "%.4g" % b["jolt"][0], "%.4g" % b["physx"][0], "%.4g" % b["jolt"][1], "%.4g" % b["physx"][1]))
    limb_vals = {"jolt": [], "physx": []}
    for limb, size, case, hz, short, j in limb_files:
        assert j["timing_note"].startswith(IDLE), j["tag"]
        for r in j["results"]:
            limb_vals[r["backend_key"]].append(stats(r["runs"][0]))
    out = []
    hi_c = max(max(v[0] for v in b.values()) for _, b in chain_rows)
    hi_p = max(max(v[1] for v in b.values()) for _, b in chain_rows)

    def nice(v):
        e = 10 ** math.floor(math.log10(v))
        for m in (1, 2, 2.5, 5, 10):
            if m * e >= v:
                return m * e
    hc, hp = nice(hi_c), nice(hi_p)
    r50 = [(l, [("jolt", b["jolt"][0], ""), ("physx", b["physx"][0], "")]) for l, b in chain_rows]
    r99 = [(l, [("jolt", b["jolt"][1], ""), ("physx", b["physx"][1], "")]) for l, b in chain_rows]
    b1, h1 = panel(24, 0, 440, 252, r50, ("lin", 0, hc, [0, hc / 2, hc]), title="Chain matrix: median step (ms)", valfmt=lambda v: "%.3f" % v, ax_label="ms per physics step", bar_h=9, gap=1, pad_r=50, pct="lo")
    b2, h2 = panel(490, 0, 340, 18, r99, ("lin", 0, hp, [0, hp / 2, hp]), title="Chain matrix: p99 step (ms)", valfmt=lambda v: "%.3f" % v, ax_label="ms per physics step", labels=False, bar_h=9, gap=1, pad_r=50, pct="lo")
    out += [b1, b2]
    nl = len(limb_vals["jolt"])
    q = {}
    for k in limb_vals:
        q[k] = dict(med50=statistics.median(v[0] for v in limb_vals[k]), max50=max(v[0] for v in limb_vals[k]), med99=statistics.median(v[1] for v in limb_vals[k]), max99=max(v[1] for v in limb_vals[k]))
    lrows = []
    for lab, f in (("median of the per-file median", "med50"), ("worst per-file median", "max50"), ("median of the per-file p99", "med99"), ("worst per-file p99", "max99")):
        lrows.append((lab, [("jolt", q["jolt"][f], ""), ("physx", q["physx"][f], "")]))
        trows.append(("limb matrix (%d files)" % nl, lab, "%.4g" % q["jolt"][f], "%.4g" % q["physx"][f], "", ""))
    hl = nice(max(r[1][1][1] for r in lrows))
    y = h1 + 18
    b3, h3 = panel(24, y, 800, 230, lrows, ("lin", 0, hl, [0, hl / 2, hl]), title="Limb matrix (%d files, 13-body limb scenes): per-file step time (ms)" % nl, valfmt=lambda v: "%.4f" % v, ax_label="ms per physics step", bar_h=11, gap=1, pct="lo")
    out.append(b3)
    cap = ["Idle-machine files only (timing_note 'idle machine, other applications closed, 1 session'): 12 chain-matrix cases (run 1 per backend) and %d limb files (run 1 per backend, 1 thread unless the label says 4)." % nl,
           "Physics update call only, after warm-up; one machine, one session, desktop CPU class, no other hardware. Step times of the other families were measured on a busy machine and are not shown.",
           "Small scenes (13 bodies); this is not a performance ranking of the libraries. " + CAP_BASE]
    svg_doc("idle-step-time.svg", 860, "Idle-machine physics step time of the small articulated scenes", "Median and p99 of the physics update call",
            [("bar", "jolt", "Jolt"), ("bar", "physx", "PhysX articulation")], "\n".join(out), y + h3, cap,
            "Bar charts of idle-machine step time: chain matrix median and p99 for 12 cases, and a summary over 96 limb files. Largest chain-matrix p99: Jolt %.3g ms, PhysX %.3g ms." % (max(b["jolt"][1] for _, b in chain_rows), max(b["physx"][1] for _, b in chain_rows)),
            "Lower is better (step time in ms: lower is faster). Green = the faster bar of each row; the percentage is best / value, so the best is 100%.")
    DATA.append(("idle-step-time.svg", "Direction: lower is better; best = lower step time per row; label percent = best / value (best = 100%). Step time of `physics_step_in_play` (ms), run 1 of each backend, idle-machine files only; sources `chain-limb/results/*.json` (non diag) and `chain-limb/results/limb/*.json`.",
                 table(["group", "case or statistic", "Jolt p50 / statistic (ms)", "PhysX p50 / statistic (ms)", "Jolt p99 (ms)", "PhysX p99 (ms)"], trows)))


# ================================================================ 9. pass fraction
def chart_summary():
    def bad(x, an="joint_pos_err_max_m", ax="joint_axis_err_max_rad", nf=None):
        a, b = x[an], x[ax]
        return a is None or a > 0.1 or b is None or b > 0.1 or (nf(x) if nf else False)
    rows_src = []  # (label, [(series key, passed, total)])

    def cell_bad_chain(runs):
        return any(bad(x, nf=lambda x: x["nan"] or x["episodes_nan"] > 0) for x in runs)
    ch = {"jolt": [0, 0], "physx": [0, 0]}
    for j in chain_matrix:
        for r in j["results"]:
            ch[r["backend_key"]][1] += 1
            ch[r["backend_key"]][0] += 0 if cell_bad_chain(r["runs"]) else 1
    lb = {"jolt": [0, 0], "physx": [0, 0]}
    for limb, size, case, hz, short, j in limb_files:
        for r in j["results"]:
            lb[r["backend_key"]][1] += 1
            lb[r["backend_key"]][0] += 0 if cell_bad_chain(r["runs"]) else 1
    rows_src.append(("chain matrix (%d cases)" % ch["jolt"][1], [("jolt", *ch["jolt"]), ("physx", *ch["physx"])]))
    rows_src.append(("limb matrix (%d cases)" % lb["jolt"][1], [("jolt", *lb["jolt"]), ("physx", *lb["physx"])]))
    sf = {}
    for fn, j in stand_files:
        sweep = "100/30" if "iters100-30" in fn else "default"
        for c in j["cases"]:
            r = c["runs"]
            nfl = any(x["nonfinite"] for x in r)
            an = None if any(x["anchor_err_max_m"] is None for x in r) else max(x["anchor_err_max_m"] for x in r)
            ax = None if any(x["axis_err_max_rad"] is None for x in r) else max(x["axis_err_max_rad"] for x in r)
            ok = not nfl and an is not None and an <= 0.1 and ax is not None and ax <= 0.1
            t = sf.setdefault((sweep, c["backend_key"]), [0, 0])
            t[1] += 1
            t[0] += 1 if ok else 0
    for sw, lab in (("default", "standing, default iterations"), ("100/30", "standing, 100/30 iterations")):
        rows_src.append(("%s (%d cases)" % (lab, sf[(sw, "jolt")][1]), [("jolt", *sf[(sw, "jolt")]), ("physx", *sf[(sw, "physx")]), ("pmax", *sf[(sw, "physx-maximal")])]))
    hp = {}
    for j in hip_files:
        for v in j["variants"]:
            ok = not any(r["nan"] or r["onset_nonfinite_step"] > 0 or r["anchor_err_max_m"] is None or r["anchor_err_max_m"] > 0.1 or r["hinge_axis_err_max_rad"] is None or r["hinge_axis_err_max_rad"] > 0.1 for r in v["runs"])
            t = hp.setdefault(v["variant"], [0, 0])
            t[1] += 1
            t[0] += 1 if ok else 0
    n_hip = hp["jolt-swingtwist"][1]
    rows_src.append(("hip-twist, Jolt swing-twist (%d cases)" % n_hip, [("jolt", *hp["jolt-swingtwist"])]))
    rows_src.append(("hip-twist, Jolt six-DOF (%d)" % n_hip, [("jolt2", *hp["jolt-sixdof"])]))
    rows_src.append(("hip-twist, PhysX articulation / D6 (%d each)" % n_hip, [("physx", *hp["physx-art-spherical"]), ("pmax", *hp["physx-d6"])]))
    fb = {}
    for j in fb_files:
        for r in j["results"]:
            x = r["runs"]
            ok = not any(a["onset_step_nonfinite"] or a["anchor_err_max_m"] > 0.1 or a["axis_err_max_rad"] > 0.1 for a in x)
            t = fb.setdefault(r["backend_key"], [0, 0])
            t[1] += 1
            t[0] += 1 if ok else 0
    rows_src.append(("full body, default iterations (%d cases)" % fb["jolt"][1], [("jolt", *fb["jolt"]), ("physx", *fb["physx"])]))
    rem = {}
    for fn, j in fb_remedy:
        sc = j["scenario"]
        k = "480 Hz, default iterations" if round(sc["step_hz"]) == 480 else "100/30 iterations (120 and 200 Hz)"
        for r in j["results"]:
            ok = not any(a["onset_step_nonfinite"] or a["anchor_err_max_m"] > 0.1 or a["axis_err_max_rad"] > 0.1 for a in r["runs"])
            t = rem.setdefault(k, [0, 0])
            t[1] += 1
            t[0] += 1 if ok else 0
    for k, v in sorted(rem.items(), reverse=True):
        rows_src.append(("full body, Jolt %s (%d; stand, step+debris)" % ("480 Hz" if k.startswith("480") else "100/30 its", v[1]), [("jolt", *v)]))
    rows = [(lab, [(k, 100.0 * p / n, "  %d/%d" % (p, n)) for k, p, n in bars], "hip" if lab.startswith("hip-twist") else lab) for lab, bars in rows_src]
    body, h = panel(24, 0, 840, 350, rows, ("lin", 0, 100, [0, 25, 50, 75, 100]), valfmt=lambda v: "%.0f %%" % v, ax_label="share of cases within the flag levels (higher is better)", bar_h=11, gap=1, pad_r=150, better="hi")
    trows = [(lab, SERIES[k][1], p, n, "%.1f" % (100.0 * p / n)) for lab, bars in rows_src for k, p, n in bars]
    cap = ["A case passes when no run of it has a non-finite state, anchor error above 0.1 m or axis error above 0.1 rad (the README flag levels); each family uses its own scenes, so rows are not comparable with each other.",
           "Case = one result file and backend (worst of its runs); the hip-twist rows and the full-body Jolt remedy rows cover only the cases they ran; chain rows include the alternating-mass chain.",
           "Tunnelling, determinism and limit-excess checks are not part of this pass rule. No machine-load dependence. " + THR, CAP_BASE]
    svg_doc("pass-fraction.svg", 900, "Share of cases within the harness flag levels, per test family and setting", "Summary of the five families; one harness rule for all rows",
            [("bar", "jolt", "Jolt"), ("bar", "jolt2", "Jolt six-DOF"), ("bar", "physx", "PhysX articulation"), ("bar", "pmax", "PhysX maximal / D6 (control)")], body, h, cap,
            "Horizontal bars of the share of cases that stay within the flag levels for each family, backend and solver setting.",
            "Higher is better (share of cases within the flag levels). Green = highest share in its comparison group; rows that run alone stay blue.")
    DATA.append(("pass-fraction.svg", "Direction: higher is better; best = highest share inside a comparison group (chain, limb, each standing setting, the three hip-twist rows together, full body default); the full-body Jolt remedy rows have no comparison group (different case set) and stay blue; no percent of best. Pass = no non-finite state, anchor error <= 0.1 m, axis error <= 0.1 rad in every run of the case; sources: all result JSON files of the four families (full-body remedy files in `full-body/results/jolt-remedy/`; the `pd-sweep` files are not used).",
                 table(["family, setting", "series", "cases passing", "cases", "percent"], trows)))


for fn in (chart_limb_anchor, chart_onset, chart_standing, chart_hip, chart_fb_anchor, chart_fb_axis, chart_fb_tilt, chart_timing, chart_summary):
    fn()

with open(os.path.join(OUT, "DATA.md"), "w", encoding="utf-8", newline="\n") as f:
    f.write("# Values plotted in the charts\n\nGenerated by `tools/gen-charts.py` from the published result files; regenerate with `python -I benchmark/physics-articulated/tools/gen-charts.py` from the repository root. "
            "Do not edit by hand.\n\n"
            "Conventions of all charts: every chart states its direction (lower or higher is better) under its title; the best result of each comparison group (the same case compared across backends or variants; "
            "the rule per chart is the first line of its section) is drawn green, ties all green, and a group with a single bar has no green. "
            "A green bar keeps its series visible through a marker in the series colour left of the axis and, where a row has several series, the series name after the value. "
            "A percentage of best (best = 100%) is printed only for step time (chart 8) and for the upright counts (chart 3, panels 1 and 3); "
            "it is left out for log-scale errors, angles, counts whose best can be 0 and step indices.\n\n")
    for name, note, tab in DATA:
        f.write("## %s\n\n%s\n\n%s\n" % (name, note, tab))
print("wrote", len(DATA), "charts")
