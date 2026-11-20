// Chain-limb test: size_label-limb physics, one harness, two backends (Jolt direct, PhysX CPU articulation), same scenario.
// Scenario (see README): 12-link limb chain, root fixed at a pivot, link masses from root (heavy) to tip (light),
// launched as a rigid rotation so the tip end moves at tip_speed, into a thin wall. Fixed step.
// Timing: only the physics update call is timed (episode rebuilds are outside the timer). Correctness metrics are computed
// from the backend's body poses by the same code for both backends.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "backend.h"
#include "bench_stats.h"
#include "skeleton.h"

namespace artic {
void derive(Params& p) {
  p.mass.assign(static_cast<size_t>(p.links), 0.0f);
  p.half_thick.assign(static_cast<size_t>(p.links), 0.0f);
  for (int i = 0; i < p.links; ++i) {
    float m;
    if (p.profile == "alternating") {
      m = (i % 2 == 0) ? p.mass_max : p.mass_min;
    } else {  // geometric, root = mass_max, tip = mass_min
      const float t = p.links > 1 ? static_cast<float>(i) / static_cast<float>(p.links - 1) : 0.0f;
      m = p.mass_max * std::pow(p.mass_min / p.mass_max, t);
    }
    p.mass[static_cast<size_t>(i)] = m;
    p.half_thick[static_cast<size_t>(i)] = 0.5f * std::cbrt(m / p.mass_max);  // 0.5 m for the heaviest link, 0.05 m for 1000:1 lighter
    if (p.profile == "alternating" && p.light_half_thick > 0.0f && i % 2 == 1) p.half_thick[static_cast<size_t>(i)] = p.light_half_thick;  // same mass, thicker box
  }
}
}  // namespace artic

