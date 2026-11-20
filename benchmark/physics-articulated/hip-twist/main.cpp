// Hip-twist test: 3-DOF hip/shoulder (swing + twist) on the humanoid limb chains. One harness, up to four variants (Jolt SwingTwist, Jolt SixDOF,
// PhysX articulation spherical, PhysX maximal PxD6Joint), the same Spec for all. Correctness and onset only; timings are not valid (busy machine).
// The skeleton reader and limb builder are the unchanged files of ../chain-limb (compiled from there).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hip.h"
#include "skeleton.h"

using namespace artic_hip;

namespace {

const double kPi = 3.14159265358979323846;

Qd q_from_frame(Vd ex, Vd ey, Vd ez) {  // rotation whose columns are ex, ey, ez
  const double m00 = ex.x, m10 = ex.y, m20 = ex.z, m01 = ey.x, m11 = ey.y, m21 = ey.z, m02 = ez.x, m12 = ez.y, m22 = ez.z;
  Qd q;
  const double tr = m00 + m11 + m22;
  if (tr > 0) { const double s = std::sqrt(tr + 1.0) * 2; q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s}; }
  else if (m00 > m11 && m00 > m22) { const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2; q = {0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s}; }
  else if (m11 > m22) { const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2; q = {(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s}; }
  else { const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2; q = {(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s}; }
  return normalize(q);
}

std::string jn(double v, const char* fmt = "%.6g") {  // JSON has no NaN/Inf: non-finite -> null
  if (!std::isfinite(v)) return "null";
  char b[64];
  std::snprintf(b, sizeof b, fmt, v);
  return b;
}
std::string jesc(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if (c == '\n') o += "\\n"; else if (static_cast<unsigned char>(c) < 0x20) o += ' '; else o += c; }
  return o;
}
void fnv(uint64_t& h, const void* data, size_t n) {
  const unsigned char* b = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}

struct Cli {
  std::string size_label = "h76m", limb = "leg", cas = "hold", variant = "all", out, tag, note = "NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)";
  std::string fbx = "", sizes = "benchmark/physics-articulated/common/sizes.json";
  double hz = 120.0, sec = -1;
  int runs = 2;
  double ton_kg = 1000.0;
};

// ---------- case set-up -------------------------------------------------------------------------------------------------------------------------
const double kTwLim = 1.2, kSw1Lim = 1.2, kSw2Lim = 0.6;  // ASSUMPTION: plausible ranges (chain-limb used pitch 1.5, abduction 0.6), not from the file
const double kTwist60 = 60.0 * kPi / 180.0;
const double kRaise = 0.9;  // rad, pitch raise in the swing case (the chain-limb swing case also raised by 0.9 rad)

bool build_spec(Cli& c, Spec& S, std::string& rep) {
  artic::LimbRequest rq;
  rq.fbx = c.fbx; rq.limb = c.limb; rq.sizes_json = c.sizes; rq.size_label = c.size_label; rq.limb_case = "hold"; rq.arm_pose = "hang"; rq.ton_kg = c.ton_kg;
  artic::LimbSpec L;
  std::string r0;
  if (!artic::build_limb(rq, L, r0)) { rep = r0; return false; }
  std::ostringstream R;
  R << r0;
  char b[512];
  auto P = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof b, fmt, a...); R << b << "\n"; };
  S = Spec{};
  S.gravity = rq.gravity;
  for (int i = 0; i < 3; ++i) {
    const artic::LimbBody& s = L.body[size_t(i + 1)];  // body 0 of the chain-limb chain is the small gimbal: dropped, the real ball joint replaces it
    Body& d = S.body[i];
    std::copy(s.c, s.c + 3, d.c); std::copy(s.q, s.q + 4, d.q); std::copy(s.half, s.half + 3, d.half); d.mass = s.mass;
    for (int k = 0; k < 3; ++k) d.v[k] = d.w[k] = 0;
  }
  for (int h = 0; h < 2; ++h) {
    const artic::LimbHinge& s = L.hinge[size_t(h + 2)];
    Hinge& d = S.hinge[h];
    std::copy(s.anchor, s.anchor + 3, d.anchor); std::copy(s.axis, s.axis + 3, d.axis);
    d.lo = s.lo; d.hi = s.hi; d.kp = s.kp; d.kd = s.kd; d.cap = s.max_torque; d.drive = true;
  }
  std::copy(L.tip, L.tip + 3, S.tip);
  Ball& B = S.ball;
  const Vd an = vd(L.hinge[1].anchor);
  put(B.anchor, an);
  const Vd t = unit(rot(qd(S.body[0].q), Vd{1, 0, 0}));  // long axis of the thigh / upper arm (local x), neutral pose
  Vd s1 = vd(L.hinge[1].axis);
  s1 = unit(s1 - t * dot(s1, t));
  const Vd z = cross(t, s1);
  const Qd F = q_from_frame(t, s1, z);
  putq(B.F, F);
  B.tw_lim = kTwLim; B.sw1_lim = kSw1Lim; B.sw2_lim = kSw2Lim;
  P("[artic_hip] ball joint at %.3f %.3f %.3f (hip/shoulder of the chain-limb chain, the chain-limb gimbal body is dropped); frame x (twist, thigh long axis) = (%.3f %.3f %.3f), y (pitch, chain-limb hinge 1 axis made orthogonal) = (%.3f %.3f %.3f), z = x cross y = (%.3f %.3f %.3f)",
    an.x, an.y, an.z, t.x, t.y, t.z, s1.x, s1.y, s1.z, z.x, z.y, z.z);
  P("[artic_hip] limits (ASSUMPTION): twist +-%.2f rad, swing about frame y (pitch) +-%.2f rad, about frame z (abduction) +-%.2f rad, cone (elliptic) swing limit", kTwLim, kSw1Lim, kSw2Lim);

  // ---- case: start rotation Qs (applied to the whole limb about the ball anchor), target Rt, which axes are driven, launch, wall, projectile
  Qd Qs{}, Rt{};
  bool dtw = true, dsw = true;
  double sec = 4.0, om = 0;
  const double wall_time = 0.5;
  if (c.cas == "hold") { sec = 4; }
  else if (c.cas == "swing") {  // HIP_DIAG_NOTWIST / HIP_DIAG_SWINGDRIVE: diagnostic switches (default off), used to isolate a Jolt SwingTwist start-up transient
    Qs = axis_angle(s1, kRaise) * (std::getenv("HIP_DIAG_NOTWIST") ? Qd{} : axis_angle(t, kTwist60)); Rt = Qs; dsw = std::getenv("HIP_DIAG_SWINGDRIVE") != nullptr; sec = 8;
  }
  else if (c.cas == "wall") { Rt = std::getenv("HIP_DIAG_WALL_NOTWIST") ? Qd{} : axis_angle(t, std::getenv("HIP_DIAG_WALL_TW") ? std::atof(std::getenv("HIP_DIAG_WALL_TW")) : 1.6); dsw = false; sec = 2; }
  else if (c.cas == "impact") { sec = 2; }
  else if (c.cas == "limits") { Rt = from_rotvec(s1 * 1.5 + z * 0.9) * axis_angle(t, 1.8); sec = 4; }
  else { rep = "unknown case " + c.cas; return false; }
  if (c.sec > 0) sec = c.sec;
  S.ball.drive_twist = dtw; S.ball.drive_swing = dsw;
  putq(B.start, Qs); putq(B.target, Rt);
  S.launch_omega = 0;
  // apply the start rotation to the bodies, hinge frames and the tip
  auto xf = [&](Vd p) { return an + rot(Qs, p - an); };
  for (int i = 0; i < 3; ++i) { put(S.body[i].c, xf(vd(S.body[i].c))); putq(S.body[i].q, Qs * qd(S.body[i].q)); }
  for (int h = 0; h < 2; ++h) { put(S.hinge[h].anchor, xf(vd(S.hinge[h].anchor))); put(S.hinge[h].axis, rot(Qs, vd(S.hinge[h].axis))); }
  put(S.tip, xf(vd(S.tip)));
  P("[artic_hip] case %s: start rotation about the ball %.3f rad (angle of Qs), target rotation vector in the neutral frame (%.3f %.3f %.3f) rad, drives: twist %s, swing %s; episode %.1f s", c.cas.c_str(),
    norm(rotvec(Qs)), rotvec(Rt).x, rotvec(Rt).y, rotvec(Rt).z, dtw ? "on" : "OFF", dsw ? "on" : "OFF", sec);

  // ---- PD gains per ball axis: same rule as chain-limb (kp = I (2 pi f)^2, kd = 2 zeta sqrt(kp I), cap = max(5 x static gravity torque, 0.25 x sum m g r_perp)), downstream bodies about the anchor, start pose
  const Vd axw[3] = {t, s1, z};
  const double w0 = 2.0 * kPi * rq.pd_freq;
  for (int k = 0; k < 3; ++k) {
    const Vd a = axw[k];
    double I = 0, tg = 0, lever = 0;
    for (int i = 0; i < 3; ++i) {
      const Body& bd = S.body[i];
      const Vd r = vd(bd.c) - an;
      const double rp2 = dot(r, r) - dot(r, a) * dot(r, a);
      const Vd al = rot(conj(qd(bd.q)), a);
      const double hx = bd.half[0], hy = bd.half[1], hz = bd.half[2];
      const double Ix = bd.mass / 3.0 * (hy * hy + hz * hz), Iy = bd.mass / 3.0 * (hx * hx + hz * hz), Iz = bd.mass / 3.0 * (hx * hx + hy * hy);
      I += Ix * al.x * al.x + Iy * al.y * al.y + Iz * al.z * al.z + bd.mass * rp2;
      tg += dot(a, cross(r, Vd{0, 0, -bd.mass * S.gravity}));
      lever += bd.mass * S.gravity * std::sqrt(rp2);
    }
    B.kp[k] = I * w0 * w0;
    B.kd[k] = 2.0 * rq.pd_zeta * std::sqrt(B.kp[k] * I);
    B.cap[k] = std::max(rq.torque_factor * std::fabs(tg), 0.25 * lever);
    if (k == 0) B.cap[k] = std::max(B.cap[k], 1.2 * B.kp[k]);  // twist: the static gravity torque is ~0, so the chain-limb rule alone would leave a cap below the PD torque for a 1 rad step
    if (c.cas == "limits") B.cap[k] *= 100.0;  // limits case: the chain-limb cap rule cannot lift the limb to the swing limit (checked: pitch stalled at 0.75 rad of 1.2 with the chain-limb cap), so the drive is made strong enough to really push into the limits
    P("[artic_hip] ball axis %s: downstream inertia %.4g kg m^2, static gravity torque %.4g N m, PD kp %.4g kd %.4g torque cap %.4g N m (%.1f Hz, zeta %.1f)", k == 0 ? "twist  " : k == 1 ? "swing1 " : "swing2 ", I, tg, B.kp[k], B.kd[k], B.cap[k], rq.pd_freq, rq.pd_zeta);
  }
  // knee/ankle gains come from chain-limb's limb builder (hold case: start pose = file pose); the swing case rotates the limb, the inertia about the hinge axes does not change
  // ---- launch and wall (wall case) / projectile (impact case): same construction as chain-limb limb.cpp, with the ball's pitch axis in place of hinge 1
  auto corner = [&](const Body& bd, int sx, int sy, int sz) { return vd(bd.c) + rot(qd(bd.q), Vd{sx * bd.half[0], sy * bd.half[1], sz * bd.half[2]}); };
  const Vd ex{1, 0, 0}, ez{0, 0, 1};
  if (c.cas == "wall") {
    const Vd rtv = vd(S.tip) - an;
    const double rho = norm(rtv - s1 * dot(rtv, s1));
    const Vd v1 = cross(s1, rtv);
    const double sgn = v1.y < 0 ? 1.0 : -1.0;
    om = sgn * 30.0 / rho;
    const Vd vdir = unit(v1 * sgn);
    S.launch_omega = om;
    for (int i = 0; i < 3; ++i) { put(S.body[i].w, s1 * om); put(S.body[i].v, cross(s1 * om, vd(S.body[i].c) - an)); }
    double dmax = -1e30;
    for (const Body& bd : S.body) for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) dmax = std::max(dmax, dot(corner(bd, sx, sy, sz), vdir));
    const double thick = 0.025, face = dmax + 30.0 * wall_time;
    S.wall = true;
    const Vd tipp = vd(S.tip);
    put(S.wall_c, tipp + vdir * (face + thick - dot(tipp, vdir)));
    const Vd xx = unit(std::fabs(vdir.z) < 0.9 ? cross(ez, vdir) : cross(ex, vdir));
    putq(S.wall_q, q_from_frame(xx, vdir, cross(xx, vdir)));
    const double ext = 3.0 * norm(rtv) + 10.0;
    S.wall_half[0] = ext; S.wall_half[1] = thick; S.wall_half[2] = ext;
    put(S.wall_n, vdir * -1.0);
    P("[artic_hip] wall case: pitch omega %.5f rad/s => tip speed 30 m/s (rho %.3f m); launch direction (%.3f %.3f %.3f); wall face %.1f m beyond the leading corner (%.2f s at tip speed: the twist drive (target 1.6 rad > limit %.2f) reaches the twist limit before the hit), 5 cm thick",
      om, rho, vdir.x, vdir.y, vdir.z, 30.0 * wall_time, wall_time, kTwLim);
  }
  if (c.cas == "impact") {
    double ymin = 1e30;
    for (const Body& bd : S.body) for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) ymin = std::min(ymin, corner(bd, sx, sy, sz).y);
    const double half = 0.5;
    S.proj = true;
    S.proj_half[0] = S.proj_half[1] = S.proj_half[2] = half;
    S.proj_mass = rq.impact_mass;
    S.proj_c[0] = S.body[2].c[0]; S.proj_c[1] = ymin - half - rq.impact_speed * 0.08; S.proj_c[2] = S.body[2].c[2];
    S.proj_v[0] = 0; S.proj_v[1] = rq.impact_speed; S.proj_v[2] = 0;
    P("[artic_hip] impact case: %.0f kg 1 m cube at %.1f m/s along +y, %.2f m in front of the frontmost corner, aimed at the centre of the last body (as chain-limb)", rq.impact_mass, rq.impact_speed, rq.impact_speed * 0.08);
  }
  char lb[256];
  std::snprintf(lb, sizeof lb, "%s size_label=%s case=%s", c.limb.c_str(), c.size_label.c_str(), c.cas.c_str());
  S.label = lb;
  P("[artic_hip] bodies (start pose; centre / half extents / mass):");
  for (int i = 0; i < 3; ++i) P("  body%d c (%.2f %.2f %.2f) half (%.3f %.3f %.3f) mass %.4g kg", i, S.body[i].c[0], S.body[i].c[1], S.body[i].c[2], S.body[i].half[0], S.body[i].half[1], S.body[i].half[2], S.body[i].mass);
  P("  tip (%.2f %.2f %.2f)", S.tip[0], S.tip[1], S.tip[2]);
  rep = R.str() + "\n";
  S.launch_omega = om;
  c.sec = sec;
  return true;
}

