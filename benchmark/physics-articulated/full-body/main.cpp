// Full-body test: whole humanoid (humanoid skeleton scaled to a size_label) standing, stepping, in a debris pile, hit by a 2 t body, and hitting a thin wall.
// One harness, two backends (Jolt bodies + hinges, PhysX reduced-coordinate articulation), same scenario. Correctness and onset; timings are NOT VALID FOR TIMING.
// Metrics are computed from the backend body states by the same code for both backends. See README.md.
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
#include "humanoid.h"

namespace {

using artic_body::Backend;
using artic_body::State;

struct V3 { double x, y, z; };
struct Q4 { double x, y, z, w; };
V3 rot(const Q4& q, V3 v) {
  const V3 u{q.x, q.y, q.z};
  const double s = q.w;
  const double d = u.x * v.x + u.y * v.y + u.z * v.z;
  const V3 c{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
  const double uu = u.x * u.x + u.y * u.y + u.z * u.z;
  return {2 * d * u.x + (s * s - uu) * v.x + 2 * s * c.x, 2 * d * u.y + (s * s - uu) * v.y + 2 * s * c.y, 2 * d * u.z + (s * s - uu) * v.z + 2 * s * c.z};
}
Q4 conj(const Q4& q) { return {-q.x, -q.y, -q.z, q.w}; }
Q4 mul(const Q4& a, const Q4& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Q4 q_of(const State& s) { return {s.q[0], s.q[1], s.q[2], s.q[3]}; }
V3 p_of(const State& s) { return {s.p[0], s.p[1], s.p[2]}; }
V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double nrm(V3 a) { return std::sqrt(dot(a, a)); }
V3 V_of(const double d[3]) { return V3{d[0], d[1], d[2]}; }
Q4 q_from_frame(V3 ex, V3 ey, V3 ez) {
  const double m00 = ex.x, m10 = ex.y, m20 = ex.z, m01 = ey.x, m11 = ey.y, m21 = ey.z, m02 = ez.x, m12 = ez.y, m22 = ez.z;
  Q4 q;
  const double tr = m00 + m11 + m22;
  if (tr > 0) { const double s = std::sqrt(tr + 1.0) * 2; q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s}; }
  else if (m00 > m11 && m00 > m22) { const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2; q = {0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s}; }
  else if (m11 > m22) { const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2; q = {(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s}; }
  else { const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2; q = {(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s}; }
  const double l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  return {q.x / l, q.y / l, q.z / l, q.w / l};
}

void fnv(uint64_t& h, const void* data, size_t n) {
  const unsigned char* b = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}
bool finite_state(const State& s) {
  for (float f : s.p) if (!std::isfinite(f)) return false;
  for (float f : s.q) if (!std::isfinite(f)) return false;
  for (float f : s.v) if (!std::isfinite(f)) return false;
  for (float f : s.w) if (!std::isfinite(f)) return false;
  return true;
}
std::string jn(double v, const char* fmt = "%.6g") {  // JSON has no NaN/Inf: non-finite -> null
  if (!std::isfinite(v)) return "null";
  char b[64];
  std::snprintf(b, sizeof b, fmt, v);
  return b;
}
std::string jesc(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

// ---------------------------------------------------------------- scenario
struct Cli {
  std::string backend = "both", out, note, tag, kase = "stand";
  std::string fbx = "", sizes_json = "benchmark/physics-articulated/common/sizes.json", size_label = "h76m";
  double hz = 120.0, ratio = 1000.0, dur = -1, ton_kg = 1000.0, impact_mass = 2000.0, impact_speed = 30.0, tip_speed = 30.0, density = 2400.0;
  int runs = 2, ndebris = 500, vel_iters = -1, pos_iters = -1;
  bool ccd = true;
  artic_body::Request rq;
  bool probe = false;
};

struct Layout {
  artic_body::Scene sc;
  double duration = 5.0;
  std::string desc;
  std::string report;
  // scripted targets (key poses, one angle per body index)
  std::vector<std::vector<double>> keys;
  std::vector<double> key_t;  // key i reached at key_t[i] (linear in time between keys, smoothstep easing)
  double wall_n[3] = {0, 0, 0}, wall_c[3] = {0, 0, 0}, wall_th = 0, wall_xlo = 0, wall_xhi = 0;
  bool wall_case = false, impact_case = false;
  double debris_mass = 0, debris_half = 0, ref_mass = 0;
  int arm_first = -1;  // wall case: first body index of the swinging arm chain
};

struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  double u() { s = s * 6364136223846793005ull + 1442695040888963407ull; return double(s >> 11) / 9007199254740992.0; }
};

double wrap_pi(double a) { const double pi = 3.14159265358979323846; while (a > pi) a -= 2 * pi; while (a < -pi) a += 2 * pi; return a; }

// planar two-link IK in the (y, z) plane. t0 = hip->knee, s0 = knee->ankle at the start pose; hinge angles h (hip pitch) and k (knee) rotate about +x:
// R(psi)(y,z) = (y cos psi - z sin psi, y sin psi + z cos psi); thigh = R(h) t0, shank = R(h+k) s0. Returns false if the target is out of reach (clamped).
bool ik2(const double t0[2], const double s0[2], double Ty, double Tz, double kprev, double& h, double& k) {
  const double A = t0[0] * s0[0] + t0[1] * s0[1], B = t0[1] * s0[0] - t0[0] * s0[1];
  const double Rr = std::hypot(A, B), del = std::atan2(B, A);
  const double C = (Ty * Ty + Tz * Tz - (t0[0] * t0[0] + t0[1] * t0[1]) - (s0[0] * s0[0] + s0[1] * s0[1])) / 2.0;
  double ratio = C / Rr;
  const bool ok = std::fabs(ratio) <= 1.0;
  ratio = std::max(-1.0, std::min(1.0, ratio));
  const double al = std::acos(ratio);
  const double k1 = wrap_pi(del + al), k2 = wrap_pi(del - al);
  k = std::fabs(k1 - kprev) < std::fabs(k2 - kprev) ? k1 : k2;
  const double ck = std::cos(k), sk = std::sin(k);
  const double ux = t0[0] + s0[0] * ck - s0[1] * sk, uz = t0[1] + s0[0] * sk + s0[1] * ck;
  h = wrap_pi(std::atan2(Tz, Ty) - std::atan2(uz, ux));
  return ok;
}

bool make_layout(const Cli& c, Layout& L) {
  artic_body::Request rq = c.rq;
  L.sc.vel_iters = c.vel_iters; L.sc.pos_iters = c.pos_iters;
  rq.fbx = c.fbx; rq.sizes_json = c.sizes_json; rq.size_label = c.size_label; rq.ton_kg = c.ton_kg;
  rq.free_shoulder_pitch = (c.kase == "wall");
  artic_body::Humanoid& M = L.sc.humanoid;
  if (!artic_body::build_humanoid(rq, M)) { std::fprintf(stderr, "humanoid: %s\n", M.report.c_str()); return false; }
  L.report = M.report;
  char b[900];
  auto P = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof b, fmt, a...); L.report += b; L.report += "\n"; };
  L.sc.gravity = rq.gravity;
  L.sc.hz = c.hz;
  L.sc.ccd = c.ccd;
  L.sc.ground_half = std::max(1000.0, 8.0 * M.height);
  const size_t nb = M.b.size();
  L.keys.assign(1, std::vector<double>(nb, 0.0));
  L.key_t.assign(1, 0.0);
  const double kind_dur = c.kase == "stand" ? 5.0 : (c.kase == "step" || c.kase == "step-debris") ? 7.0 : c.kase == "impact" ? 3.0 : 2.0;
  L.duration = c.dur > 0 ? c.dur : kind_dur;
  Rng rng;
  auto torso = M.b[0];
  // ---- debris helpers
  auto debris_size = [&](double ref_mass_kg) {
    L.ref_mass = ref_mass_kg;
    L.debris_mass = ref_mass_kg / c.ratio;
    const double side = std::cbrt(L.debris_mass / c.density);
    L.debris_half = 0.5 * side;
  };
  auto push_debris = [&](double x, double y, double z) {
    artic_body::DebrisBody d{};
    d.c[0] = x; d.c[1] = y; d.c[2] = z;
    d.q[3] = 1.0;
    d.half[0] = d.half[1] = d.half[2] = L.debris_half;
    d.mass = L.debris_mass;
    L.sc.debris.push_back(d);
  };
  if (c.kase == "stand" || c.kase == "impact") {
    L.desc = c.kase == "stand" ? "stand with PD holds (all targets 0)" : "stand, 2 t cube hits the torso front";
    if (c.kase == "impact") {
      const double half = 0.5;
      artic_body::Projectile& pj = L.sc.proj;
      pj.half[0] = pj.half[1] = pj.half[2] = half;
      pj.mass = c.impact_mass;
      pj.c[0] = torso.c[0]; pj.c[1] = torso.c[1] - torso.half[1] - half - c.impact_speed * 0.08; pj.c[2] = torso.c[2];
      pj.v[0] = 0; pj.v[1] = c.impact_speed; pj.v[2] = 0;
      L.sc.has_proj = true;
      L.impact_case = true;
      P("[case] impact: %.0f kg cube (1 m) at %.1f m/s along +y, starting %.2f m in front of the torso front face, aimed at the torso centre (x, z); the 30 m/s is an assumption", c.impact_mass, c.impact_speed, c.impact_speed * 0.08);
    }
  } else if (c.kase == "step" || c.kase == "step-debris") {
    // key poses: S1 crouch, S2 lean (both legs), S3 lift right foot, S4 right foot forward (-y), S5 land, S6 lean back, S7 hold
    const double* t0L = M.thigh0[0]; const double* s0L = M.shank0[0];
    const double Ty0 = t0L[0] + s0L[0], Tz0 = t0L[1] + s0L[1];  // hip->ankle at the start pose (left leg; legs mirror each other)
    const double zc = 0.88 * Tz0;                                // crouch: ankle 12 % closer to the hip (Tz0 < 0)
    const double lift = 0.10 * std::fabs(Tz0);
    const double Dy = std::max(0.20 * std::fabs(Tz0), std::min(1.1 * M.foot_len, 0.33 * std::fabs(Tz0)));
    const double phi = std::asin(std::min(0.95, 0.95 * M.hip_x[0] / std::fabs(zc)));  // lateral lean: pelvis over the left (stance) foot
    auto pose = [&](double Ry_target_y, double Ry_target_z, int side, std::vector<double>& a) {
      double h = 0, k = 0;
      const bool ok = ik2(M.thigh0[side], M.shank0[side], Ry_target_y, Ry_target_z, 0.0, h, k);
      a[size_t(M.hip_pitch[side])] = h; a[size_t(M.knee[side])] = k; a[size_t(M.ankle_pitch[side])] = -(h + k);
      return ok;
    };
    std::vector<double> S1(nb, 0.0);
    bool ok = true;
    ok &= pose(Ty0, zc, 0, S1); ok &= pose(Ty0, zc, 1, S1);
    std::vector<double> S2 = S1;
    for (int sd = 0; sd < 2; ++sd) { S2[size_t(M.hip_abd[sd])] = phi; S2[size_t(M.ankle_roll[sd])] = -phi; }
    std::vector<double> S3 = S2, S4 = S2, S5 = S2;
    ok &= pose(Ty0, zc + lift, 1, S3);
    ok &= pose(Ty0 - Dy, zc + lift, 1, S4);
    ok &= pose(Ty0 - Dy, zc, 1, S5);
    std::vector<double> S6 = S5;
    for (int sd = 0; sd < 2; ++sd) { S6[size_t(M.hip_abd[sd])] = 0; S6[size_t(M.ankle_roll[sd])] = 0; }
    const double T = L.duration / 7.0;  // phase length (1 s for the default 7 s)
    L.keys = {std::vector<double>(nb, 0.0), S1, S2, S3, S4, S5, S6, S6};
    L.key_t = {0, T, 2 * T, 3 * T, 4 * T, 5 * T, 6 * T, 7 * T};
    L.desc = "crouch, lean over the left foot, lift the right foot, move it forward, land, lean back (scripted joint-angle trajectory, no balance controller)";
    P("[case] step script: scripted joint angles from planar 2-link IK of the (y,z) plane (hip->ankle start (%.3f, %.3f) m, crouch to z %.3f, lift %.3f m, forward %.3f m (foot length %.3f m), lean %.3f rad, all targets reachable: %s); phase length %.3f s",
      Ty0, Tz0, zc, lift, Dy, M.foot_len, phi, ok ? "yes" : "NO (clamped)", T);
    if (c.kase == "step-debris") {
      const artic_body::Body& fr = M.b[size_t(M.foot[1])];
      const artic_body::Body& fl = M.b[size_t(M.foot[0])];
      debris_size(fr.mass);
      const double d = L.debris_half, cell = 2.05 * d;
      auto outside_feet = [&](double x, double y) {  // keep debris out of the initial footprints of both feet
        for (const artic_body::Body* f : {&fr, &fl}) if (std::fabs(x - f->c[0]) < f->half[0] + 2.0 * d && std::fabs(y - f->c[1]) < f->half[1] + 2.0 * d) return false;
        return true;
      };
      const double lx = fr.c[0], ly = fr.c[1] - Dy;  // landing centre of the right foot (flat foot, x unchanged, y moved by -Dy)
      std::vector<int> level;  // stacking level per cell, key = (ix, iy) in a map-free way: linear search on a small vector of cells
      struct Cell { long ix, iy; int lvl; };
      std::vector<Cell> cells;
      auto place = [&](double cx, double cy, double hx, double hy, int count) {
        int placed = 0, guard = 0;
        while (placed < count && guard++ < count * 200) {
          const long ix = long(std::floor((cx + (rng.u() * 2 - 1) * hx) / cell)), iy = long(std::floor((cy + (rng.u() * 2 - 1) * hy) / cell));
          const double x = (double(ix) + 0.5) * cell, y = (double(iy) + 0.5) * cell;
          if (!outside_feet(x, y)) continue;
          Cell* cp = nullptr;
          for (Cell& ce : cells) if (ce.ix == ix && ce.iy == iy) { cp = &ce; break; }
          if (!cp) { cells.push_back({ix, iy, 0}); cp = &cells.back(); }
          if (cp->lvl >= 3) continue;
          push_debris(x + (rng.u() * 2 - 1) * 0.02 * d, y + (rng.u() * 2 - 1) * 0.02 * d, d + cp->lvl * (2.0 * d + 0.04 * d) + 0.01 * d);
          ++cp->lvl;
          ++placed;
        }
        return placed;
      };
      const int nA = c.ndebris * 6 / 10;
      const int pa = place(lx, ly, 1.4 * fr.half[0], 1.4 * fr.half[1], nA);
      const int pb = place(0.5 * (fl.c[0] + fr.c[0]), 0.5 * (fl.c[1] + fr.c[1]), std::fabs(fl.c[0]) + 2.0 * fl.half[0], 2.5 * fl.half[1], c.ndebris - pa);
      P("[case] debris: %zu boxes (zone under the landing spot %d, around both feet %d), cubes half %.4f m, mass %.4g kg each = foot mass %.4g kg / %.0f (contact mass ratio foot link : debris body = %.0f:1), density %.0f kg/m^3 (assumption), stacked up to 3 high, none inside the initial foot footprints",
        L.sc.debris.size(), pa, pb, d, L.debris_mass, fr.mass, c.ratio, c.ratio, c.density);
    }
  } else if (c.kase == "wall") {
    L.wall_case = true;
    L.desc = "left arm (shoulder pitch undriven) launched to a 30 m/s tip speed toward a thin wall, debris behind it";
    const int sp = M.sh_pitch[0];
    const V3 an = V_of(M.b[size_t(sp)].anchor), a1 = V_of(M.b[size_t(sp)].axis);
    const V3 tip = V_of(M.arm_tip[0]);
    const V3 rt = sub(tip, an);
    const double rho = nrm(sub(rt, V3{a1.x * dot(rt, a1), a1.y * dot(rt, a1), a1.z * dot(rt, a1)}));
    const V3 v1 = cross(a1, rt);
    // wanted: tip velocity along -y (the humanoid faces -y: its eyes are at -y in the file); v1 = tip velocity direction for omega = +1
    const double sgn = v1.y < 0 ? 1.0 : -1.0;
    const double use_om = sgn * c.tip_speed / rho;
    const V3 vdu = {v1.x * sgn / nrm(v1), v1.y * sgn / nrm(v1), v1.z * sgn / nrm(v1)};
    for (size_t i = 0; i < nb; ++i) {
      const int bi = static_cast<int>(i);
      if (bi == M.sh_pitch[0] || bi == M.elbow[0] || bi == M.wrist[0]) {
        artic_body::Body& bd = M.b[i];
        const V3 w = {a1.x * use_om, a1.y * use_om, a1.z * use_om};
        const V3 v = cross(w, sub(V_of(bd.c), an));
        for (int k = 0; k < 3; ++k) { bd.w[k] = (&w.x)[k]; bd.v[k] = (&v.x)[k]; }
      }
    }
    L.sc.launch_joint = sp;
    L.sc.launch_omega = use_om;
    L.arm_first = M.sh_abd[0];
    // wall: a slab 5 cm thick perpendicular to the launch direction, face where the leading corner arrives after 80 ms
    auto corner = [&](const artic_body::Body& bd, int sx, int sy, int sz) {
      const Q4 q{bd.q[0], bd.q[1], bd.q[2], bd.q[3]};
      return add(V_of(bd.c), rot(q, V3{sx * bd.half[0], sy * bd.half[1], sz * bd.half[2]}));
    };
    double dmax = -1e30, xmin = 1e30, xmax = -1e30;
    for (int bi : {M.sh_abd[0], M.sh_pitch[0], M.elbow[0], M.wrist[0]})
      for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) {
        const V3 cp = corner(M.b[size_t(bi)], sx, sy, sz);
        dmax = std::max(dmax, -cp.y);  // leading corner along -y (the wall plane is y = const)
        xmin = std::min(xmin, cp.x); xmax = std::max(xmax, cp.x);
      }
    const double thick = 0.025, face = dmax + c.tip_speed * 0.08;
    // the wall is a wide slab (as in the chain-limb test) as long as it does not touch any body outside the swinging chain: check the frontmost (-y) corner of every other body
    double other_ymin = 1e30;
    for (size_t i = 0; i < nb; ++i) {
      const int bi = static_cast<int>(i);
      if (bi == M.sh_abd[0] || bi == M.sh_pitch[0] || bi == M.elbow[0] || bi == M.wrist[0]) continue;
      for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) other_ymin = std::min(other_ymin, corner(M.b[i], sx, sy, sz).y);
    }
    const double ext_z = 3.0 * nrm(rt) + 10.0;
    const double torso_max_x = torso.c[0] + torso.half[0];
    const bool full_width = other_ymin > -face + 0.5;  // the slab occupies y in [-face - 2 thick, -face]: clear of every other body
    const double xlo = full_width ? tip.x - ext_z : std::max(xmin - 0.5, torso_max_x + 0.5), xhi = full_width ? tip.x + ext_z : xmax + 0.5;
    L.wall_xlo = xlo; L.wall_xhi = xhi;
    artic_body::Wall& w = L.sc.wall;
    L.sc.has_wall = true;
    const V3 xx = {1, 0, 0};
    w.c[0] = 0.5 * (xlo + xhi); w.c[1] = -(face + thick); w.c[2] = tip.z;
    w.half[0] = 0.5 * (xhi - xlo); w.half[1] = thick; w.half[2] = ext_z;
    const Q4 wq = q_from_frame(xx, V3{0, -1, 0}, cross(xx, V3{0, -1, 0}));  // local y = -y world (launch direction), local z = -z world
    w.q[0] = wq.x; w.q[1] = wq.y; w.q[2] = wq.z; w.q[3] = wq.w;
    L.wall_n[0] = 0; L.wall_n[1] = 1; L.wall_n[2] = 0;  // normal pointing at the oncoming limb (+y)
    for (int k = 0; k < 3; ++k) L.wall_c[k] = w.c[k];
    L.wall_th = thick;
    P("[case] wall: pitch omega %.5f rad/s => tip speed %.2f m/s (rho %.3f m); tip launch direction (%.3f %.3f %.3f) (expected about -y); wall front face at y %.3f m (80 ms ahead of the leading arm corner along -y at tip speed), 5 cm thick, x window %.2f..%.2f m (arm x %.2f..%.2f, torso side face at %.2f; frontmost other body at y %.2f), half height %.1f m; full width (clear of all other bodies): %s",
      use_om, c.tip_speed, rho, vdu.x, vdu.y, vdu.z, w.c[1] + thick, xlo, xhi, xmin, xmax, torso_max_x, other_ymin, ext_z, full_width ? "yes" : "NO, narrowed window");
    // debris behind the wall (-y side): 10 x 10 x 5 lattice, spacing 3 box sizes, 1 box size behind the back face
    const artic_body::Body& hd = M.b[size_t(M.hand[0])];
    debris_size(hd.mass);
    const double d = L.debris_half, sp3 = 6.0 * d;
    const double back_y = w.c[1] - thick;  // back face (-y side)
    int n = 0;
    for (int ky = 0; ky < 5 && n < c.ndebris; ++ky)
      for (int kz = 0; kz < 10 && n < c.ndebris; ++kz)
        for (int kx = 0; kx < 10 && n < c.ndebris; ++kx) {
          push_debris(w.c[0] + (kx - 4.5) * sp3, back_y - 2.0 * d - ky * sp3, tip.z + (kz - 4.5) * sp3);
          ++n;
        }
    P("[case] debris: %zu cubes half %.4f m, mass %.4g kg each = hand mass %.4g kg / %.0f (contact mass ratio hand link : debris body = %.0f:1), density %.0f kg/m^3 (assumption); 10x10x5 lattice (spacing %.3f m) behind the wall around the tip height; they start in the air and fall",
      L.sc.debris.size(), d, L.debris_mass, hd.mass, c.ratio, c.ratio, c.density, sp3);
  } else {
    std::fprintf(stderr, "unknown --case %s\n", c.kase.c_str());
    return false;
  }
  return true;
}