namespace {

using artic::LinkState;
using artic::Params;

struct V3 { double x, y, z; };
struct Q4 { double x, y, z, w; };
V3 rot(const Q4& q, V3 v) {
  const V3 u{q.x, q.y, q.z};
  const double s = q.w;
  const double d = u.x * v.x + u.y * v.y + u.z * v.z;
  const V3 c{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
  return {2 * d * u.x + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.x + 2 * s * c.x,
          2 * d * u.y + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.y + 2 * s * c.y,
          2 * d * u.z + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.z + 2 * s * c.z};
}
Q4 conj(const Q4& q) { return {-q.x, -q.y, -q.z, q.w}; }
Q4 mul(const Q4& a, const Q4& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Q4 q_of(const LinkState& s) { return {s.q[0], s.q[1], s.q[2], s.q[3]}; }
V3 p_of(const LinkState& s) { return {s.p[0], s.p[1], s.p[2]}; }

struct Metrics {
  // per-step maxima over the whole run (all episodes, warm-up included: correctness is not timing)
  double joint_pos_err_max = 0;   // m, distance between the two anchor points of one hinge
  double joint_pos_err_sum = 0;   // for the mean over all joint samples
  uint64_t joint_samples = 0;
  double joint_axis_err_max = 0;  // rad, hinge axis misalignment
  double joint_limit_excess_max = 0;  // rad, how far a hinge angle went beyond its limit
  double max_pen = 0;             // m, deepest corner past the wall front face
  int episodes_started = 0, episodes_tunnelled = 0;
  bool tunnelled_now = false;
  double max_speed = 0, max_ang_speed = 0;
  double energy_gain_max = 0;     // max over steps of (E(t)-E0)/KE0 ; > 0 means energy was created
  double energy_end_ratio_last = 0;  // E_end / E0 of the last completed episode
  bool nan_seen = false;          // any non-finite value in any episode
  bool ep_nan = false;            // current episode
  int episodes_nan = 0;
  // onset (first occurrence over the whole run; episode = 1-based episode counter, step = 1-based step within that episode)
  int onset_err_ep = 0, onset_err_step = 0;    // first step with any hinge anchor error > 0.01 m (a non-finite state counts too)
  int onset_err1_ep = 0, onset_err1_step = 0;  // first step with any hinge anchor error > 1 m (non-finite counts too)
  int onset_nan_ep = 0, onset_nan_step = 0;    // first step with any non-finite value in any link state
  bool explosion = false;         // harness thresholds: |v|,|w| > 100, joint error > 1 m
  // determinism
  uint64_t traj_hash = 14695981039346656037ull;
};

void fnv(uint64_t& h, const void* data, size_t n) {
  const unsigned char* b = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}

struct Harness {
  Params p;
  std::unique_ptr<artic::Backend> be;
  int episode_len = 0;
  int warmup = 0;
  // run state
  int ep_step = 0;
  int total = 0;
  double ke0 = 1, e0 = 1;
  Metrics m;
  std::vector<double> phys_ms, window_ms;  // in-play physics-only step times, and the subset inside the impact window
  double dt = 0;
  int axis_i = 2;  // hinge axis component index (0 x, 1 y, 2 z)

  void mark(int& ep, int& step) { if (ep == 0) { ep = m.episodes_started; step = ep_step + 1; } }

  double ke_pe(const std::vector<LinkState>& st, double* ke_out) const {
    double ke = 0, pe = 0;
    for (int i = 0; i < p.links; ++i) {
      const auto& s = st[static_cast<size_t>(i)];
      const double mass = p.mass[static_cast<size_t>(i)], h = p.half_thick[static_cast<size_t>(i)], a = p.link_half_len;
      const double ix = mass / 3.0 * (h * h + h * h), iy = mass / 3.0 * (a * a + h * h), iz = iy;
      ke += 0.5 * mass * (double(s.v[0]) * s.v[0] + double(s.v[1]) * s.v[1] + double(s.v[2]) * s.v[2]);
      const V3 wl = rot(conj(q_of(s)), {s.w[0], s.w[1], s.w[2]});
      ke += 0.5 * (ix * wl.x * wl.x + iy * wl.y * wl.y + iz * wl.z * wl.z);
      pe += mass * p.gravity * s.p[2];
    }
    if (ke_out) *ke_out = ke;
    return ke + pe;
  }

  std::vector<LinkState> read() const {
    std::vector<LinkState> st(static_cast<size_t>(p.links));
    for (int i = 0; i < p.links; ++i) st[static_cast<size_t>(i)] = be->link(i);
    return st;
  }

  void begin_episode() {
    ++m.episodes_started;
    m.ep_nan = false;
    m.tunnelled_now = false;
    ep_step = 0;
    if (p.limb) { ke0 = e0 = 1; return; }
    // analytic launch energy: rigid rotation omega about the pivot (z); not read from the backend, so both start from one number
    const double omega = p.tip_speed / (2.0 * p.link_half_len * p.links);
    double ke = 0;
    for (int i = 0; i < p.links; ++i) {
      const double mass = p.mass[static_cast<size_t>(i)], h = p.half_thick[static_cast<size_t>(i)], a = p.link_half_len;
      const double cx = a * (2.0 * i + 1.0);
      ke += 0.5 * mass * (omega * cx) * (omega * cx) + 0.5 * (mass / 3.0 * (a * a + h * h)) * omega * omega;
    }
    ke0 = ke;
    e0 = ke;  // z = 0 for all links at launch
  }

  void end_episode() {
    if (p.limb) { if (m.tunnelled_now) ++m.episodes_tunnelled; if (m.ep_nan) ++m.episodes_nan; return; }
    const auto st = read();
    const double e = ke_pe(st, nullptr);
    m.energy_end_ratio_last = e / e0;
    if (m.tunnelled_now) ++m.episodes_tunnelled;
    if (m.ep_nan) ++m.episodes_nan;
  }

  void finish() {  // count the flags of the last, usually partial, episode
    if (m.tunnelled_now) ++m.episodes_tunnelled;
    if (m.ep_nan) ++m.episodes_nan;
  }

  // ---- limb scenario metrics (same thresholds as the chain: 0.01 m, 1 m, non-finite)
  struct HLocal { V3 lp, lc, ap; Q4 rel0; };
  std::vector<HLocal> hl;
  double ang_err_max = 0, ang_err_final = 0, proj_min_speed = 1e30, proj_speed0 = 0, ang_final_abs_max = 0;
  void limb_setup() {
    const artic::LimbSpec& L = *p.limb;
    hl.clear();
    for (size_t i = 0; i < L.hinge.size(); ++i) {
      const auto& h = L.hinge[i];
      const V3 an{h.anchor[0], h.anchor[1], h.anchor[2]};
      const V3 ax{h.axis[0], h.axis[1], h.axis[2]};
      const auto& c = L.body[i];
      const Q4 qc{c.q[0], c.q[1], c.q[2], c.q[3]};
      V3 pc{c.c[0], c.c[1], c.c[2]};
      HLocal l;
      l.lc = rot(conj(qc), {an.x - pc.x, an.y - pc.y, an.z - pc.z});
      Q4 qp{0, 0, 0, 1};
      V3 pp{0, 0, 0};
      if (i > 0) { const auto& pr = L.body[i - 1]; qp = {pr.q[0], pr.q[1], pr.q[2], pr.q[3]}; pp = {pr.c[0], pr.c[1], pr.c[2]}; }
      l.lp = rot(conj(qp), {an.x - pp.x, an.y - pp.y, an.z - pp.z});
      const double n = std::sqrt(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z);
      l.ap = rot(conj(qp), {ax.x / n, ax.y / n, ax.z / n});
      l.rel0 = mul(conj(qp), qc);
      hl.push_back(l);
    }
    ang_err_max = ang_err_final = ang_final_abs_max = 0;
    proj_min_speed = 1e30;
    proj_speed0 = L.proj ? std::sqrt(L.proj_v[0] * L.proj_v[0] + L.proj_v[1] * L.proj_v[1] + L.proj_v[2] * L.proj_v[2]) : 0.0;
  }
  void observe_limb() {
    const artic::LimbSpec& L = *p.limb;
    const int n = p.links;
    std::vector<LinkState> st(static_cast<size_t>(n + (L.proj ? 1 : 0)));
    for (size_t i = 0; i < st.size(); ++i) st[i] = be->link(static_cast<int>(i));
    for (const auto& s : st) {
      fnv(m.traj_hash, s.p, sizeof s.p); fnv(m.traj_hash, s.q, sizeof s.q); fnv(m.traj_hash, s.v, sizeof s.v); fnv(m.traj_hash, s.w, sizeof s.w);
      for (float f : s.p) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.q) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.v) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.w) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
    }
    if (m.ep_nan) { mark(m.onset_nan_ep, m.onset_nan_step); mark(m.onset_err_ep, m.onset_err_step); mark(m.onset_err1_ep, m.onset_err1_step); m.explosion = true; return; }
    const double pi = 3.14159265358979323846;
    double step_ang_err = 0;
    for (int k = 0; k < n; ++k) {
      const auto& c = st[static_cast<size_t>(k)];
      const HLocal& l = hl[static_cast<size_t>(k)];
      const V3 pc = p_of(c);
      const Q4 qc = q_of(c);
      const V3 rc = rot(qc, l.lc);
      const V3 ac{pc.x + rc.x, pc.y + rc.y, pc.z + rc.z};
      V3 ap = {l.lp.x, l.lp.y, l.lp.z};
      Q4 qp{0, 0, 0, 1};
      if (k > 0) { const auto& par = st[static_cast<size_t>(k - 1)]; qp = q_of(par); const V3 r = rot(qp, l.lp); const V3 pp = p_of(par); ap = {pp.x + r.x, pp.y + r.y, pp.z + r.z}; }
      const double e = std::sqrt((ap.x - ac.x) * (ap.x - ac.x) + (ap.y - ac.y) * (ap.y - ac.y) + (ap.z - ac.z) * (ap.z - ac.z));
      m.joint_pos_err_max = std::max(m.joint_pos_err_max, e);
      m.joint_pos_err_sum += e;
      ++m.joint_samples;
      if (e > 0.01) mark(m.onset_err_ep, m.onset_err_step);
      if (e > 1.0) { mark(m.onset_err1_ep, m.onset_err1_step); m.explosion = true; }
      // hinge axis misalignment: child axis (parent axis rotated by the relative start rotation) vs parent axis in world
      const V3 axp = rot(qp, l.ap);
      const V3 axc = rot(qc, rot(conj(l.rel0), l.ap));  // child frame holds the axis rotated back by rel0
      const double d = std::max(-1.0, std::min(1.0, axp.x * axc.x + axp.y * axc.y + axp.z * axc.z));
      m.joint_axis_err_max = std::max(m.joint_axis_err_max, std::acos(d));
      const Q4 rel = mul(conj(qp), qc);
      const Q4 Rr = mul(rel, conj(l.rel0));
      double ang = 2.0 * std::atan2(Rr.x * l.ap.x + Rr.y * l.ap.y + Rr.z * l.ap.z, Rr.w);
      while (ang > pi) ang -= 2 * pi;
      while (ang < -pi) ang += 2 * pi;
      const auto& hg = L.hinge[static_cast<size_t>(k)];
      m.joint_limit_excess_max = std::max(m.joint_limit_excess_max, std::max(ang - hg.hi, hg.lo - ang));
      if (hg.drive) step_ang_err = std::max(step_ang_err, std::fabs(ang));
    }
    ang_err_max = std::max(ang_err_max, step_ang_err);
    ang_err_final = step_ang_err;
    if (L.wall) {
      const V3 nw{L.wall_n[0], L.wall_n[1], L.wall_n[2]}, cw{L.wall_c[0], L.wall_c[1], L.wall_c[2]};
      const double th = L.wall_half[1];
      for (int i = 0; i < n; ++i) {
        const auto& s = st[static_cast<size_t>(i)];
        const auto& b = L.body[static_cast<size_t>(i)];
        const Q4 q = q_of(s);
        for (int cx = -1; cx <= 1; cx += 2) for (int cy = -1; cy <= 1; cy += 2) for (int cz = -1; cz <= 1; cz += 2) {
          const V3 r = rot(q, {cx * b.half[0], cy * b.half[1], cz * b.half[2]});
          const V3 x{s.p[0] + r.x - cw.x, s.p[1] + r.y - cw.y, s.p[2] + r.z - cw.z};
          m.max_pen = std::max(m.max_pen, -(x.x * nw.x + x.y * nw.y + x.z * nw.z - th));  // beyond the front face (plane at +th along n from the centre)
        }
        const double dc = (s.p[0] - cw.x) * nw.x + (s.p[1] - cw.y) * nw.y + (s.p[2] - cw.z) * nw.z;
        if (dc < -th) m.tunnelled_now = true;  // a link centre is behind the back face
      }
    }
    if (L.proj) {
      const auto& s = st[static_cast<size_t>(n)];
      const double sp = std::sqrt(double(s.v[0]) * s.v[0] + double(s.v[1]) * s.v[1] + double(s.v[2]) * s.v[2]);
      proj_min_speed = std::min(proj_min_speed, sp);
      if (std::getenv("ARTIC_DBG") && ep_step % 6 == 0) std::fprintf(stderr, "[dbg] step %d proj p (%.2f %.2f %.2f) v (%.2f %.2f %.2f) | last body p (%.2f %.2f %.2f)\n", ep_step, s.p[0], s.p[1], s.p[2], s.v[0], s.v[1], s.v[2], st[size_t(n - 1)].p[0], st[size_t(n - 1)].p[1], st[size_t(n - 1)].p[2]);
    }
    for (int i = 0; i < n; ++i) {
      const auto& s = st[static_cast<size_t>(i)];
      const double sp = std::sqrt(double(s.v[0]) * s.v[0] + double(s.v[1]) * s.v[1] + double(s.v[2]) * s.v[2]);
      const double wp = std::sqrt(double(s.w[0]) * s.w[0] + double(s.w[1]) * s.w[1] + double(s.w[2]) * s.w[2]);
      m.max_speed = std::max(m.max_speed, sp);
      m.max_ang_speed = std::max(m.max_ang_speed, wp);
      if (sp > 150.0 || wp > 100.0) m.explosion = true;  // speed threshold 150 m/s here: a 30 m/s tip speed can rebound faster than the 100 m/s chain threshold only by error
    }
  }

