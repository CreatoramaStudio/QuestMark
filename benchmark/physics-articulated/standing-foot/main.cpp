// Standing-foot test: standing foot with ground contact and body weight. See README.md.
// A leg chain from the humanoid skeleton (chain-limb limb builder, ../chain-limb/limb.cpp) is mirrored to two legs under a free pelvis that carries the upper-body mass,
// standing on a thick ground box. Cases: two legs / one leg x stand / lateral 2 t impact / swing of the free leg. Backends: Jolt, PhysX articulation, PhysX maximal.
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

#include "skeleton.h"
#include "stand.h"

namespace {

// ---------------------------------------------------------------- math
struct V { double x, y, z; };
V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(V a) { return std::sqrt(dot(a, a)); }
V unit(V a) { const double l = norm(a); return l > 0 ? a * (1.0 / l) : a; }
struct Q { double x, y, z, w; };
Q qmul(Q a, Q b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Q qconj(Q q) { return {-q.x, -q.y, -q.z, q.w}; }
Q qaxis(V ax, double a) { const V u = unit(ax) * std::sin(0.5 * a); return {u.x, u.y, u.z, std::cos(0.5 * a)}; }
V qrot(Q q, V v) { const V u{q.x, q.y, q.z}; const V t = cross(u, v) * 2.0; return v + t * q.w + cross(u, t); }
V Vd(const double* d) { return {d[0], d[1], d[2]}; }
Q Qd(const double* d) { return {d[0], d[1], d[2], d[3]}; }
void put(double* d, V v) { d[0] = v.x; d[1] = v.y; d[2] = v.z; }
void putq(double* d, Q q) { d[0] = q.x; d[1] = q.y; d[2] = q.z; d[3] = q.w; }
V Vf(const float* d) { return {d[0], d[1], d[2]}; }
Q Qf(const float* d) { return {d[0], d[1], d[2], d[3]}; }
const double kPi = 3.14159265358979323846;

void fnv(uint64_t& h, const void* p, size_t n) {
  const unsigned char* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}
// JSON has no NaN/Inf: a non-finite double is written as null.
std::string jn(double v, const char* fmt = "%.6g") {
  if (!std::isfinite(v)) return "null";
  char b[64];
  std::snprintf(b, sizeof b, fmt, v);
  return b;
}
std::string jesc(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; if (c == '\n') { o += "\\n"; continue; } o += c; }
  return o;
}

// ---------------------------------------------------------------- configuration
struct Cfg {
  std::string size_label = "h85m", fbx = "", sizes_json = "benchmark/physics-articulated/common/sizes.json";
  std::string backends = "jolt,physx,physx-maximal", cases = "2leg-stand,2leg-impact,1leg-stand,1leg-impact,1leg-swing", out, note, tag;
  double hz = 120, sec = 5.0, kp_mult = 1.0, pd_freq = 2.0, zeta = 1.0, torque_factor = 5.0;
  int vel_iters = -1, pos_iters = -1;  // solver iterations, <0 = library default
  double impact_t = 1.0, impact_speed = 30.0, impact_mass = 2000.0;  // projectile reaches the pelvis at impact_t
  double swing_t = 1.0, swing_amp = 0.3, swing_f = 0.5;               // free-leg hip pitch target: s * amp * (1 - cos(2 pi f (t - swing_t))) / 2
  double clearance_frac = 0.04;                                      // free foot clearance at the lifted pose, fraction of the hip height
  int runs = 2;
};
struct CaseId { std::string stance, what; };  // stance: 2leg | 1leg ; what: stand | impact | swing

std::string json_find_row(const std::string& txt, const std::string& name) {
  const size_t k = txt.find("\"name\": \"" + name + "\"");
  if (k == std::string::npos) return "";
  const size_t b = txt.rfind('{', k), e = txt.find('}', k);
  return txt.substr(b, e - b + 1);
}
bool json_num(const std::string& row, const char* key, double& v) {
  const size_t k = row.find(std::string("\"") + key + "\":");
  if (k == std::string::npos) return false;
  v = std::atof(row.c_str() + k + std::strlen(key) + 3);
  return true;
}

struct Info {
  std::ostringstream rep;
  double W = 0, sc = 0, pelvis_mass = 0, lift_angle = 0, lift_clear = 0, z_lo = 0, z_hi = 0, hip_z = 0, hip_x = 0, ground_z = 0, swing_sign = 0;
  int free_hinge1 = -1;                       // hinge index of the free leg hip pitch (-1 = none)
  std::vector<std::string> hinge_note;
};

// foot box (any orientation): lowest corner z
double box_min_z(V c, Q q, const double half[3]) {
  double m = 1e300;
  for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2)
    m = std::min(m, (c + qrot(q, V{sx * half[0], sy * half[1], sz * half[2]})).z);
  return m;
}