// ---------- metrics -----------------------------------------------------------------------------------------------------------------------------
struct Res {
  int steps = 0;
  int onset_nan = 0, onset_err001 = 0, onset_err1 = 0, onset_axis01 = 0;
  double anchor_ball_max = 0, anchor_knee_max = 0, anchor_ankle_max = 0, anchor_max = 0;
  double hinge_axis_err_max = 0;
  double hinge_ang_err_max = 0, hinge_limit_excess_max = 0;
  double twist_excess_max = 0, swing_excess_ell_max = 0, swing_excess_rect_max = 0, expmap_excess_max = 0;
  double over_jolt = -1, over_px = -1;
  double twist_max_abs = 0, swing1_max_abs = 0, swing2_max_abs = 0;
  double ori_err_max = 0, ori_err_steady = 0, tw_err_max = 0, tw_err_steady = 0;  // ball: full orientation error, twist-angle error
  double ori_err_final = 0, tw_err_final = 0;
  double jitter_rms = 0, jitter_max = 0, w0_rms_last_half = 0, w0_max = 0, frac_twist_limit = 0, frac_swing_limit = 0;
  double max_pen = 0, max_speed = 0, max_ang_speed = 0, proj_min_speed = 1e30, proj_speed0 = 0;
  bool tunnelled = false, explosion = false, nan_seen = false;
  double start_dev_m = 0;
  double ms_mean = 0;  // NOT VALID FOR TIMING
  uint64_t hash = 14695981039346656037ull;
};