  void observe() {
    if (p.limb) { observe_limb(); return; }
    const auto st = read();
    for (const auto& s : st) {
      fnv(m.traj_hash, s.p, sizeof s.p);
      fnv(m.traj_hash, s.q, sizeof s.q);
      fnv(m.traj_hash, s.v, sizeof s.v);
      fnv(m.traj_hash, s.w, sizeof s.w);
    }
    for (const auto& s : st) {
      for (float f : s.p) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.q) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.v) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
      for (float f : s.w) if (!std::isfinite(f)) m.ep_nan = m.nan_seen = true;
    }
    if (m.ep_nan) { mark(m.onset_nan_ep, m.onset_nan_step); mark(m.onset_err_ep, m.onset_err_step); mark(m.onset_err1_ep, m.onset_err1_step); m.explosion = true; return; }
    // joints
    for (int k = 0; k < p.links; ++k) {
      const auto& c = st[static_cast<size_t>(k)];
      const V3 pc = p_of(c);
      const Q4 qc = q_of(c);
      const V3 ac = [&] { const V3 r = rot(qc, {-p.link_half_len, 0, 0}); return V3{pc.x + r.x, pc.y + r.y, pc.z + r.z}; }();
      V3 ap{0, 0, 0};
      Q4 qp{0, 0, 0, 1};
      if (k > 0) {
        const auto& par = st[static_cast<size_t>(k - 1)];
        qp = q_of(par);
        const V3 r = rot(qp, {p.link_half_len, 0, 0});
        const V3 pp = p_of(par);
        ap = {pp.x + r.x, pp.y + r.y, pp.z + r.z};
      }
      const double e = std::sqrt((ap.x - ac.x) * (ap.x - ac.x) + (ap.y - ac.y) * (ap.y - ac.y) + (ap.z - ac.z) * (ap.z - ac.z));
      m.joint_pos_err_max = std::max(m.joint_pos_err_max, e);
      m.joint_pos_err_sum += e;
      ++m.joint_samples;
      if (e > 0.01) mark(m.onset_err_ep, m.onset_err_step);
      if (e > 1.0) { mark(m.onset_err1_ep, m.onset_err1_step); m.explosion = true; }
      const V3 av{axis_i == 0 ? 1.0 : 0.0, axis_i == 1 ? 1.0 : 0.0, axis_i == 2 ? 1.0 : 0.0};
      const V3 zc = rot(qc, av), zp = rot(qp, av);
      const double d = std::max(-1.0, std::min(1.0, zc.x * zp.x + zc.y * zp.y + zc.z * zp.z));
      m.joint_axis_err_max = std::max(m.joint_axis_err_max, std::acos(d));
      const Q4 rel = mul(conj(qp), qc);
      double ang = 2.0 * std::atan2(axis_i == 0 ? rel.x : axis_i == 1 ? rel.y : rel.z, rel.w);
      const double pi = 3.14159265358979323846;
      while (ang > pi) ang -= 2 * pi;
      while (ang < -pi) ang += 2 * pi;
      m.joint_limit_excess_max = std::max(m.joint_limit_excess_max, std::fabs(ang) - p.joint_limit);
    }
    // wall: front face y = wall_y, back face y = wall_y + 2*half_thick; wall spans x -1..26, z -10..10 (covers the whole limb)
    const double back = p.wall_y + 2.0 * p.wall_half_thick;
    for (int i = 0; i < p.links; ++i) {
      const auto& s = st[static_cast<size_t>(i)];
      const Q4 q = q_of(s);
      const double h = p.half_thick[static_cast<size_t>(i)];
      double ymax = -1e30;
      for (int cx = -1; cx <= 1; cx += 2)
        for (int cy = -1; cy <= 1; cy += 2)
          for (int cz = -1; cz <= 1; cz += 2) {
            const V3 r = rot(q, {cx * double(p.link_half_len), cy * h, cz * h});
            ymax = std::max(ymax, s.p[1] + r.y);
          }
      m.max_pen = std::max(m.max_pen, ymax - p.wall_y);
      if (s.p[1] > back) m.tunnelled_now = true;  // a link centre is behind the wall's back face
      const double sp = std::sqrt(double(s.v[0]) * s.v[0] + double(s.v[1]) * s.v[1] + double(s.v[2]) * s.v[2]);
      const double wp = std::sqrt(double(s.w[0]) * s.w[0] + double(s.w[1]) * s.w[1] + double(s.w[2]) * s.w[2]);
      m.max_speed = std::max(m.max_speed, sp);
      m.max_ang_speed = std::max(m.max_ang_speed, wp);
      if (sp > 100.0 || wp > 100.0) m.explosion = true;
    }
    double ke = 0;
    const double e = ke_pe(st, &ke);
    m.energy_gain_max = std::max(m.energy_gain_max, (e - e0) / ke0);
  }

  void setup() {
    axis_i = p.axis == "y" ? 1 : 2;
    be->build(p);
    m = Metrics{};
    if (p.limb) limb_setup();
    phys_ms.clear();
    window_ms.clear();
    total = 0;
    dt = 1.0 / p.hz;
    begin_episode();
  }

  void step(double dtd) {
    if (ep_step >= episode_len) {
      end_episode();
      be->build(p);  // new episode: rebuilt from scratch, outside the timed region below
      begin_episode();
    }
    const auto t0 = std::chrono::steady_clock::now();
    be->step(static_cast<float>(dtd));
    const auto t1 = std::chrono::steady_clock::now();
    if (total >= warmup) {
      const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      phys_ms.push_back(ms);
      const double t = ep_step * dtd;
      if (t >= 0.04 && t <= 0.30) window_ms.push_back(ms);  // impact window: tip reaches the wall at about 0.08 s
    }
    observe();
    ++ep_step;
    ++total;
  }
};