// downstream inertia, static gravity torque and lever sum of the bodies k0+h .. k0+3 about a hinge (formulas of chain-limb limb.cpp)
void downstream(const artic_foot::Spec& s, int k0, int h, const artic_foot::Hinge& hg, double g, double& I, double& tg, double& lever) {
  const V a = unit(Vd(hg.axis)), an = Vd(hg.anchor);
  I = tg = lever = 0;
  for (int i = k0 + h; i < k0 + 4; ++i) {
    const artic_foot::Body& bd = s.body[size_t(i)];
    const V r = Vd(bd.c) - an;
    const double rp2 = dot(r, r) - dot(r, a) * dot(r, a);
    const Q q = Qd(bd.q);
    const V al = qrot(qconj(q), a);
    const double hx = bd.half[0], hy = bd.half[1], hz = bd.half[2];
    const double Ix = bd.mass / 3.0 * (hy * hy + hz * hz), Iy = bd.mass / 3.0 * (hx * hx + hz * hz), Iz = bd.mass / 3.0 * (hx * hx + hy * hy);
    I += Ix * al.x * al.x + Iy * al.y * al.y + Iz * al.z * al.z + bd.mass * rp2;
    tg += dot(a, cross(r, V{0, 0, -bd.mass * g}));
    lever += bd.mass * g * std::sqrt(rp2);
  }
}