std::vector<double> targets_at(const Layout& L, double t) {
  const size_t n = L.keys.size();
  if (n == 1 || t <= L.key_t.front()) return L.keys.front();
  if (t >= L.key_t.back()) return L.keys.back();
  size_t i = 0;
  while (i + 1 < n && t > L.key_t[i + 1]) ++i;
  double u = (t - L.key_t[i]) / (L.key_t[i + 1] - L.key_t[i]);
  u = u * u * (3.0 - 2.0 * u);  // smoothstep
  std::vector<double> r(L.keys[i].size());
  for (size_t k = 0; k < r.size(); ++k) r[k] = L.keys[i][k] + (L.keys[i + 1][k] - L.keys[i][k]) * u;
  return r;
}

// ---------------------------------------------------------------- run and metrics
struct Sample { double t; int awake; double torso_z, drift, anchor_max, ang_max; };
struct RunOut {
  std::string backend, notes;
  int steps_done = 0, steps_planned = 0;
  double p50 = 0, p99 = 0, pmax = 0, mean = 0;
  int onset_nonfinite = 0, onset_anchor_001 = 0, onset_anchor_01 = 0, onset_anchor_1 = 0, onset_axis_01 = 0;
  double anchor_max = 0, axis_max = 0, pd_err_max = 0, pd_err_final = 0, limit_excess_max = 0;
  double ground_pen_max = 0, wall_pen_max = 0, wall_pen_final = 0;
  int inside_first_step = 0, inside_first_body = -1;  // first step with a debris centre inside a humanoid box, and which humanoid body
  bool tunnelled = false, tunnelled_corner = false;  // tunnelled: a link centre (wall case, as in the chain-limb test) or the cube centre (impact case) went through; tunnelled_corner: any box corner inside the wall extent is behind the back face
  int debris_below_ground_max = 0, debris_inside_humanoid_max = 0, debris_crossed_wall_final = 0;
  double debris_speed_max = 0, humanoid_speed_max = 0, humanoid_angspeed_max = 0;
  double drift_max = 0, drift_final = 0, torso_z_ratio_min = 1e30, torso_z_ratio_final = 0, tilt_max = 0, tilt_final = 0;
  double proj_speed0 = 0, proj_speed_min = 1e30, proj_y_final = 0;
  int awake_first = 0, awake_last = 0, awake_min = 1 << 30;
  uint64_t hash = 14695981039346656037ull;
  std::vector<Sample> samples;
};