struct Cli {
  std::string backend = "both";
  std::string out;
  std::string note;
  std::string tag;
  Params p;
  double episode_sec = 2.0;
  int warmup = 120, steps = 1200, runs = 3;
  std::string scenario = "chain";  // chain (original) | limb
  artic::LimbRequest lr;
};

bool parse(int argc, char** argv, Cli& c) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--backend") c.backend = next();
    else if (a == "--hz") c.p.hz = std::atof(next());
    else if (a == "--ccd") c.p.ccd = std::string(next()) != "off";
    else if (a == "--threads") c.p.threads = std::atoi(next());
    else if (a == "--profile") c.p.profile = next();
    else if (a == "--mass-min") c.p.mass_min = static_cast<float>(std::atof(next()));
    else if (a == "--mass-max") c.p.mass_max = static_cast<float>(std::atof(next()));
    else if (a == "--tip-speed") c.p.tip_speed = static_cast<float>(std::atof(next()));
    else if (a == "--gravity") c.p.gravity = static_cast<float>(std::atof(next()));
    else if (a == "--limit") c.p.joint_limit = static_cast<float>(std::atof(next()));
    else if (a == "--warmup") c.warmup = std::atoi(next());
    else if (a == "--steps") c.steps = std::atoi(next());
    else if (a == "--runs") c.runs = std::atoi(next());
    else if (a == "--episode-sec") c.episode_sec = std::atof(next());
    else if (a == "--iters") std::sscanf(next(), "%d,%d", &c.p.vel_iters, &c.p.pos_iters);  // velocity,position iterations
    else if (a == "--axis") c.p.axis = next();
    else if (a == "--light-half-thick") c.p.light_half_thick = static_cast<float>(std::atof(next()));
    else if (a == "--jolt-prio") c.p.jolt_prio = std::string(next()) != "off";
    else if (a == "--jolt-heavy-body1") c.p.jolt_heavy_body1 = std::string(next()) != "off";
    else if (a == "--jolt-inertia-mult") c.p.jolt_inertia_mult = static_cast<float>(std::atof(next()));
    else if (a == "--jolt-max-angvel") c.p.jolt_max_angvel = static_cast<float>(std::atof(next()));
    else if (a == "--physx-tgs") c.p.physx_tgs = std::string(next()) != "off";
    else if (a == "--scenario") c.scenario = next();
    else if (a == "--limb") c.lr.limb = next();
    else if (a == "--size_label") c.lr.size_label = next();
    else if (a == "--limb-case") c.lr.limb_case = next();
    else if (a == "--arm-pose") c.lr.arm_pose = next();
    else if (a == "--worst-axes") c.lr.worst_axes = std::string(next()) != "off";
    else if (a == "--ton-kg") c.lr.ton_kg = std::atof(next());
    else if (a == "--sizes-json") c.lr.sizes_json = next();
    else if (a == "--fbx") c.lr.fbx = next();
    else if (a == "--impact-mass") c.lr.impact_mass = std::atof(next());
    else if (a == "--impact-speed") c.lr.impact_speed = std::atof(next());
    else if (a == "--pd-freq") c.lr.pd_freq = std::atof(next());
    else if (a == "--out") c.out = next();
    else if (a == "--note") c.note = next();
    else if (a == "--tag") c.tag = next();
    else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return false; }
  }
  return true;
}