Res run_once(Backend& be, const Spec& S, double hz, double sec, std::string& note) {
  Res R;
  if (!be.build(S, hz, note)) { R.steps = -1; return R; }
  const double dt = 1.0 / hz;
  const int n = int(std::lround(sec * hz));
  const bool has_proj = S.proj;
  const Qd F = qd(S.ball.F), Rs = qd(S.ball.start), Rt = qd(S.ball.target);
  const Qd qn = conj(Rs) * qd(S.body[0].q);  // neutral orientation of the thigh/upper arm
  const Qd Rcs_t = conj(F) * Rt * F;
  const double tw_target = split(Rcs_t).twist;
  const Vd an = vd(S.ball.anchor);
  // local anchors / axes at the start pose
  Vd lcb = rot(conj(qd(S.body[0].q)), an - vd(S.body[0].c));
  Vd lp[2], lc[2], ap[2], ac[2];
  Qd rel0[2];
  for (int h = 0; h < 2; ++h) {
    const Body& P0 = S.body[h]; const Body& C0 = S.body[h + 1];
    const Vd a = vd(S.hinge[h].anchor), ax = unit(vd(S.hinge[h].axis));
    lp[h] = rot(conj(qd(P0.q)), a - vd(P0.c)); lc[h] = rot(conj(qd(C0.q)), a - vd(C0.c));
    ap[h] = rot(conj(qd(P0.q)), ax); ac[h] = rot(conj(qd(C0.q)), ax);
    rel0[h] = conj(qd(P0.q)) * qd(C0.q);
  }
  // start-pose self-check on the freshly built world
  for (int i = 0; i < 3; ++i) { const auto s0 = be.link(i); R.start_dev_m = std::max(R.start_dev_m, norm(Vd{s0.p[0] - S.body[i].c[0], s0.p[1] - S.body[i].c[1], s0.p[2] - S.body[i].c[2]})); }
  if (has_proj) R.proj_speed0 = norm(vd(S.proj_v));
  std::vector<Vd> w0hist;
  w0hist.reserve(size_t(n));
  std::vector<double> ori(size_t(n), 0.0), twe(size_t(n), 0.0);
  std::vector<char> at_tw(size_t(n), 0), at_sw(size_t(n), 0);
  double ms_total = 0;
  int ran = 0;
  for (int step = 1; step <= n; ++step) {
    const auto t0 = std::chrono::steady_clock::now();
    be.step(float(dt));
    ms_total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ran = step;
    artic::LinkState st[4];
    const int nb = has_proj ? 4 : 3;
    bool bad = false;
    for (int i = 0; i < nb; ++i) {
      st[i] = be.link(i);
      fnv(R.hash, st[i].p, sizeof st[i].p); fnv(R.hash, st[i].q, sizeof st[i].q); fnv(R.hash, st[i].v, sizeof st[i].v); fnv(R.hash, st[i].w, sizeof st[i].w);
      for (float f : st[i].p) if (!std::isfinite(f)) bad = true;
      for (float f : st[i].q) if (!std::isfinite(f)) bad = true;
      for (float f : st[i].v) if (!std::isfinite(f)) bad = true;
      for (float f : st[i].w) if (!std::isfinite(f)) bad = true;
    }
    if (bad) {
      R.nan_seen = R.explosion = true;
      if (!R.onset_nan) R.onset_nan = step;
      if (!R.onset_err001) R.onset_err001 = step;
      if (!R.onset_err1) R.onset_err1 = step;
      break;
    }
    auto P_of = [&](int i) { return Vd{st[i].p[0], st[i].p[1], st[i].p[2]}; };
    auto Q_of = [&](int i) { return normalize(Qd{st[i].q[0], st[i].q[1], st[i].q[2], st[i].q[3]}); };
    // anchors
    double e_ball = norm(P_of(0) + rot(Q_of(0), lcb) - an);
    double e_h[2];
    for (int h = 0; h < 2; ++h) e_h[h] = norm((P_of(h) + rot(Q_of(h), lp[h])) - (P_of(h + 1) + rot(Q_of(h + 1), lc[h])));
    const double e = std::max(e_ball, std::max(e_h[0], e_h[1]));
    if (std::getenv("HIP_DBG") && (step <= 6 || step % 40 == 0)) std::fprintf(stderr, "[dbg] step %d ball %.4g knee %.4g ankle %.4g\n", step, e_ball, e_h[0], e_h[1]);
    R.anchor_ball_max = std::max(R.anchor_ball_max, e_ball);
    R.anchor_knee_max = std::max(R.anchor_knee_max, e_h[0]);
    R.anchor_ankle_max = std::max(R.anchor_ankle_max, e_h[1]);
    R.anchor_max = std::max(R.anchor_max, e);
    if (e > 0.01 && !R.onset_err001) R.onset_err001 = step;
    if (e > 1.0 && !R.onset_err1) { R.onset_err1 = step; R.explosion = true; }
    // hinges: axis misalignment, angle vs start pose, limit excess
    for (int h = 0; h < 2; ++h) {
      const Qd qp = Q_of(h), qc = Q_of(h + 1);
      const Vd axp = rot(qp, ap[h]), axc = rot(qc, ac[h]);
      const double d = std::max(-1.0, std::min(1.0, dot(axp, axc)));
      const double ae = std::acos(d);
      R.hinge_axis_err_max = std::max(R.hinge_axis_err_max, ae);
      if (ae > 0.1 && !R.onset_axis01) R.onset_axis01 = step;
      const Qd rel = conj(qp) * qc;
      const Qd Rr = rel * conj(rel0[h]);
      double ang = 2.0 * std::atan2(Rr.x * ap[h].x + Rr.y * ap[h].y + Rr.z * ap[h].z, Rr.w);
      while (ang > kPi) ang -= 2 * kPi;
      while (ang < -kPi) ang += 2 * kPi;
      R.hinge_limit_excess_max = std::max(R.hinge_limit_excess_max, std::max(ang - S.hinge[h].hi, S.hinge[h].lo - ang));
      R.hinge_ang_err_max = std::max(R.hinge_ang_err_max, std::fabs(ang));
    }
    // ball joint: rotation of body 0 relative to the neutral pose, in the joint frame, split into swing and twist
    const Qd Rw = normalize(Q_of(0) * conj(qn));
    const Qd Rcs = conj(F) * Rw * F;
    const SwingTwist stw = split(Rcs);
    const Vd ev = rotvec(Rcs);
    const double sy = stw.swing.y, sz = stw.swing.z, sv = norm(stw.swing);
    R.twist_max_abs = std::max(R.twist_max_abs, std::fabs(stw.twist));
    R.swing1_max_abs = std::max(R.swing1_max_abs, std::fabs(sy));
    R.swing2_max_abs = std::max(R.swing2_max_abs, std::fabs(sz));
    R.twist_excess_max = std::max(R.twist_excess_max, std::fabs(stw.twist) - S.ball.tw_lim);
    const double rr = std::sqrt((sy / S.ball.sw1_lim) * (sy / S.ball.sw1_lim) + (sz / S.ball.sw2_lim) * (sz / S.ball.sw2_lim));
    R.swing_excess_ell_max = std::max(R.swing_excess_ell_max, rr > 1.0 ? (rr - 1.0) * sv / rr : 0.0);
    R.swing_excess_rect_max = std::max(R.swing_excess_rect_max, std::max(std::fabs(sy) - S.ball.sw1_lim, std::fabs(sz) - S.ball.sw2_lim));
    R.expmap_excess_max = std::max(R.expmap_excess_max, std::max(std::fabs(ev.x) - S.ball.tw_lim, std::max(std::fabs(ev.y) - S.ball.sw1_lim, std::fabs(ev.z) - S.ball.sw2_lim)));
    // swing limit in the libraries' own measure: Jolt = ellipse over the swing quaternion y,z (sin of half angles); PhysX D6 = ellipse over tan of quarter angles
    Qd sq = Rcs * conj(Qd{std::sin(0.5 * stw.twist), 0, 0, std::cos(0.5 * stw.twist)});
    if (sq.w < 0) sq = {-sq.x, -sq.y, -sq.z, -sq.w};
    const double rj = std::sqrt((sq.y / std::sin(0.5 * S.ball.sw1_lim)) * (sq.y / std::sin(0.5 * S.ball.sw1_lim)) + (sq.z / std::sin(0.5 * S.ball.sw2_lim)) * (sq.z / std::sin(0.5 * S.ball.sw2_lim)));
    const double rp = std::sqrt((sq.y / (1 + sq.w) / std::tan(0.25 * S.ball.sw1_lim)) * (sq.y / (1 + sq.w) / std::tan(0.25 * S.ball.sw1_lim)) + (sq.z / (1 + sq.w) / std::tan(0.25 * S.ball.sw2_lim)) * (sq.z / (1 + sq.w) / std::tan(0.25 * S.ball.sw2_lim)));
    R.over_jolt = std::max(R.over_jolt, rj - 1.0); R.over_px = std::max(R.over_px, rp - 1.0);
    at_tw[size_t(step - 1)] = std::fabs(stw.twist) >= S.ball.tw_lim - 0.02;
    at_sw[size_t(step - 1)] = std::max(rr, std::max(rj, rp)) >= 0.98;
    const Qd dq = conj(Rt) * Rw;
    const double oe = 2.0 * std::acos(std::min(1.0, std::fabs(dq.w)));
    const double te = std::fabs(stw.twist - tw_target);
    ori[size_t(step - 1)] = oe; twe[size_t(step - 1)] = te;
    R.ori_err_max = std::max(R.ori_err_max, oe); R.tw_err_max = std::max(R.tw_err_max, te);
    const Vd w0{st[0].w[0], st[0].w[1], st[0].w[2]};
    w0hist.push_back(w0);
    // wall, projectile, speeds
    if (S.wall) {
      const Vd nw = vd(S.wall_n), cw = vd(S.wall_c);
      const double th = S.wall_half[1];
      for (int i = 0; i < 3; ++i) {
        const Qd q = Q_of(i);
        for (int cx = -1; cx <= 1; cx += 2) for (int cy = -1; cy <= 1; cy += 2) for (int cz = -1; cz <= 1; cz += 2) {
          const Vd r = rot(q, Vd{cx * S.body[i].half[0], cy * S.body[i].half[1], cz * S.body[i].half[2]});
          R.max_pen = std::max(R.max_pen, -(dot(P_of(i) + r - cw, nw) - th));
        }
        if (dot(P_of(i) - cw, nw) < -th) R.tunnelled = true;
      }
    }
    if (has_proj) {
      const double sp = norm(Vd{st[3].v[0], st[3].v[1], st[3].v[2]});
      R.proj_min_speed = std::min(R.proj_min_speed, sp);
    }
    for (int i = 0; i < 3; ++i) {
      const double sp = norm(Vd{st[i].v[0], st[i].v[1], st[i].v[2]}), wp = norm(Vd{st[i].w[0], st[i].w[1], st[i].w[2]});
      R.max_speed = std::max(R.max_speed, sp); R.max_ang_speed = std::max(R.max_ang_speed, wp);
      if (sp > 150.0 || wp > 100.0) R.explosion = true;
    }
  }
  R.steps = ran;
  R.ms_mean = ran ? ms_total / ran : 0;
  // steady / final: last 10 % of the steps that ran; jitter: last half
  const int m = int(w0hist.size());
  if (m > 2) {
    const int tail = std::max(1, m / 10);
    for (int i = m - tail; i < m; ++i) { R.ori_err_steady = std::max(R.ori_err_steady, ori[size_t(i)]); R.tw_err_steady = std::max(R.tw_err_steady, twe[size_t(i)]); }
    R.ori_err_final = ori[size_t(m - 1)]; R.tw_err_final = twe[size_t(m - 1)];
    const int h0 = m / 2;
    double s2sum = 0, w2sum = 0, ntw = 0, nsw = 0;
    for (int i = h0 + 1; i < m; ++i) {
      const double dj = norm(w0hist[size_t(i)] - w0hist[size_t(i - 1)]);
      s2sum += dj * dj; R.jitter_max = std::max(R.jitter_max, dj);
      w2sum += dot(w0hist[size_t(i)], w0hist[size_t(i)]);
      R.w0_max = std::max(R.w0_max, norm(w0hist[size_t(i)]));
      ntw += at_tw[size_t(i)]; nsw += at_sw[size_t(i)];
    }
    const double cnt = double(m - h0 - 1);
    R.jitter_rms = std::sqrt(s2sum / cnt); R.w0_rms_last_half = std::sqrt(w2sum / cnt);
    R.frac_twist_limit = ntw / cnt; R.frac_swing_limit = nsw / cnt;
  }
  return R;
}

}  // namespace