struct HLocal { V3 lp, lc, ap; Q4 rel0; };

RunOut run_once(Backend& be, const Layout& L, const Cli& c) {
  RunOut o;
  const artic_body::Humanoid& M = L.sc.humanoid;
  const size_t nb = M.b.size(), nd = L.sc.debris.size();
  be.build(L.sc);
  o.backend = be.name();
  o.notes = be.notes();
  std::vector<HLocal> hl(nb);
  for (size_t i = 1; i < nb; ++i) {
    const artic_body::Body& b = M.b[i];
    const artic_body::Body& pr = M.b[size_t(b.parent)];
    const V3 an = V_of(b.anchor);
    const Q4 qc{b.q[0], b.q[1], b.q[2], b.q[3]}, qp{pr.q[0], pr.q[1], pr.q[2], pr.q[3]};
    HLocal l;
    l.lc = rot(conj(qc), sub(an, V_of(b.c)));
    l.lp = rot(conj(qp), sub(an, V_of(pr.c)));
    const double nn = nrm(V_of(b.axis));
    l.ap = rot(conj(qp), V3{b.axis[0] / nn, b.axis[1] / nn, b.axis[2] / nn});
    l.rel0 = mul(conj(qp), qc);
    hl[i] = l;
  }
  const double dt = 1.0 / c.hz;
  const int steps = static_cast<int>(std::lround(L.duration * c.hz));
  o.steps_planned = steps;
  const V3 torso0 = V_of(M.b[0].c);
  std::vector<double> times;
  times.reserve(size_t(steps));
  std::vector<State> st(nb), ds(nd);
  double next_sample = 0.0;
  o.proj_speed0 = L.sc.has_proj ? nrm(V_of(L.sc.proj.v)) : 0.0;
  for (int s = 0; s < steps; ++s) {
    const double t = s * dt;
    be.set_targets(targets_at(L, t));
    const auto t0 = std::chrono::steady_clock::now();
    be.step(static_cast<float>(dt));
    const auto t1 = std::chrono::steady_clock::now();
    times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    o.steps_done = s + 1;
    const int step1 = s + 1;
    bool nonfin = false;
    for (size_t i = 0; i < nb; ++i) { st[i] = be.humanoid_body(int(i)); fnv(o.hash, &st[i], sizeof(State)); if (!finite_state(st[i])) nonfin = true; }
    for (size_t i = 0; i < nd; ++i) { ds[i] = be.debris_body(int(i)); fnv(o.hash, &ds[i], sizeof(State)); if (!finite_state(ds[i])) nonfin = true; }
    State pj{};
    if (L.sc.has_proj) { pj = be.proj_body(); fnv(o.hash, &pj, sizeof(State)); if (!finite_state(pj)) nonfin = true; }
    if (nonfin) { o.onset_nonfinite = step1; if (!o.onset_anchor_001) o.onset_anchor_001 = step1; if (!o.onset_anchor_01) o.onset_anchor_01 = step1; if (!o.onset_anchor_1) o.onset_anchor_1 = step1; if (!o.onset_axis_01) o.onset_axis_01 = step1; break; }
    // ---- joints
    const std::vector<double> tg = targets_at(L, t);
    double step_anchor = 0, step_ang = 0;
    for (size_t k = 1; k < nb; ++k) {
      const artic_body::Body& b = M.b[k];
      const State& c1 = st[k];
      const State& p1 = st[size_t(b.parent)];
      const V3 pc = p_of(c1), pp = p_of(p1);
      const Q4 qc = q_of(c1), qp = q_of(p1);
      const V3 rc = rot(qc, hl[k].lc), rp = rot(qp, hl[k].lp);
      const V3 ac = add(pc, rc), ap = add(pp, rp);
      const double e = nrm(sub(ap, ac));
      step_anchor = std::max(step_anchor, e);
      o.anchor_max = std::max(o.anchor_max, e);
      if (e > 0.01 && !o.onset_anchor_001) o.onset_anchor_001 = step1;
      if (e > 0.1 && !o.onset_anchor_01) o.onset_anchor_01 = step1;
      if (e > 1.0 && !o.onset_anchor_1) o.onset_anchor_1 = step1;
      const Q4 rel = mul(conj(qp), qc);
      const Q4 Rr = mul(rel, conj(hl[k].rel0));
      double ax_err, ang = 0;
      if (b.jtype == 2) {  // fixed: whole relative rotation deviation
        ax_err = 2.0 * std::acos(std::min(1.0, std::fabs(Rr.w)));
      } else {
        const V3 axp = rot(qp, hl[k].ap);
        const V3 axc = rot(qc, rot(conj(hl[k].rel0), hl[k].ap));
        ax_err = std::acos(std::max(-1.0, std::min(1.0, dot(axp, axc))));
        ang = wrap_pi(2.0 * std::atan2(Rr.x * hl[k].ap.x + Rr.y * hl[k].ap.y + Rr.z * hl[k].ap.z, Rr.w));
        o.limit_excess_max = std::max(o.limit_excess_max, std::max(ang - b.hi, b.lo - ang));
        if (b.drive) step_ang = std::max(step_ang, std::fabs(ang - tg[k]));
      }
      o.axis_max = std::max(o.axis_max, ax_err);
      if (ax_err > 0.1 && !o.onset_axis_01) o.onset_axis_01 = step1;
    }
    o.pd_err_max = std::max(o.pd_err_max, step_ang);
    o.pd_err_final = step_ang;
    // ---- ground penetration, speeds
    for (size_t i = 0; i < nb; ++i) {
      const Q4 q = q_of(st[i]);
      for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) {
        const V3 r = rot(q, V3{sx * M.b[i].half[0], sy * M.b[i].half[1], sz * M.b[i].half[2]});
        o.ground_pen_max = std::max(o.ground_pen_max, -(st[i].p[2] + r.z));
      }
      o.humanoid_speed_max = std::max(o.humanoid_speed_max, nrm(V3{st[i].v[0], st[i].v[1], st[i].v[2]}));
      o.humanoid_angspeed_max = std::max(o.humanoid_angspeed_max, nrm(V3{st[i].w[0], st[i].w[1], st[i].w[2]}));
    }
    // ---- drift, height, tilt of the torso
    const V3 tp = p_of(st[0]);
    const double drift = std::hypot(tp.x - torso0.x, tp.y - torso0.y);
    o.drift_max = std::max(o.drift_max, drift);
    o.drift_final = drift;
    const double zr = tp.z / torso0.z;
    o.torso_z_ratio_min = std::min(o.torso_z_ratio_min, zr);
    o.torso_z_ratio_final = zr;
    const double tilt = std::acos(std::max(-1.0, std::min(1.0, rot(q_of(st[0]), V3{0, 0, 1}).z)));
    o.tilt_max = std::max(o.tilt_max, tilt);
    o.tilt_final = tilt;
    // ---- debris
    int below = 0, inside = 0, awake = 0, crossed = 0;
    double step_wall_pen = 0;
    for (size_t i = 0; i < nd; ++i) {
      const V3 dp = p_of(ds[i]);
      if (dp.z < 0.0) ++below;
      o.debris_speed_max = std::max(o.debris_speed_max, nrm(V3{ds[i].v[0], ds[i].v[1], ds[i].v[2]}));
      if (be.debris_awake(int(i))) ++awake;
      for (size_t k = 0; k < nb; ++k) {  // centre inside a humanoid box (deep interpenetration)
        const V3 r = rot(conj(q_of(st[k])), sub(dp, p_of(st[k])));
        if (std::fabs(r.x) < M.b[k].half[0] && std::fabs(r.y) < M.b[k].half[1] && std::fabs(r.z) < M.b[k].half[2]) { ++inside; if (!o.inside_first_step) { o.inside_first_step = step1; o.inside_first_body = int(k); } break; }
      }
      if (L.wall_case) {
        const V3 nw{L.wall_n[0], L.wall_n[1], L.wall_n[2]}, cw{L.wall_c[0], L.wall_c[1], L.wall_c[2]};
        if (dot(sub(dp, cw), nw) > L.wall_th) ++crossed;  // debris centre on the wall's front side (it started behind)
      }
    }
    o.debris_below_ground_max = std::max(o.debris_below_ground_max, below);
    o.debris_inside_humanoid_max = std::max(o.debris_inside_humanoid_max, inside);
    o.debris_crossed_wall_final = crossed;
    if (s == 0) o.awake_first = awake;
    o.awake_last = awake;
    o.awake_min = std::min(o.awake_min, awake);
    // ---- wall (arm chain only) and projectile
    if (L.wall_case) {
      const V3 nw{L.wall_n[0], L.wall_n[1], L.wall_n[2]}, cw{L.wall_c[0], L.wall_c[1], L.wall_c[2]};
      for (size_t i = size_t(L.arm_first); i < size_t(L.arm_first) + 4 && i < nb; ++i) {
        const Q4 q = q_of(st[i]);
        for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) {
          const V3 r = rot(q, V3{sx * M.b[i].half[0], sy * M.b[i].half[1], sz * M.b[i].half[2]});
          const V3 cp = add(p_of(st[i]), r);
          if (cp.x < L.wall_xlo || cp.x > L.wall_xhi) continue;  // only corners inside the wall's lateral extent can meet it
          const double dpl = dot(sub(cp, cw), nw);
          o.wall_pen_max = std::max(o.wall_pen_max, -(dpl - L.wall_th));  // depth past the front face (> 2 x half thickness = through the slab)
          step_wall_pen = std::max(step_wall_pen, -(dpl - L.wall_th));
          if (dpl < -L.wall_th) o.tunnelled_corner = true;  // a corner inside the wall extent is behind the back face
        }
        const V3 pc = p_of(st[i]);
        if (pc.x >= L.wall_xlo && pc.x <= L.wall_xhi && dot(sub(pc, cw), nw) < -L.wall_th) o.tunnelled = true;  // a link centre behind the back face (the chain-limb test definition)
      }
      o.wall_pen_final = step_wall_pen;
    }
    if (L.sc.has_proj) {
      o.proj_speed_min = std::min(o.proj_speed_min, nrm(V3{pj.v[0], pj.v[1], pj.v[2]}));
      o.proj_y_final = pj.p[1];
      if (pj.p[1] > M.b[0].c[1]) o.tunnelled = true;  // cube centre passed the torso centre plane
    }
    if (t >= next_sample - 1e-9) {
      next_sample += 0.5;
      o.samples.push_back({t, awake, tp.z, drift, o.anchor_max, o.pd_err_max});
    }
  }
  std::vector<double> sorted = times;
  std::sort(sorted.begin(), sorted.end());
  if (!sorted.empty()) {
    o.p50 = sorted[sorted.size() / 2];
    o.p99 = sorted[std::min(sorted.size() - 1, size_t(0.99 * double(sorted.size())))];
    o.pmax = sorted.back();
    double sum = 0;
    for (double v : sorted) sum += v;
    o.mean = sum / double(sorted.size());
  }
  if (o.awake_min == (1 << 30)) o.awake_min = 0;
  if (o.torso_z_ratio_min > 1e29) o.torso_z_ratio_min = 0;
  if (o.proj_speed_min > 1e29) o.proj_speed_min = 0;
  return o;
}