struct RunOut {
  std::string backend;
  artic::bench::Stats phys, window, bench_incl_reset;
  Metrics m;
  int episode_len = 0, samples = 0, window_samples = 0;
  uint64_t final_hash = 0;
  double ang_err_max = 0, ang_err_final = 0, proj_min_speed = 0, proj_speed0 = 0;
};

uint64_t final_state_hash(const Harness& h) {
  uint64_t x = 14695981039346656037ull;
  for (int i = 0; i < h.p.links; ++i) {
    const LinkState s = h.be->link(i);
    fnv(x, s.p, sizeof s.p); fnv(x, s.q, sizeof s.q); fnv(x, s.v, sizeof s.v); fnv(x, s.w, sizeof s.w);
  }
  return x;
}

// JSON has no NaN/Inf: a non-finite double is written as null; finite values keep the exact printf text they had before.
std::string jn(double v, const char* fmt) {
  if (!std::isfinite(v)) return "null";
  char b[64];
  std::snprintf(b, sizeof b, fmt, v);
  return b;
}

void json_run(std::FILE* f, const RunOut& r, bool last) {
  auto st = [&](const char* n, const artic::bench::Stats& s) {
    std::fprintf(f, "\"%s\": {\"mean_ms\": %s, \"p50_ms\": %s, \"p99_ms\": %s, \"max_ms\": %s}", n, jn(s.mean_ms, "%.4f").c_str(), jn(s.p50_ms, "%.4f").c_str(),
                 jn(s.p99_ms, "%.4f").c_str(), jn(s.max_ms, "%.4f").c_str());
  };
  std::fprintf(f, "    {\"backend\": \"%s\", \"episode_steps\": %d, \"in_play_samples\": %d, \"window_samples\": %d,\n      ", artic::bench::json_escape(r.backend).c_str(),
               r.episode_len, r.samples, r.window_samples);
  st("physics_step_in_play", r.phys); std::fprintf(f, ",\n      ");
  st("physics_step_impact_window", r.window); std::fprintf(f, ",\n      ");
  st("bench_step_incl_episode_rebuild", r.bench_incl_reset); std::fprintf(f, ",\n");
  const Metrics& m = r.m;
  std::fprintf(f,
               "      \"joint_pos_err_max_m\": %s, \"joint_pos_err_mean_m\": %s, \"joint_axis_err_max_rad\": %s, \"joint_limit_excess_max_rad\": %s,\n"
               "      \"max_penetration_m\": %s, \"episodes\": %d, \"episodes_tunnelled\": %d, \"episodes_nan\": %d,\n"
               "      \"max_link_speed_mps\": %s, \"max_link_ang_speed_radps\": %s, \"energy_gain_max_over_ke0\": %s, \"energy_end_over_e0_last_episode\": %s,\n"
               "      \"onset_err_gt_0p01m\": {\"episode\": %d, \"step\": %d}, \"onset_err_gt_1m\": {\"episode\": %d, \"step\": %d}, \"onset_nonfinite\": {\"episode\": %d, \"step\": %d},\n"
               "      \"limb_drive_angle_err_max_rad\": %s, \"limb_drive_angle_err_final_rad\": %s, \"impact_body_min_speed_mps\": %s, \"impact_body_start_speed_mps\": %s,\n"
               "      \"nan\": %s, \"explosion\": %s, \"trajectory_hash\": \"%016llx\", \"final_state_hash\": \"%016llx\"}%s\n",
               jn(m.joint_pos_err_max, "%.6g").c_str(), jn(m.joint_samples ? m.joint_pos_err_sum / double(m.joint_samples) : 0.0, "%.6g").c_str(),
               jn(m.joint_axis_err_max, "%.6g").c_str(), jn(m.joint_limit_excess_max, "%.6g").c_str(),
               jn(m.max_pen, "%.6g").c_str(), m.episodes_started, m.episodes_tunnelled, m.episodes_nan, jn(m.max_speed, "%.6g").c_str(), jn(m.max_ang_speed, "%.6g").c_str(),
               jn(m.energy_gain_max, "%.6g").c_str(), jn(m.energy_end_ratio_last, "%.6g").c_str(),
               m.onset_err_ep, m.onset_err_step, m.onset_err1_ep, m.onset_err1_step, m.onset_nan_ep, m.onset_nan_step,
               jn(r.ang_err_max, "%.6g").c_str(), jn(r.ang_err_final, "%.6g").c_str(), jn(r.proj_min_speed > 1e29 ? 0.0 : r.proj_min_speed, "%.6g").c_str(),
               jn(r.proj_speed0, "%.6g").c_str(),
               m.nan_seen ? "true" : "false", m.explosion ? "true" : "false", static_cast<unsigned long long>(m.traj_hash),
               static_cast<unsigned long long>(r.final_hash), last ? "" : ",");
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i)
    if (std::string(argv[i]) == "--dump-skeleton") return artic::dump_skeleton(argv[i + 1]);
  Cli c;
  if (!parse(argc, argv, c)) return 2;
  if (c.scenario == "limb") {
    if (c.lr.fbx.empty()) { std::fprintf(stderr, "--fbx <humanoid skeleton .fbx> is required (not included in this repository)\n"); return 2; }
    if (c.lr.sizes_json.empty()) c.lr.sizes_json = "benchmark/physics-articulated/common/sizes.json";
    c.lr.gravity = c.p.gravity;
    auto spec = std::make_shared<artic::LimbSpec>();
    std::string rep;
    if (!artic::build_limb(c.lr, *spec, rep)) { std::fprintf(stderr, "limb: %s\n", rep.c_str()); return 2; }
    std::printf("%s", rep.c_str());
    c.p.limb = spec;
    c.p.links = static_cast<int>(spec->body.size());
    c.p.mass.clear();
    c.p.half_thick.clear();
    for (const auto& b : spec->body) { c.p.mass.push_back(static_cast<float>(b.mass)); c.p.half_thick.push_back(static_cast<float>(b.half[1])); }
    c.p.joint_limit = 0;
  } else {
    artic::derive(c.p);
  }
  struct Entry { std::string name; std::unique_ptr<artic::Backend> be; };
  std::vector<Entry> list;
  if (c.backend == "jolt" || c.backend == "both") { auto b = artic::make_jolt(); if (b) list.push_back({"jolt", std::move(b)}); else std::fprintf(stderr, "this build has no Jolt backend\n"); }
  if (c.backend == "physx" || c.backend == "both") { auto b = artic::make_physx(); if (b) list.push_back({"physx", std::move(b)}); else std::fprintf(stderr, "this build has no PhysX backend\n"); }
  if (c.backend == "physx-maximal") { auto b = artic::make_physx_maximal(); if (b) list.push_back({"physx-maximal", std::move(b)}); else std::fprintf(stderr, "this build has no PhysX backend\n"); }
  if (list.empty()) return 2;
  if (!c.p.limb) {  // analytic principal inertia of the box links, the same formulas as ke_pe(): about the long axis (x) and the bending axes (y, z)
    const double a = c.p.link_half_len;
    auto ixx = [&](int i) { const double m = c.p.mass[size_t(i)], h = c.p.half_thick[size_t(i)]; return m / 3.0 * (h * h + h * h); };
    auto iyy = [&](int i) { const double m = c.p.mass[size_t(i)], h = c.p.half_thick[size_t(i)]; return m / 3.0 * (a * a + h * h); };
    if (c.p.links >= 2)
      std::printf("[inertia, analytic] link0 m=%g h=%g: long %g bend %g kg m^2 | link1 m=%g h=%g: long %g bend %g | ratio link0:link1 long %.1f bend %.1f mass %.1f | axis=%s gravity=%g limit=%g\n",
                  double(c.p.mass[0]), double(c.p.half_thick[0]), ixx(0), iyy(0), double(c.p.mass[1]), double(c.p.half_thick[1]), ixx(1), iyy(1), ixx(0) / ixx(1),
                  iyy(0) / iyy(1), double(c.p.mass[0] / c.p.mass[1]), c.p.axis.c_str(), double(c.p.gravity), double(c.p.joint_limit));
  }

  std::FILE* jf = nullptr;
  if (!c.out.empty()) jf = std::fopen(c.out.c_str(), "wb");
  if (jf) {
    std::fprintf(jf, "{\n  \"test\": \"chain-limb\", \"device_class\": \"desktop\", \"tag\": \"%s\", \"timing_note\": \"%s\",\n", artic::bench::json_escape(c.tag).c_str(),
                 artic::bench::json_escape(c.note).c_str());
    std::fprintf(jf, "  \"scenario\": {\"links\": %d, \"kind\": \"%s\", \"limb_label\": \"%s\", \"profile\": \"%s\", \"mass_min_kg\": %g, \"mass_max_kg\": %g, \"tip_speed_mps\": %g, \"step_hz\": %g, \"ccd\": %s, \"threads\": %d,\n",
                 c.p.links, c.scenario.c_str(), c.p.limb ? artic::bench::json_escape(c.p.limb->label).c_str() : "", c.p.profile.c_str(), c.p.mass_min, c.p.mass_max, c.p.tip_speed, c.p.hz, c.p.ccd ? "true" : "false", c.p.threads);
    std::fprintf(jf, "    \"hinge_axis\": \"%s\", \"gravity\": %g, \"joint_limit_rad\": %g, \"light_half_thick_m\": %g, \"jolt_prio\": %s, \"jolt_heavy_body1\": %s, \"jolt_inertia_mult\": %g, \"jolt_max_angvel\": %g, \"physx_tgs\": %s,\n",
                 c.p.axis.c_str(), c.p.gravity, c.p.joint_limit, c.p.light_half_thick, c.p.jolt_prio ? "true" : "false", c.p.jolt_heavy_body1 ? "true" : "false",
                 c.p.jolt_inertia_mult, c.p.jolt_max_angvel, c.p.physx_tgs ? "true" : "false");
    std::fprintf(jf, "    \"episode_sec\": %g, \"warmup_steps\": %d, \"measured_steps\": %d, \"runs\": %d, \"masses_kg\": [", c.episode_sec, c.warmup, c.steps, c.runs);
    for (size_t i = 0; i < c.p.mass.size(); ++i) std::fprintf(jf, "%s%.4g", i ? ", " : "", c.p.mass[i]);
    std::fprintf(jf, "]},\n  \"results\": [\n");
  }

  int rc = 0;
  for (size_t bi = 0; bi < list.size(); ++bi) {
    Harness h;
    h.p = c.p;
    h.be = std::move(list[bi].be);
    h.episode_len = static_cast<int>(std::lround(c.episode_sec * c.p.hz));
    h.warmup = c.warmup;
    artic::bench::Config cfg;
    cfg.name = "chain-limb-" + list[bi].name;
    cfg.device_class = "desktop";
    cfg.step_hz = c.p.hz;
    cfg.warmup_steps = c.warmup;
    cfg.measured_steps = c.steps;
    cfg.runs = 1;  // one bench run per loop iteration, so the harness state can be read between runs
    artic::bench::Workload w;
    w.setup = [&] { h.setup(); };
    w.step = [&](double dt) { h.step(dt); };
    w.backend = [&] { return h.be->name(); };
    std::vector<RunOut> outs;
    for (int r = 0; r < c.runs; ++r) {
      std::vector<artic::bench::RunResult> res;
      if (!artic::bench::run(cfg, w, res)) { rc = 1; break; }
      RunOut o;
      o.backend = res[0].backend;
      o.bench_incl_reset = res[0].stats;
      o.phys = artic::bench::compute_stats(h.phys_ms);
      o.window = artic::bench::compute_stats(h.window_ms);
      h.finish();
      o.m = h.m;
      o.episode_len = h.episode_len;
      o.samples = static_cast<int>(h.phys_ms.size());
      o.window_samples = static_cast<int>(h.window_ms.size());
      o.final_hash = final_state_hash(h);
      o.ang_err_max = h.ang_err_max; o.ang_err_final = h.ang_err_final; o.proj_min_speed = h.proj_min_speed; o.proj_speed0 = h.proj_speed0;
      outs.push_back(o);
      std::printf("[%s run %d] %s\n", cfg.name.c_str(), r + 1, o.backend.c_str());
      std::printf("  timing%s physics-only in-play ms: p50 %.4f p99 %.4f max %.4f | impact-window ms: p50 %.4f p99 %.4f (n=%d/%d)\n",
                  c.note.empty() ? "" : (" [" + c.note + "]").c_str(), o.phys.p50_ms, o.phys.p99_ms, o.phys.max_ms, o.window.p50_ms, o.window.p99_ms,
                  o.window_samples, o.samples);
      std::printf("  joint err max %.3e m (mean %.3e) axis %.3e rad limit-excess %.3e rad | max pen %.4f m, tunnelled episodes %d/%d, nan episodes %d\n",
                  o.m.joint_pos_err_max, o.m.joint_samples ? o.m.joint_pos_err_sum / double(o.m.joint_samples) : 0.0, o.m.joint_axis_err_max,
                  o.m.joint_limit_excess_max, o.m.max_pen, o.m.episodes_tunnelled, o.m.episodes_started, o.m.episodes_nan);
      std::printf("  max |v| %.2f m/s max |w| %.2f rad/s energy gain max %.4f x KE0, E_end/E0 %.4f, nan=%d explosion=%d\n", o.m.max_speed, o.m.max_ang_speed,
                  o.m.energy_gain_max, o.m.energy_end_ratio_last, o.m.nan_seen, o.m.explosion);
      auto on = [](int ep, int st) { char b[48]; if (ep == 0) std::snprintf(b, sizeof b, "never"); else std::snprintf(b, sizeof b, "ep %d step %d", ep, st); return std::string(b); };
      std::printf("  ONSET (1-based step in episode; episode = %d steps at %g Hz): anchor err > 0.01 m: %s | > 1 m: %s | non-finite state: %s\n", h.episode_len, c.p.hz,
                  on(o.m.onset_err_ep, o.m.onset_err_step).c_str(), on(o.m.onset_err1_ep, o.m.onset_err1_step).c_str(), on(o.m.onset_nan_ep, o.m.onset_nan_step).c_str());
      if (c.p.limb) std::printf("  limb: PD angle error max %.5f rad final %.5f rad | impact body speed start %.2f min %.2f m/s | tunnelled episodes %d/%d\n", o.ang_err_max, o.ang_err_final, o.proj_speed0, o.proj_min_speed > 1e29 ? 0.0 : o.proj_min_speed, o.m.episodes_tunnelled, o.m.episodes_started);
      std::printf("  hash trajectory %016llx final %016llx\n", static_cast<unsigned long long>(o.m.traj_hash), static_cast<unsigned long long>(o.final_hash));
    }
    bool det = !outs.empty();
    for (const auto& o : outs) det = det && o.m.traj_hash == outs[0].m.traj_hash && o.final_hash == outs[0].final_hash;
    std::printf("[%s] determinism: %zu runs, all trajectory+final hashes equal: %s\n", cfg.name.c_str(), outs.size(), det ? "YES" : "NO");
    if (jf) {
      std::fprintf(jf, "   {\"backend_key\": \"%s\", \"deterministic_all_runs_equal\": %s, \"runs\": [\n", list[bi].name.c_str(), det ? "true" : "false");
      for (size_t i = 0; i < outs.size(); ++i) json_run(jf, outs[i], i + 1 == outs.size());
      std::fprintf(jf, "   ]}%s\n", bi + 1 < list.size() ? "," : "");
    }
    list[bi].be = std::move(h.be);
  }
  if (jf) { std::fprintf(jf, "  ]\n}\n"); std::fclose(jf); }
  return rc;
}