bool build_spec(const Cfg& cf, const CaseId& cs, artic_foot::Spec& sp, Info& in, std::string& err) {
  using namespace artic_foot;
  artic::LimbRequest rq;
  rq.fbx = cf.fbx; rq.limb = "leg"; rq.sizes_json = cf.sizes_json; rq.size_label = cf.size_label; rq.limb_case = "hold"; rq.pd_freq = cf.pd_freq; rq.pd_zeta = 0; rq.torque_factor = cf.torque_factor;
  rq.pd_zeta = cf.zeta;
  artic::LimbSpec L;
  std::string rep;
  if (!artic::build_limb(rq, L, rep)) { err = rep; return false; }
  in.rep << rep;
  // scale factor and file z range are read back from the chain-limb report text (the builder does not export them)
  {
    const size_t k = rep.find("scale factor to the target size: ");
    if (k == std::string::npos) { err = "scale factor not found in the limb report"; return false; }
    in.sc = std::atof(rep.c_str() + k + 27);
    const size_t z = rep.find("(z ");
    if (z == std::string::npos) { err = "z range not found in the limb report"; return false; }
    char* e = nullptr;
    in.z_lo = std::strtod(rep.c_str() + z + 3, &e);
    in.z_hi = std::strtod(e + 4, nullptr);  // " .. "
  }
  {
    std::string gj;
    { std::ifstream f(cf.sizes_json, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); gj = ss.str(); }
    const std::string row = json_find_row(gj, cf.size_label);
    double wt = 0;
    if (row.empty() || !json_num(row, "weight_tons", wt)) { err = "size_label weight not found"; return false; }
    in.W = wt * 1000.0;
  }
  const double sc = in.sc, g = cf.pd_freq > 0 ? 9.81 : 9.81;
  const double zg = in.z_lo * sc;  // ground top = lowest point of the humanoid skin at the file pose
  in.ground_z = zg;
  // ---- flat foot box replacing the chain-limb foot box (which is tilted about 29 deg in the file pose: it would stand on one edge)
  {
    artic::LimbBody& f = L.body[3];
    const Q q = Qd(f.q);
    double ext[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
      const V col = qrot(q, V{i == 0 ? 1.0 : 0.0, i == 1 ? 1.0 : 0.0, i == 2 ? 1.0 : 0.0});
      ext[0] += std::fabs(col.x) * f.half[i]; ext[1] += std::fabs(col.y) * f.half[i];
    }
    const double ymin = f.c[1] - ext[1], ymax = f.c[1] + ext[1], ztop = L.hinge[3].anchor[2];
    in.rep << "[stand] chain-limb foot box replaced by a flat box: x half " << ext[0] << ", y " << ymin << " .. " << ymax << ", z " << zg << " .. " << ztop << " (AABB of the chain-limb foot box in x and y; top = ankle anchor height; sole at ground)\n";
    f.c[0] = f.c[0]; f.c[1] = 0.5 * (ymin + ymax); f.c[2] = 0.5 * (zg + ztop);
    putq(f.q, {0, 0, 0, 1});
    f.half[0] = ext[0]; f.half[1] = 0.5 * (ymax - ymin); f.half[2] = 0.5 * (ztop - zg);
  }
  // ---- spec: body 0 pelvis, 1..4 left leg, 5..8 right leg (mirror about x = 0; the file has UpperLeg_R at -x of UpperLeg_L, checked with --dump-skeleton)
  Spec s;
  s.gravity = g;
  s.ccd = true;
  s.vel_iters = cf.vel_iters; s.pos_iters = cf.pos_iters;
  s.body.resize(9);
  double leg_mass = 0;
  for (const artic::LimbBody& b : L.body) leg_mass += b.mass;
  const double hipx = L.hinge[0].anchor[0], hipz = L.hinge[0].anchor[2], ankle_y = L.hinge[3].anchor[1];
  in.hip_x = hipx; in.hip_z = hipz;
  const double top = in.z_hi * sc;
  const double pm = in.W - 2.0 * leg_mass;
  in.pelvis_mass = pm;
  const std::string nm[4] = {"gimbal", "thigh", "shank", "foot"};
  for (int leg = 0; leg < 2; ++leg) {
    const double sx = leg == 0 ? 1.0 : -1.0;
    for (int i = 0; i < 4; ++i) {
      const artic::LimbBody& b = L.body[size_t(i)];
      Body& d = s.body[size_t(1 + 4 * leg + i)];
      d.name = std::string(leg == 0 ? "L-" : "R-") + nm[i];
      d.c[0] = sx * b.c[0]; d.c[1] = b.c[1]; d.c[2] = b.c[2];
      d.q[0] = b.q[0]; d.q[1] = leg == 0 ? b.q[1] : -b.q[1]; d.q[2] = leg == 0 ? b.q[2] : -b.q[2]; d.q[3] = b.q[3];  // reflection x -> -x of a rotation: (qx, -qy, -qz, qw)
      for (int k = 0; k < 3; ++k) d.half[k] = b.half[k];
      d.mass = b.mass;
    }
    for (int i = 0; i < 4; ++i) {
      const artic::LimbHinge& h = L.hinge[size_t(i)];
      Hinge d{};
      d.name = std::string(leg == 0 ? "L-" : "R-") + h.name;
      d.parent = i == 0 ? 0 : 1 + 4 * leg + i - 1;
      d.child = 1 + 4 * leg + i;
      d.anchor[0] = sx * h.anchor[0]; d.anchor[1] = h.anchor[1]; d.anchor[2] = h.anchor[2];
      d.axis[0] = sx * h.axis[0]; d.axis[1] = h.axis[1]; d.axis[2] = h.axis[2];
      d.lo = h.lo; d.hi = h.hi;
      s.hinge.push_back(d);
    }
  }
  const bool one = cs.stance == "1leg";
  const int stance_leg = 0, free_leg = 1;
  // ---- free leg lifted (one-leg stance): rotate everything below the hip pitch hinge about it, as chain-limb swing does for its raise; the PD target stays 0 at this start pose
  if (one) {
    const int hb = 4 * free_leg, k0 = 1 + 4 * free_leg;
    const V an = Vd(s.hinge[size_t(hb + 1)].anchor), ax = Vd(s.hinge[size_t(hb + 1)].axis);
    double best = 0, bsign = 0, clr = 0;
    for (int step = 1; step <= 40 && best == 0; ++step) {
      for (double sg : {1.0, -1.0}) {
        const double th = sg * 0.05 * step;
        const Q rr = qaxis(ax, th);
        const Body& f = s.body[size_t(k0 + 3)];
        const double mz = box_min_z(an + qrot(rr, Vd(f.c) - an), qmul(rr, Qd(f.q)), f.half);
        if (mz - zg >= cf.clearance_frac * (hipz - zg)) { best = th; bsign = sg; clr = mz - zg; break; }
      }
    }
    if (best == 0) { err = "no lift angle found for the free leg"; return false; }
    const Q rr = qaxis(ax, best);
    for (int i = k0 + 1; i < k0 + 4; ++i) {
      Body& b = s.body[size_t(i)];
      put(b.c, an + qrot(rr, Vd(b.c) - an));
      putq(b.q, qmul(rr, Qd(b.q)));
    }
    for (int h = hb + 2; h < hb + 4; ++h) {
      put(s.hinge[size_t(h)].anchor, an + qrot(rr, Vd(s.hinge[size_t(h)].anchor) - an));
      put(s.hinge[size_t(h)].axis, qrot(rr, Vd(s.hinge[size_t(h)].axis)));
    }
    s.hinge[size_t(hb + 1)].lo -= best; s.hinge[size_t(hb + 1)].hi -= best;
    in.lift_angle = best; in.swing_sign = bsign; in.lift_clear = clr; in.free_hinge1 = hb + 1;
    in.rep << "[stand] free (right) leg lifted: hip pitch rotated by " << best << " rad (joint-angle sign of its axis); lowest foot corner " << clr << " m above the ground (>= " << cf.clearance_frac << " x hip height " << hipz - zg << " m)\n";
  }
  // ---- pelvis: one box carrying everything that is not in the two legs
  {
    Body& p = s.body[0];
    p.name = "pelvis";
    const double hz = 0.5 * (top - hipz);
    p.half[0] = 1.6 * hipx; p.half[1] = 1.0 * hipx; p.half[2] = hz;
    double cx = 0;
    if (one) {  // system centre of mass (pelvis + free leg) above the stance hip/foot in x
      double sm = 0;
      for (int i = 1 + 4 * free_leg; i < 5 + 4 * free_leg; ++i) sm += s.body[size_t(i)].mass * (s.body[size_t(i)].c[0] - hipx);
      cx = hipx - sm / pm;
    }
    p.c[0] = cx; p.c[1] = ankle_y; p.c[2] = hipz + hz;
    putq(p.q, {0, 0, 0, 1});
    p.mass = pm;
    in.rep << "[stand] mass: size_label " << in.W << " kg = pelvis " << pm << " kg (everything outside the two legs: " << 100.0 * pm / in.W << " %) + 2 x leg bodies " << leg_mass << " kg (chain-limb volume shares: thigh, shank, foot from bone-AABB volumes + gimbal 10% of thigh)\n";
    in.rep << "[stand] pelvis box: half " << p.half[0] << " x " << p.half[1] << " x " << p.half[2] << " m, centre (" << p.c[0] << ", " << p.c[1] << ", " << p.c[2] << "); height from the hip joint to the top of the humanoid skin; width 1.6 x hip offset, depth 1.0 x hip offset are ASSUMPTIONS; centre y = ankle y\n";
  }
  // ---- ground
  s.ground_top = zg;
  {
    double height = 0;
    { std::string gj; std::ifstream f(cf.sizes_json, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); gj = ss.str(); json_num(json_find_row(gj, cf.size_label), "height_m", height); }
    s.ground_half[0] = s.ground_half[1] = 2.0 * height; s.ground_half[2] = 5.0;
    s.ground_c[0] = 0; s.ground_c[1] = 0; s.ground_c[2] = zg - 5.0;
    in.rep << "[stand] ground box half " << s.ground_half[0] << " x " << s.ground_half[1] << " x 5 m (10 m thick), top z " << zg << ", friction " << s.friction << "\n";
  }
  s.foot = {4, 8};
  s.foot_stance = {true, !one};
  // ---- impact body
  if (cs.what == "impact") {
    s.proj = true;
    s.proj_half[0] = s.proj_half[1] = s.proj_half[2] = 0.5;
    s.proj_mass = cf.impact_mass;
    const Body& p = s.body[0];
    s.proj_c[0] = p.c[0] - p.half[0] - 0.5 - cf.impact_speed * cf.impact_t; s.proj_c[1] = p.c[1]; s.proj_c[2] = p.c[2];
    s.proj_v[0] = cf.impact_speed;
    in.rep << "[stand] impact: " << cf.impact_mass << " kg cube (1 m) flying along +x (lateral) at " << cf.impact_speed << " m/s, gravity off for it, aimed at the pelvis centre, reaches the pelvis face at t = " << cf.impact_t << " s\n";
  }
  // ---- PD gains (chain-limb rule: kp = I (2 pi f)^2, kd = 2 zeta sqrt(kp I)); torque cap raised for the weight a stance leg carries
  const double w0 = 2.0 * kPi * cf.pd_freq;
  for (int leg = 0; leg < 2; ++leg) {
    const bool stance = leg == stance_leg || !one;
    const double share = one ? 1.0 : 0.5;
    const double Fleg = stance ? share * in.W * g : 0.0;
    const V cop{s.body[size_t(4 + 4 * leg)].c[0], ankle_y, zg};  // assumed centre of pressure: under the ankle, foot centre in x
    for (int h = 0; h < 4; ++h) {
      Hinge& hg = s.hinge[size_t(4 * leg + h)];
      double I, tg, lever;
      downstream(s, 1 + 4 * leg, h, hg, g, I, tg, lever);
      hg.kp = cf.kp_mult * I * w0 * w0;
      hg.kd = 2.0 * cf.zeta * std::sqrt(hg.kp * I);
      double tmax = std::max(cf.torque_factor * std::fabs(tg), 0.25 * lever);
      char nb[256] = "";
      if (stance) {
        const V a = unit(Vd(hg.axis));
        const double tgrf = std::fabs(dot(a, cross(cop - Vd(hg.anchor), V{0, 0, Fleg})));
        const double t2 = std::max(cf.torque_factor * (tgrf + std::fabs(tg)), 0.1 * Fleg * (hipz - zg));
        tmax = std::max(tmax, t2);
        std::snprintf(nb, sizeof nb, "stance leg: chain-limb cap %.4g, ground-reaction static torque %.4g, cap raised to %.4g N m (5 x (grf + gravity), floor 0.1 x F x hip height)", std::max(cf.torque_factor * std::fabs(tg), 0.25 * lever), tgrf, tmax);
      } else if (h == 1) {
        const double alpha = cf.swing_amp * (2.0 * kPi * cf.swing_f) * (2.0 * kPi * cf.swing_f);
        const double t2 = 2.0 * I * alpha;
        std::snprintf(nb, sizeof nb, "free leg hip pitch: chain-limb cap %.4g, swing inertial torque 2 I alpha = %.4g, cap = max", tmax, t2);
        tmax = std::max(tmax, t2);
      } else std::snprintf(nb, sizeof nb, "free leg: chain-limb cap");
      hg.max_torque = tmax;
      in.hinge_note.push_back(nb);
      in.rep << "[stand] hinge " << hg.name << ": downstream inertia " << I << " kg m^2, gravity torque " << tg << ", kp " << hg.kp << " kd " << hg.kd << " max torque " << hg.max_torque << " (" << nb << ")\n";
    }
  }
  s.label = cf.size_label + " " + cs.stance + "-" + cs.what;
  sp = s;
  return true;
}