bool parse(int argc, char** argv, Cli& c) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--backend") c.backend = next();
    else if (a == "--case") c.kase = next();
    else if (a == "--size_label") c.size_label = next();
    else if (a == "--hz") c.hz = std::atof(next());
    else if (a == "--ratio") c.ratio = std::atof(next());
    else if (a == "--dur") c.dur = std::atof(next());
    else if (a == "--runs") c.runs = std::atoi(next());
    else if (a == "--debris") c.ndebris = std::atoi(next());
    else if (a == "--density") c.density = std::atof(next());
    else if (a == "--iters") std::sscanf(next(), "%d,%d", &c.vel_iters, &c.pos_iters);
    else if (a == "--ccd") c.ccd = std::string(next()) != "off";
    else if (a == "--ton-kg") c.ton_kg = std::atof(next());
    else if (a == "--fbx") c.fbx = next();
    else if (a == "--sizes-json") c.sizes_json = next();
    else if (a == "--impact-mass") c.impact_mass = std::atof(next());
    else if (a == "--impact-speed") c.impact_speed = std::atof(next());
    else if (a == "--tip-speed") c.tip_speed = std::atof(next());
    else if (a == "--pd-freq") c.rq.pd_freq = std::atof(next());
    else if (a == "--pd-zeta") c.rq.pd_zeta = std::atof(next());
    else if (a == "--stand-mult") c.rq.stand_mult = std::atof(next());
    else if (a == "--leg-cap") c.rq.leg_cap = std::atof(next());
    else if (a == "--out") c.out = next();
    else if (a == "--note") c.note = next();
    else if (a == "--tag") c.tag = next();
    else if (a == "--probe-physx-limits") c.probe = true;
    else if (a == "--dump-skeleton") { return false; }
    else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return false; }
  }
  return true;
}