int main(int argc, char** argv) {
  Cli c;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--size_label") c.size_label = next();
    else if (a == "--limb") c.limb = next();
    else if (a == "--case") c.cas = next();
    else if (a == "--variant") c.variant = next();
    else if (a == "--hz") c.hz = std::atof(next());
    else if (a == "--runs") c.runs = std::atoi(next());
    else if (a == "--episode-sec") c.sec = std::atof(next());
    else if (a == "--out") c.out = next();
    else if (a == "--tag") c.tag = next();
    else if (a == "--note") c.note = next();
    else if (a == "--fbx") c.fbx = next();
    else if (a == "--sizes-json") c.sizes = next();
    else if (a == "--ton-kg") c.ton_kg = std::atof(next());
    else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
  }
  Spec S;
  std::string rep;
  if (!build_spec(c, S, rep)) { std::fprintf(stderr, "spec: %s\n", rep.c_str()); return 2; }
  std::printf("%s", rep.c_str());
  struct Entry { std::string key; std::unique_ptr<Backend> be; };
  std::vector<Entry> list;
  auto add = [&](const char* key, std::unique_ptr<Backend> b) {
    if (c.variant != "all" && c.variant != key) return;
    if (b) list.push_back({key, std::move(b)}); else std::fprintf(stderr, "this build has no variant %s\n", key);
  };