// ---------------------------------------------------------------- run + metrics
struct Run {
  bool nonfinite = false, upright = true;
  int steps_done = 0, steps_planned = 0, onset_nonfinite = 0, onset_anchor_0p01 = 0, onset_anchor_0p1 = 0, onset_anchor_1 = 0, onset_axis_0p1 = 0, onset_pen_0p1 = 0;
  double anchor_max = 0, axis_max = 0, limit_excess = 0, slip_max = 0, pen_max = 0, gap_max = 0, sag_end = 0, sag_max = 0, tilt_max = 0, tilt_end = 0, pelvis_dx_end = 0, pelvis_dx_max = 0, pelvis_dy_max = 0,
         pd_err_max = 0, pd_err_end = 0, free_clear_min = 1e300, free_clear_max = -1e300, free_ang_min = 1e300, free_ang_max = -1e300, free_target_max = 0, free_track_err_max = 0, max_speed = 0,
         proj_vx_min = 1e300, proj_vx_end = 0, pelvis_vx_max = 0, step_ms_mean = 0, z0 = 0;
  uint64_t hash = 14695981039346656037ull;
};

struct HLocal { V lp, lc, ap; Q rel0; };

Run run_once(artic_foot::Backend& be, const artic_foot::Spec& s, const Cfg& cf, const CaseId& cs, const Info& in) {
  using namespace artic_foot;
  Run r;
  const int n = int(s.body.size());
  const double dt = 1.0 / cf.hz;
  r.steps_planned = int(std::lround(cf.sec * cf.hz));
  std::vector<HLocal> hl;
  for (const Hinge& h : s.hinge) {
    const Body& p = s.body[size_t(h.parent)];
    const Body& c = s.body[size_t(h.child)];
    HLocal l;
    const V an = Vd(h.anchor);
    l.lc = qrot(qconj(Qd(c.q)), an - Vd(c.c));
    l.lp = qrot(qconj(Qd(p.q)), an - Vd(p.c));
    l.ap = qrot(qconj(Qd(p.q)), unit(Vd(h.axis)));
    l.rel0 = qmul(qconj(Qd(p.q)), Qd(c.q));
    hl.push_back(l);
  }
  be.build(s);
  const double z0 = s.body[0].c[2];
  r.z0 = z0;
  std::vector<V> foot0;
  for (int f : s.foot) foot0.push_back(Vd(s.body[size_t(f)].c));
  std::vector<State> st(size_t(n + (s.proj ? 1 : 0)));
  double ms_sum = 0;
  for (int step = 0; step < r.steps_planned; ++step) {
    const double t = step * dt;
    if (cs.what == "swing" && in.free_hinge1 >= 0 && t >= cf.swing_t) {
      const double tgt = in.swing_sign * cf.swing_amp * 0.5 * (1.0 - std::cos(2.0 * kPi * cf.swing_f * (t - cf.swing_t)));
      be.drive_target(in.free_hinge1, tgt);
    }
    const auto t0 = std::chrono::steady_clock::now();
    be.step(float(dt));
    const auto t1 = std::chrono::steady_clock::now();
    ms_sum += std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.steps_done = step + 1;
    if (std::getenv("FOOT_TRACE") && (step + 1) % int(cf.hz / 2) == 0) { const State q0 = be.body(0); std::fprintf(stderr, "[trace] t %.2f pelvis z %.3f (sag %.3f) x %.3f vz %.3f\n", (step + 1) * dt, q0.p[2], z0 - q0.p[2], q0.p[0], q0.v[2]); }
    bool nf = false;
    for (size_t i = 0; i < st.size(); ++i) {
      st[i] = be.body(int(i));
      fnv(r.hash, &st[i], sizeof(State));
      for (float f : st[i].p) if (!std::isfinite(f)) nf = true;
      for (float f : st[i].q) if (!std::isfinite(f)) nf = true;
      for (float f : st[i].v) if (!std::isfinite(f)) nf = true;
      for (float f : st[i].w) if (!std::isfinite(f)) nf = true;
    }
    const int sn = step + 1;
    auto mark = [&](int& o) { if (o == 0) o = sn; };
    if (nf) { r.nonfinite = true; mark(r.onset_nonfinite); mark(r.onset_anchor_0p01); mark(r.onset_anchor_0p1); mark(r.onset_anchor_1); mark(r.onset_axis_0p1); break; }
    // hinges
    for (size_t k = 0; k < s.hinge.size(); ++k) {
      const Hinge& h = s.hinge[k];
      const HLocal& l = hl[k];
      const State& c = st[size_t(h.child)];
      const State& p = st[size_t(h.parent)];
      const Q qc = Qf(c.q), qp = Qf(p.q);
      const V ac = Vf(c.p) + qrot(qc, l.lc), ap = Vf(p.p) + qrot(qp, l.lp);
      const double e = norm(ap - ac);
      r.anchor_max = std::max(r.anchor_max, e);
      if (e > 0.01) mark(r.onset_anchor_0p01);
      if (e > 0.1) mark(r.onset_anchor_0p1);
      if (e > 1.0) mark(r.onset_anchor_1);
      const V axp = qrot(qp, l.ap), axc = qrot(qc, qrot(qconj(l.rel0), l.ap));
      const double d = std::max(-1.0, std::min(1.0, dot(axp, axc)));
      const double ae = std::acos(d);
      r.axis_max = std::max(r.axis_max, ae);
      if (ae > 0.1) mark(r.onset_axis_0p1);
      const Q rel = qmul(qconj(qp), qc);
      const Q Rr = qmul(rel, qconj(l.rel0));
      double ang = 2.0 * std::atan2(Rr.x * l.ap.x + Rr.y * l.ap.y + Rr.z * l.ap.z, Rr.w);
      while (ang > kPi) ang -= 2 * kPi;
      while (ang < -kPi) ang += 2 * kPi;
      r.limit_excess = std::max(r.limit_excess, std::max(ang - h.hi, h.lo - ang));
      if (int(k) == in.free_hinge1) {
        r.free_ang_min = std::min(r.free_ang_min, ang); r.free_ang_max = std::max(r.free_ang_max, ang);
        if (cs.what == "swing" && t >= cf.swing_t) {
          const double tgt = in.swing_sign * cf.swing_amp * 0.5 * (1.0 - std::cos(2.0 * kPi * cf.swing_f * (t - cf.swing_t)));
          r.free_target_max = std::max(r.free_target_max, std::fabs(tgt));
          r.free_track_err_max = std::max(r.free_track_err_max, std::fabs(ang - tgt));
        }
      } else {
        const bool stance_h = (k < 4) || !(cs.stance == "1leg");
        if (stance_h) { r.pd_err_max = std::max(r.pd_err_max, std::fabs(ang)); if (step == r.steps_planned - 1) r.pd_err_end = std::max(r.pd_err_end, std::fabs(ang)); }
      }
    }
    // feet
    for (size_t f = 0; f < s.foot.size(); ++f) {
      const Body& b = s.body[size_t(s.foot[f])];
      const State& c = st[size_t(s.foot[f])];
      const double mz = box_min_z(Vf(c.p), Qf(c.q), b.half);
      if (s.foot_stance[f]) {
        const double pen = s.ground_top - mz;
        r.pen_max = std::max(r.pen_max, pen);
        if (pen > 0.1) mark(r.onset_pen_0p1);
        r.gap_max = std::max(r.gap_max, -pen);
        r.slip_max = std::max(r.slip_max, std::hypot(c.p[0] - foot0[f].x, c.p[1] - foot0[f].y));
      } else {
        r.free_clear_min = std::min(r.free_clear_min, mz - s.ground_top);
        r.free_clear_max = std::max(r.free_clear_max, mz - s.ground_top);
      }
    }
    // pelvis
    {
      const State& p = st[0];
      const double sag = z0 - p.p[2];
      r.sag_max = std::max(r.sag_max, sag);
      const V up = qrot(Qf(p.q), V{0, 0, 1});
      const double tilt = std::acos(std::max(-1.0, std::min(1.0, up.z)));
      r.tilt_max = std::max(r.tilt_max, tilt);
      r.pelvis_dx_max = std::max(r.pelvis_dx_max, std::fabs(p.p[0] - s.body[0].c[0]));
      r.pelvis_dy_max = std::max(r.pelvis_dy_max, std::fabs(p.p[1] - s.body[0].c[1]));
      r.pelvis_vx_max = std::max(r.pelvis_vx_max, double(std::fabs(p.v[0])));
      if (step == r.steps_planned - 1) { r.sag_end = sag; r.tilt_end = tilt; r.pelvis_dx_end = p.p[0] - s.body[0].c[0]; }
    }
    for (int i = 0; i < n; ++i) r.max_speed = std::max(r.max_speed, norm(Vf(st[size_t(i)].v)));
    if (s.proj) { r.proj_vx_min = std::min(r.proj_vx_min, double(st[size_t(n)].v[0])); r.proj_vx_end = st[size_t(n)].v[0]; }
  }
  r.step_ms_mean = r.steps_done ? ms_sum / r.steps_done : 0;
  if (r.nonfinite) { r.sag_end = r.tilt_end = NAN; }
  r.upright = !r.nonfinite && r.tilt_max < 0.1 && r.sag_end < 0.1 * z0 && r.steps_done == r.steps_planned;
  return r;
}