void json_run(std::FILE* f, const RunOut& r, bool last, double hz) {
  std::fprintf(f, "    {\"backend\": \"%s\", \"notes\": \"%s\", \"steps_planned\": %d, \"steps_done\": %d,\n", jesc(r.backend).c_str(), jesc(r.notes).c_str(), r.steps_planned, r.steps_done);
  std::fprintf(f, "      \"physics_step_ms\": {\"p50\": %s, \"p99\": %s, \"max\": %s, \"mean\": %s},\n", jn(r.p50, "%.4f").c_str(), jn(r.p99, "%.4f").c_str(), jn(r.pmax, "%.4f").c_str(), jn(r.mean, "%.4f").c_str());
  std::fprintf(f, "      \"onset_step_nonfinite\": %d, \"onset_step_anchor_gt_0p01m\": %d, \"onset_step_anchor_gt_0p1m\": %d, \"onset_step_anchor_gt_1m\": %d, \"onset_step_axis_gt_0p1rad\": %d,\n", r.onset_nonfinite,
               r.onset_anchor_001, r.onset_anchor_01, r.onset_anchor_1, r.onset_axis_01);
  std::fprintf(f, "      \"anchor_err_max_m\": %s, \"axis_err_max_rad\": %s, \"pd_angle_err_max_rad\": %s, \"pd_angle_err_final_rad\": %s, \"limit_excess_max_rad\": %s,\n", jn(r.anchor_max).c_str(), jn(r.axis_max).c_str(),
               jn(r.pd_err_max).c_str(), jn(r.pd_err_final).c_str(), jn(r.limit_excess_max).c_str());
  std::fprintf(f, "      \"humanoid_ground_penetration_max_m\": %s, \"wall_penetration_max_m\": %s, \"wall_penetration_final_m\": %s, \"tunnelled\": %s, \"tunnelled_corner\": %s, \"debris_below_ground_max\": %d, \"debris_centre_inside_humanoid_box_max\": %d, \"debris_inside_first_step\": %d, \"debris_inside_first_body\": %d, \"debris_crossed_wall_final\": %d,\n",
               jn(r.ground_pen_max).c_str(), jn(r.wall_pen_max).c_str(), jn(r.wall_pen_final).c_str(), r.tunnelled ? "true" : "false", r.tunnelled_corner ? "true" : "false", r.debris_below_ground_max, r.debris_inside_humanoid_max, r.inside_first_step, r.inside_first_body, r.debris_crossed_wall_final);
  std::fprintf(f, "      \"debris_awake\": {\"first_step\": %d, \"last_step\": %d, \"min\": %d}, \"debris_speed_max_mps\": %s, \"humanoid_speed_max_mps\": %s, \"humanoid_angspeed_max_radps\": %s,\n", r.awake_first, r.awake_last, r.awake_min,
               jn(r.debris_speed_max).c_str(), jn(r.humanoid_speed_max).c_str(), jn(r.humanoid_angspeed_max).c_str());
  std::fprintf(f, "      \"humanoid_drift_max_m\": %s, \"humanoid_drift_final_m\": %s, \"torso_height_ratio_min\": %s, \"torso_height_ratio_final\": %s, \"torso_tilt_max_rad\": %s, \"torso_tilt_final_rad\": %s,\n", jn(r.drift_max).c_str(),
               jn(r.drift_final).c_str(), jn(r.torso_z_ratio_min).c_str(), jn(r.torso_z_ratio_final).c_str(), jn(r.tilt_max).c_str(), jn(r.tilt_final).c_str());
  std::fprintf(f, "      \"proj_speed_start_mps\": %s, \"proj_speed_min_mps\": %s, \"proj_y_final_m\": %s, \"trajectory_hash\": \"%016llx\",\n", jn(r.proj_speed0).c_str(), jn(r.proj_speed_min).c_str(), jn(r.proj_y_final).c_str(),
               static_cast<unsigned long long>(r.hash));
  std::fprintf(f, "      \"samples_every_0p5s\": [");
  for (size_t i = 0; i < r.samples.size(); ++i) {
    const Sample& s = r.samples[i];
    std::fprintf(f, "%s{\"t\": %s, \"debris_awake\": %d, \"torso_z_m\": %s, \"drift_m\": %s, \"anchor_max_so_far_m\": %s, \"pd_err_max_so_far_rad\": %s}", i ? ", " : "", jn(s.t, "%.3f").c_str(), s.awake, jn(s.torso_z).c_str(),
                 jn(s.drift).c_str(), jn(s.anchor_max).c_str(), jn(s.ang_max).c_str());
  }
  std::fprintf(f, "]}%s\n", last ? "" : ",");
  (void)hz;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i)
    if (std::string(argv[i]) == "--dump-skeleton") return artic_body::dump_skeleton(argv[i + 1]);
  Cli c;
  if (!parse(argc, argv, c)) return 2;
  if (c.probe) {
    const std::string r = artic_body::physx_probe_limits();
    std::printf("PhysX articulation probe (reduced coordinate, CPU):\n%s", r.c_str());
    return r.empty() ? 2 : 0;
  }
  Layout L;
  if (!make_layout(c, L)) return 2;
  std::printf("%s", L.report.c_str());
  std::vector<std::pair<std::string, std::unique_ptr<Backend>>> list;
  if (c.backend == "jolt" || c.backend == "both") { auto b = artic_body::make_jolt(); if (b) list.emplace_back("jolt", std::move(b)); else std::fprintf(stderr, "this build has no Jolt backend\n"); }
  if (c.backend == "physx" || c.backend == "both") { auto b = artic_body::make_physx(); if (b) list.emplace_back("physx", std::move(b)); else std::fprintf(stderr, "this build has no PhysX backend\n"); }
  if (list.empty()) return 2;
  std::FILE* jf = nullptr;
  if (!c.out.empty()) jf = std::fopen(c.out.c_str(), "wb");
  if (jf) {
    std::fprintf(jf, "{\n  \"test\": \"full-body\", \"device_class\": \"desktop\", \"tag\": \"%s\", \"timing_note\": \"%s\",\n", jesc(c.tag).c_str(), jesc(c.note).c_str());
    std::fprintf(jf, "  \"scenario\": {\"case\": \"%s\", \"description\": \"%s\", \"size_label\": \"%s\", \"step_hz\": %g, \"duration_s\": %g, \"ccd\": %s, \"threads\": 1, \"bodies\": %zu, \"debris_bodies\": %zu, \"mass_ratio_ref_to_debris\": %g, \"ref_mass_kg\": %s, \"debris_mass_kg\": %s, \"debris_half_m\": %s,\n",
                 jesc(c.kase).c_str(), jesc(L.desc).c_str(), jesc(c.size_label).c_str(), c.hz, L.duration, c.ccd ? "true" : "false", L.sc.humanoid.b.size(), L.sc.debris.size(), c.ratio, jn(L.ref_mass).c_str(), jn(L.debris_mass).c_str(),
                 jn(L.debris_half).c_str());
    std::fprintf(jf, "    \"humanoid_total_mass_kg\": %s, \"scale\": %s, \"runs\": %d, \"pd_freq_hz\": %g, \"pd_zeta\": %g, \"stand_mult\": %g, \"leg_cap\": %g},\n  \"results\": [\n", jn(L.sc.humanoid.total_mass).c_str(), jn(L.sc.humanoid.scale).c_str(), c.runs,
                 c.rq.pd_freq, c.rq.pd_zeta, c.rq.stand_mult, c.rq.leg_cap);
  }
  int rc = 0;
  for (size_t bi = 0; bi < list.size(); ++bi) {
    std::vector<RunOut> outs;
    for (int r = 0; r < c.runs; ++r) {
      outs.push_back(run_once(*list[bi].second, L, c));
      const RunOut& o = outs.back();
      std::printf("[%s run %d] %s\n  notes: %s\n", list[bi].first.c_str(), r + 1, o.backend.c_str(), o.notes.c_str());
      std::printf("  steps %d/%d | step ms p50 %.4f p99 %.4f max %.4f [%s]\n", o.steps_done, o.steps_planned, o.p50, o.p99, o.pmax, c.note.c_str());
      std::printf("  nonfinite onset %d | anchor max %.4g m (>0.01 at %d, >0.1 at %d, >1 at %d) | axis max %.4g rad (>0.1 at %d) | PD err max %.4g final %.4g rad | limit excess %.3g\n", o.onset_nonfinite, o.anchor_max,
                  o.onset_anchor_001, o.onset_anchor_01, o.onset_anchor_1, o.axis_max, o.onset_axis_01, o.pd_err_max, o.pd_err_final, o.limit_excess_max);
      std::printf("  ground pen %.4g m | wall pen %.4g m (final %.4g) | inside-box first step %d body %d | tunnelled(centre) %d corner %d | debris below ground max %d, centre inside humanoid box max %d, crossed wall %d | debris awake first/min/last %d/%d/%d\n", o.ground_pen_max, o.wall_pen_max, o.wall_pen_final, o.inside_first_step, o.inside_first_body,
                  o.tunnelled, o.tunnelled_corner, o.debris_below_ground_max, o.debris_inside_humanoid_max, o.debris_crossed_wall_final, o.awake_first, o.awake_min, o.awake_last);
      std::printf("  drift max %.4g final %.4g m | torso height ratio min %.4f final %.4f | tilt max %.4g final %.4g rad | humanoid v max %.3g w max %.3g | hash %016llx\n", o.drift_max, o.drift_final, o.torso_z_ratio_min,
                  o.torso_z_ratio_final, o.tilt_max, o.tilt_final, o.humanoid_speed_max, o.humanoid_angspeed_max, static_cast<unsigned long long>(o.hash));
      std::fflush(stdout);
    }
    bool det = !outs.empty();
    for (const auto& o : outs) det = det && o.hash == outs[0].hash;
    std::printf("[%s] determinism: %zu runs, trajectory hashes equal: %s\n", list[bi].first.c_str(), outs.size(), det ? "YES" : "NO");
    if (jf) {
      std::fprintf(jf, "   {\"backend_key\": \"%s\", \"deterministic_all_runs_equal\": %s, \"runs\": [\n", list[bi].first.c_str(), det ? "true" : "false");
      for (size_t i = 0; i < outs.size(); ++i) json_run(jf, outs[i], i + 1 == outs.size(), c.hz);
      std::fprintf(jf, "   ]}%s\n", bi + 1 < list.size() ? "," : "");
    }
  }
  if (jf) { std::fprintf(jf, "  ]\n}\n"); std::fclose(jf); }
  return rc;
}