#ifdef HIP_HAVE_JOLT
  add("jolt-swingtwist", make_jolt_swingtwist());
  add("jolt-sixdof", make_jolt_sixdof());
#endif
#ifdef HIP_HAVE_PHYSX
  add("physx-art-spherical", make_physx_art());
  add("physx-d6", make_physx_d6());
#endif
  if (list.empty()) return 2;
  std::FILE* jf = c.out.empty() ? nullptr : std::fopen(c.out.c_str(), "wb");
  if (jf) {
    std::fprintf(jf, "{\n  \"test\": \"hip-twist\", \"device_class\": \"desktop\", \"tag\": \"%s\", \"timing_note\": \"%s\",\n", jesc(c.tag).c_str(), jesc(c.note).c_str());
    std::fprintf(jf, "  \"scenario\": {\"label\": \"%s\", \"limb\": \"%s\", \"size_label\": \"%s\", \"case\": \"%s\", \"step_hz\": %g, \"episode_sec\": %g, \"runs\": %d, \"ccd\": true, \"threads\": 1,\n",
                 jesc(S.label).c_str(), c.limb.c_str(), jesc(c.size_label).c_str(), c.cas.c_str(), c.hz, c.sec, c.runs);
    std::fprintf(jf, "    \"limits_rad\": {\"twist\": %g, \"swing_pitch_frame_y\": %g, \"swing_abduction_frame_z\": %g}, \"ball_drive_twist\": %s, \"ball_drive_swing\": %s, \"masses_kg\": [%s, %s, %s]},\n",
                 S.ball.tw_lim, S.ball.sw1_lim, S.ball.sw2_lim, S.ball.drive_twist ? "true" : "false", S.ball.drive_swing ? "true" : "false", jn(S.body[0].mass, "%.4g").c_str(),
                 jn(S.body[1].mass, "%.4g").c_str(), jn(S.body[2].mass, "%.4g").c_str());
    std::fprintf(jf, "  \"variants\": [\n");
  }
  for (size_t vi = 0; vi < list.size(); ++vi) {
    std::vector<Res> rs;
    std::string note;
    for (int r = 0; r < c.runs; ++r) rs.push_back(run_once(*list[vi].be, S, c.hz, c.sec, note));
    bool det = !rs.empty();
    for (const Res& x : rs) det = det && x.hash == rs[0].hash;
    const std::string nm = list[vi].be->name();
    std::printf("[%s] %s\n  note: %s\n", list[vi].key.c_str(), nm.c_str(), note.c_str());
    for (size_t r = 0; r < rs.size(); ++r) {
      const Res& x = rs[r];
      std::printf("  run %zu: steps %d | nonfinite onset %d | anchor max %.3e m (ball %.3e knee %.3e ankle %.3e) onset>0.01m %d >1m %d | hinge axis err max %.3e rad (>0.1 at %d) hinge ang err max %.3e | twist max %.3f swing1 %.3f swing2 %.3f | limit excess twist %.3e swing-ell %.3e swing-rect %.3e expmap %.3e hinge %.3e\n",
                  r + 1, x.steps, x.onset_nan, x.anchor_max, x.anchor_ball_max, x.anchor_knee_max, x.anchor_ankle_max, x.onset_err001, x.onset_err1, x.hinge_axis_err_max, x.onset_axis01, x.hinge_ang_err_max,
                  x.twist_max_abs, x.swing1_max_abs, x.swing2_max_abs, x.twist_excess_max, x.swing_excess_ell_max, x.swing_excess_rect_max, x.expmap_excess_max, x.hinge_limit_excess_max);
      std::printf("          drive: orientation err max %.3e steady %.3e | twist err max %.3e steady %.3e | jitter rms %.3e max %.3e rad/s per step (|w0| rms %.3e max %.3e) at-twist-limit %.2f at-swing-limit %.2f | pen %.4f m tunnelled %d | max v %.2f w %.2f | start dev %.2e m | hash %016llx\n",
                  x.ori_err_max, x.ori_err_steady, x.tw_err_max, x.tw_err_steady, x.jitter_rms, x.jitter_max, x.w0_rms_last_half, x.w0_max, x.frac_twist_limit, x.frac_swing_limit, x.max_pen, x.tunnelled, x.max_speed,
                  x.max_ang_speed, x.start_dev_m, static_cast<unsigned long long>(x.hash));
    }
    std::printf("[%s] determinism: %zu runs, trajectory hashes equal: %s\n", list[vi].key.c_str(), rs.size(), det ? "YES" : "NO");
    if (jf) {
      std::fprintf(jf, "   {\"variant\": \"%s\", \"backend\": \"%s\", \"build_note\": \"%s\", \"deterministic_all_runs_equal\": %s, \"runs\": [\n", list[vi].key.c_str(), jesc(nm).c_str(), jesc(note).c_str(), det ? "true" : "false");
      for (size_t r = 0; r < rs.size(); ++r) {
        const Res& x = rs[r];
        std::fprintf(jf,
          "    {\"steps_run\": %d, \"onset_nonfinite_step\": %d, \"onset_anchor_gt_0p01m_step\": %d, \"onset_anchor_gt_1m_step\": %d, \"onset_hinge_axis_gt_0p1rad_step\": %d,\n"
          "     \"anchor_err_max_m\": %s, \"anchor_ball_max_m\": %s, \"anchor_knee_max_m\": %s, \"anchor_ankle_max_m\": %s, \"hinge_axis_err_max_rad\": %s, \"hinge_angle_err_max_rad\": %s,\n"
          "     \"twist_abs_max_rad\": %s, \"swing_pitch_abs_max_rad\": %s, \"swing_abduction_abs_max_rad\": %s,\n"
          "     \"limit_excess_twist_rad\": %s, \"limit_excess_swing_ellipse_rad\": %s, \"limit_excess_swing_rect_rad\": %s, \"limit_excess_expmap_rect_rad\": %s, \"swing_over_jolt_ellipse_frac\": %s, \"swing_over_physx_ellipse_frac\": %s, \"limit_excess_hinge_rad\": %s,\n"
          "     \"ball_orientation_err_max_rad\": %s, \"ball_orientation_err_steady_rad\": %s, \"ball_twist_err_max_rad\": %s, \"ball_twist_err_steady_rad\": %s,\n"
          "     \"jitter_rms_radps_per_step\": %s, \"jitter_max_radps_per_step\": %s, \"w_body0_rms_last_half_radps\": %s, \"w_body0_max_last_half_radps\": %s, \"frac_last_half_at_twist_limit\": %s, \"frac_last_half_at_swing_limit\": %s,\n"
          "     \"max_penetration_m\": %s, \"tunnelled\": %s, \"max_link_speed_mps\": %s, \"max_link_ang_speed_radps\": %s, \"impact_body_min_speed_mps\": %s, \"impact_body_start_speed_mps\": %s,\n"
          "     \"nan\": %s, \"explosion\": %s, \"start_pose_dev_m\": %s, \"step_ms_mean_NOT_VALID\": %s, \"trajectory_hash\": \"%016llx\"}%s\n",
          x.steps, x.onset_nan, x.onset_err001, x.onset_err1, x.onset_axis01, jn(x.anchor_max).c_str(), jn(x.anchor_ball_max).c_str(), jn(x.anchor_knee_max).c_str(), jn(x.anchor_ankle_max).c_str(),
          jn(x.hinge_axis_err_max).c_str(), jn(x.hinge_ang_err_max).c_str(), jn(x.twist_max_abs).c_str(), jn(x.swing1_max_abs).c_str(), jn(x.swing2_max_abs).c_str(),
          jn(std::max(0.0, x.twist_excess_max)).c_str(), jn(x.swing_excess_ell_max).c_str(), jn(std::max(0.0, x.swing_excess_rect_max)).c_str(), jn(std::max(0.0, x.expmap_excess_max)).c_str(), jn(x.over_jolt).c_str(), jn(x.over_px).c_str(),
          jn(std::max(0.0, x.hinge_limit_excess_max)).c_str(), jn(x.ori_err_max).c_str(), jn(x.ori_err_steady).c_str(), jn(x.tw_err_max).c_str(), jn(x.tw_err_steady).c_str(),
          jn(x.jitter_rms).c_str(), jn(x.jitter_max).c_str(), jn(x.w0_rms_last_half).c_str(), jn(x.w0_max).c_str(), jn(x.frac_twist_limit, "%.4f").c_str(), jn(x.frac_swing_limit, "%.4f").c_str(),
          jn(x.max_pen).c_str(), x.tunnelled ? "true" : "false", jn(x.max_speed).c_str(), jn(x.max_ang_speed).c_str(), jn(x.proj_min_speed > 1e29 ? 0.0 : x.proj_min_speed).c_str(), jn(x.proj_speed0).c_str(),
          x.nan_seen ? "true" : "false", x.explosion ? "true" : "false", jn(x.start_dev_m).c_str(), jn(x.ms_mean, "%.5f").c_str(), static_cast<unsigned long long>(x.hash), r + 1 < rs.size() ? "," : "");
      }
      std::fprintf(jf, "   ]}%s\n", vi + 1 < list.size() ? "," : "");
    }
  }
  if (jf) { std::fprintf(jf, "  ]\n}\n"); std::fclose(jf); }
  return 0;
}