void json_run(std::FILE* f, const Run& r, bool last) {
  std::fprintf(f,
               "      {\"nonfinite\": %s, \"onset_nonfinite_step\": %d, \"steps_done\": %d, \"steps_planned\": %d,\n"
               "       \"anchor_err_max_m\": %s, \"axis_err_max_rad\": %s, \"joint_limit_excess_max_rad\": %s,\n"
               "       \"onset_anchor_gt_0p01m\": %d, \"onset_anchor_gt_0p1m\": %d, \"onset_anchor_gt_1m\": %d, \"onset_axis_gt_0p1rad\": %d,\n"
               "       \"foot_slip_max_m\": %s, \"foot_penetration_max_m\": %s, \"foot_gap_max_m\": %s, \"onset_penetration_gt_0p1m\": %d,\n"
               "       \"pelvis_z0_m\": %s, \"pelvis_sag_end_m\": %s, \"pelvis_sag_max_m\": %s, \"pelvis_tilt_max_rad\": %s, \"pelvis_tilt_end_rad\": %s,\n"
               "       \"pelvis_dx_max_m\": %s, \"pelvis_dy_max_m\": %s, \"pelvis_dx_end_m\": %s, \"pelvis_vx_max_mps\": %s, \"max_body_speed_mps\": %s,\n"
               "       \"stance_pd_angle_err_max_rad\": %s, \"stance_pd_angle_err_end_rad\": %s,\n"
               "       \"free_foot_clearance_min_m\": %s, \"free_foot_clearance_max_m\": %s, \"free_hip_angle_min_rad\": %s, \"free_hip_angle_max_rad\": %s, \"free_hip_target_max_rad\": %s, \"free_hip_track_err_max_rad\": %s,\n"
               "       \"impact_body_vx_min_mps\": %s, \"impact_body_vx_end_mps\": %s,\n"
               "       \"stays_upright\": %s, \"step_ms_mean_NOT_VALID_machine_was_busy\": %s, \"trajectory_hash\": \"%016llx\"}%s\n",
               r.nonfinite ? "true" : "false", r.onset_nonfinite, r.steps_done, r.steps_planned, jn(r.anchor_max).c_str(), jn(r.axis_max).c_str(), jn(r.limit_excess).c_str(), r.onset_anchor_0p01,
               r.onset_anchor_0p1, r.onset_anchor_1, r.onset_axis_0p1, jn(r.slip_max).c_str(), jn(r.pen_max).c_str(), jn(r.gap_max).c_str(), r.onset_pen_0p1, jn(r.z0).c_str(), jn(r.sag_end).c_str(),
               jn(r.sag_max).c_str(), jn(r.tilt_max).c_str(), jn(r.tilt_end).c_str(), jn(r.pelvis_dx_max).c_str(), jn(r.pelvis_dy_max).c_str(), jn(r.pelvis_dx_end).c_str(), jn(r.pelvis_vx_max).c_str(),
               jn(r.max_speed).c_str(), jn(r.pd_err_max).c_str(), jn(r.pd_err_end).c_str(), jn(r.free_clear_min > 1e299 ? NAN : r.free_clear_min).c_str(),
               jn(r.free_clear_max < -1e299 ? NAN : r.free_clear_max).c_str(), jn(r.free_ang_min > 1e299 ? NAN : r.free_ang_min).c_str(), jn(r.free_ang_max < -1e299 ? NAN : r.free_ang_max).c_str(),
               jn(r.free_target_max).c_str(), jn(r.free_track_err_max).c_str(), jn(r.proj_vx_min > 1e299 ? NAN : r.proj_vx_min).c_str(), jn(r.proj_vx_end).c_str(), r.upright ? "true" : "false",
               jn(r.step_ms_mean, "%.4f").c_str(), static_cast<unsigned long long>(r.hash), last ? "" : ",");
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> o;
  std::stringstream ss(s);
  std::string t;
  while (std::getline(ss, t, ',')) if (!t.empty()) o.push_back(t);
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  Cfg cf;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--size_label") cf.size_label = next();
    else if (a == "--hz") cf.hz = std::atof(next());
    else if (a == "--backend") cf.backends = next();
    else if (a == "--cases") cf.cases = next();
    else if (a == "--runs") cf.runs = std::atoi(next());
    else if (a == "--sec") cf.sec = std::atof(next());
    else if (a == "--kp-mult") cf.kp_mult = std::atof(next());
    else if (a == "--pd-freq") cf.pd_freq = std::atof(next());
    else if (a == "--iters") std::sscanf(next(), "%d,%d", &cf.vel_iters, &cf.pos_iters);  // velocity,position
    else if (a == "--torque-factor") cf.torque_factor = std::atof(next());
    else if (a == "--out") cf.out = next();
    else if (a == "--note") cf.note = next();
    else if (a == "--tag") cf.tag = next();
    else if (a == "--fbx") cf.fbx = next();
    else if (a == "--sizes-json") cf.sizes_json = next();
    else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
  }
  std::FILE* jf = cf.out.empty() ? nullptr : std::fopen(cf.out.c_str(), "wb");
  if (jf) {
    std::fprintf(jf, "{\n  \"test\": \"standing-foot\", \"device_class\": \"desktop\", \"tag\": \"%s\", \"timing_note\": \"%s\",\n", jesc(cf.tag).c_str(), jesc(cf.note).c_str());
    std::fprintf(jf, "  \"size_label\": \"%s\", \"step_hz\": %g, \"episode_sec\": %g, \"runs\": %d, \"kp_mult\": %g, \"pd_freq_hz\": %g, \"torque_factor\": %g,\n", jesc(cf.size_label).c_str(), cf.hz, cf.sec, cf.runs, cf.kp_mult, cf.pd_freq, cf.torque_factor);
    std::fprintf(jf, "  \"thresholds\": {\"anchor_onset_m\": 0.01, \"anchor_explosion_m\": 1.0, \"nonfinite_is_failure\": true, \"trigger_anchor_m\": 0.1, \"trigger_axis_rad\": 0.1, \"upright_tilt_rad\": 0.1, \"upright_sag_fraction_of_pelvis_height\": 0.1},\n  \"cases\": [\n");
  }
  bool printed_rep = false, wrote = false;
  int rc = 0;
  for (const std::string& cname : split(cf.cases)) {
    const size_t dash = cname.find('-');
    CaseId cs{cname.substr(0, dash), cname.substr(dash + 1)};
    artic_foot::Spec spec;
    Info info;
    std::string err;
    if (!build_spec(cf, cs, spec, info, err)) { std::fprintf(stderr, "build_spec(%s): %s\n", cname.c_str(), err.c_str()); rc = 2; continue; }
    if (!printed_rep) { std::printf("%s", info.rep.str().c_str()); printed_rep = true; }
    for (const std::string& bname : split(cf.backends)) {
      std::unique_ptr<artic_foot::Backend> be = bname == "jolt" ? artic_foot::make_jolt() : bname == "physx" ? artic_foot::make_physx() : bname == "physx-maximal" ? artic_foot::make_physx_maximal() : nullptr;
      if (!be) { std::fprintf(stderr, "no backend %s in this build\n", bname.c_str()); continue; }
      std::vector<Run> runs;
      for (int k = 0; k < cf.runs; ++k) runs.push_back(run_once(*be, spec, cf, cs, info));
      bool det = true;
      for (const Run& r : runs) det = det && r.hash == runs[0].hash;
      const Run& r = runs[0];
      std::printf("[%s %s %s %.0fHz] %s\n  nonfinite %d (step %d) steps %d/%d | anchor max %.3e m axis max %.3e rad limit excess %.3e | slip %.4f m pen %.4f m | sag end %.4f m (max %.4f) tilt max %.4f rad | upright %d | det %s\n",
                  cf.size_label.c_str(), bname.c_str(), cname.c_str(), cf.hz, be->name().c_str(), r.nonfinite, r.onset_nonfinite, r.steps_done, r.steps_planned, r.anchor_max, r.axis_max, r.limit_excess, r.slip_max,
                  r.pen_max, r.sag_end, r.sag_max, r.tilt_max, r.upright, det ? "YES" : "NO");
      if (jf) {
        std::fprintf(jf, "%s   {\"backend_key\": \"%s\", \"backend\": \"%s\", \"stance\": \"%s\", \"case\": \"%s\", \"label\": \"%s\", \"deterministic_all_runs_equal\": %s,\n", wrote ? "   ,\n" : "", bname.c_str(),
                     jesc(be->name()).c_str(), cs.stance.c_str(), cs.what.c_str(), jesc(spec.label).c_str(), det ? "true" : "false");
        wrote = true;
        std::fprintf(jf, "    \"scene\": {\"body_mass_kg\": %s, \"pelvis_mass_kg\": %s, \"scale\": %s, \"lift_angle_rad\": %s, \"free_foot_lift_clearance_m\": %s, \"ground_top_z_m\": %s, \"bodies\": [", jn(info.W).c_str(),
                     jn(info.pelvis_mass).c_str(), jn(info.sc).c_str(), jn(info.lift_angle).c_str(), jn(info.lift_clear).c_str(), jn(info.ground_z).c_str());
        for (size_t i = 0; i < spec.body.size(); ++i)
          std::fprintf(jf, "%s{\"name\": \"%s\", \"mass_kg\": %s, \"half_m\": [%s, %s, %s]}", i ? ", " : "", spec.body[i].name.c_str(), jn(spec.body[i].mass).c_str(), jn(spec.body[i].half[0]).c_str(),
                       jn(spec.body[i].half[1]).c_str(), jn(spec.body[i].half[2]).c_str());
        std::fprintf(jf, "], \"hinges\": [");
        for (size_t i = 0; i < spec.hinge.size(); ++i)
          std::fprintf(jf, "%s{\"name\": \"%s\", \"kp\": %s, \"kd\": %s, \"max_torque_nm\": %s, \"note\": \"%s\"}", i ? ", " : "", spec.hinge[i].name.c_str(), jn(spec.hinge[i].kp).c_str(),
                       jn(spec.hinge[i].kd).c_str(), jn(spec.hinge[i].max_torque).c_str(), jesc(info.hinge_note[i]).c_str());
        std::fprintf(jf, "]},\n    \"runs\": [\n");
        for (size_t i = 0; i < runs.size(); ++i) json_run(jf, runs[i], i + 1 == runs.size());
        std::fprintf(jf, "    ]}\n");
      }
    }
  }
  if (jf) { std::fprintf(jf, "  ]\n}\n"); std::fclose(jf); }
  return rc;
}
