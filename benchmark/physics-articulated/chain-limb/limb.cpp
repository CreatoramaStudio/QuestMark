// Chain-limb test: limb builder. Reads a humanoid skeleton with ufbx and derives one limb chain
// (boxes, hinges, masses, case set-up) for the limb scenarios. Every derivation step is printed into the report.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include "skeleton.h"
#include "ufbx.h"

namespace artic {
namespace {

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
Q qaxis(V ax, double a) { const V u = unit(ax) * std::sin(0.5 * a); return {u.x, u.y, u.z, std::cos(0.5 * a)}; }
V qrot(Q q, V v) {
  const V u{q.x, q.y, q.z};
  const V t = cross(u, v) * 2.0;
  return v + t * q.w + cross(u, t);
}
Q qconj(Q q) { return {-q.x, -q.y, -q.z, q.w}; }
Q q_from_frame(V ex, V ey, V ez) {  // rotation whose columns are ex, ey, ez
  const double m00 = ex.x, m10 = ex.y, m20 = ex.z, m01 = ey.x, m11 = ey.y, m21 = ey.z, m02 = ez.x, m12 = ez.y, m22 = ez.z;
  Q q;
  const double tr = m00 + m11 + m22;
  if (tr > 0) { const double s = std::sqrt(tr + 1.0) * 2; q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s}; }
  else if (m00 > m11 && m00 > m22) { const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2; q = {0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s}; }
  else if (m11 > m22) { const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2; q = {(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s}; }
  else { const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2; q = {(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s}; }
  const double l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  return {q.x / l, q.y / l, q.z / l, q.w / l};
}
V fv(ufbx_vec3 v) { return {v.x, v.y, v.z}; }
void put(double d[3], V v) { d[0] = v.x; d[1] = v.y; d[2] = v.z; }
void putq(double d[4], Q q) { d[0] = q.x; d[1] = q.y; d[2] = q.z; d[3] = q.w; }
V V_of(const double d[3]) { return V{d[0], d[1], d[2]}; }

struct Box {
  V lo{1e30, 1e30, 1e30}, hi{-1e30, -1e30, -1e30};
  bool any = false;
  void add(V p) { lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)}; hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)}; any = true; }
  double vol() const { return any ? (hi.x - lo.x) * (hi.y - lo.y) * (hi.z - lo.z) : 0.0; }
};

// Chain link (0..2) a bone belongs to for the left limb, -1 = not in this chain
int group_of(const std::string& limb, const std::string& n) {
  auto pre = [&](const char* p) { return n.rfind(p, 0) == 0; };
  if (limb == "leg") {
    if (n == "UpperLeg_L") return 0;
    if (n == "LowerLeg_L") return 1;
    if (n == "Ankle_L" || n == "AnkleReverse_L" || n == "Ball_L" || n == "Toes_L") return 2;
  } else {
    if (n == "Shoulder_L") return 0;
    if (n == "Elbow_L") return 1;
    if (n == "Hand_L" || pre("Thumb_L") || pre("IndexFinger_L") || pre("Finger_L")) return 2;
  }
  return -1;
}

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

}  // namespace

bool build_limb(const LimbRequest& rq, LimbSpec& out, std::string& rep) {
  std::ostringstream R;
  char b[640];
  auto P = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof b, fmt, a...); R << b << "\n"; };
  // ---- size_label (read from the small copy sizes.json)
  std::string gj;
  { std::ifstream f(rq.sizes_json, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); gj = ss.str(); }
  const std::string row = json_find_row(gj, rq.size_label);
  double height_m = 0, weight_t = 0;
  if (row.empty() || !json_num(row, "height_m", height_m) || !json_num(row, "weight_tons", weight_t)) { rep = "size_label not found in " + rq.sizes_json + ": " + rq.size_label; return false; }
  const bool verified = row.find("\"verified\": true") != std::string::npos;
  const double W = weight_t * rq.ton_kg;
  P("[limb] size_label %s: height %.2f m, weight %.0f (tons; unit read as %.1f kg) = %.4g kg, row flag verified=%s (public fan-wiki figure, see sizes.json)", rq.size_label.c_str(), height_m, weight_t, rq.ton_kg, W,
    verified ? "true" : "false");
  // ---- scene
  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_z_up;
  opts.target_unit_meters = 1.0f;
  ufbx_error err;
  ufbx_scene* s = ufbx_load_file(rq.fbx.c_str(), &opts, &err);
  if (!s) { rep = std::string("ufbx: ") + err.description.data; return false; }
  std::map<std::string, const ufbx_node*> node;
  for (size_t i = 0; i < s->nodes.count; ++i) node[s->nodes.data[i]->name.data] = s->nodes.data[i];
  // ---- vertices per volume key; chain-group vertices kept
  std::map<std::string, Box> vol;
  std::vector<V> gv[3];
  Box gbox_all;
  double diag_geo = 0;
  size_t nverts = 0, nmesh = 0;
  for (size_t i = 0; i < s->nodes.count; ++i) {
    const ufbx_node* n = s->nodes.data[i];
    if (!n->mesh || n->mesh->skin_deformers.count == 0) continue;
    ++nmesh;
    const ufbx_mesh* m = n->mesh;
    const ufbx_skin_deformer* sk = m->skin_deformers.data[0];
    for (size_t v = 0; v < m->num_vertices; ++v) {
      const ufbx_skin_vertex sv = sk->vertices.data[v];
      if (sv.num_weights == 0) continue;
      size_t best = 0;
      float bw = -1;
      for (uint32_t k = 0; k < sv.num_weights; ++k) {
        const ufbx_skin_weight sw = sk->weights.data[sv.weight_begin + k];
        if (sw.weight > bw) { bw = sw.weight; best = sw.cluster_index; }
      }
      const ufbx_skin_cluster* cl = sk->clusters.data[best];
      if (!cl->bone_node) continue;
      const ufbx_vec3 lv = m->vertices.data[v];
      const V w = fv(ufbx_transform_position(&n->geometry_to_world, lv));
      if (v == 0) {  // cross-check geometry_to_world against the skin cluster bind pose for one vertex per mesh
        const V w2 = fv(ufbx_transform_position(&cl->bind_to_world, ufbx_transform_position(&cl->geometry_to_bone, lv)));
        diag_geo = std::max(diag_geo, norm(w - w2));
      }
      const std::string bn = cl->bone_node->name.data;
      std::string key = bn;
      for (const char* lm : {"leg", "arm"}) { const int gg = group_of(lm, bn); if (gg >= 0) key = std::string(lm) + ":g" + std::to_string(gg); }
      vol[key].add(w);
      gbox_all.add(w);
      const int g = group_of(rq.limb, bn);
      if (g >= 0) gv[g].push_back(w);
      ++nverts;
    }
  }
  const double humanoid_h = gbox_all.hi.z - gbox_all.lo.z;
  double vtot = 0;
  for (auto& kv : vol) vtot += kv.second.vol();
  P("[limb] file %s: %zu skinned meshes, %zu vertices (dominant-weight bone), humanoid AABB height %.4f m (z %.4f .. %.4f), %zu volume keys, sum of bone-AABB volumes %.4f m^3", rq.fbx.c_str(), nmesh, nverts,
    humanoid_h, gbox_all.lo.z, gbox_all.hi.z, vol.size(), vtot);
  P("[limb] check: max |geometry_to_world*v - bind_to_world*geometry_to_bone*v| over one vertex per mesh = %.3g m", diag_geo);
  const double sc = height_m / humanoid_h;
  P("[limb] scale factor to the target size: %.4f (skeleton height %.4f m -> %.2f m)", sc, humanoid_h, height_m);

  // ---- joints
  const char* jn_leg[4] = {"UpperLeg_L", "LowerLeg_L", "Ankle_L", "Toes_L"};
  const char* jn_arm[4] = {"Shoulder_L", "Elbow_L", "Hand_L", "IndexFinger_L_04"};
  const char** jn = rq.limb == "leg" ? jn_leg : jn_arm;
  V J[4];
  const ufbx_node* jnode[4];
  for (int i = 0; i < 4; ++i) {
    if (!node.count(jn[i])) { rep = std::string("bone missing: ") + jn[i]; ufbx_free_scene(s); return false; }
    jnode[i] = node[jn[i]];
    J[i] = fv(jnode[i]->node_to_world.cols[3]);
    double dmax = 1e30;
    for (size_t c = 0; c < s->skin_clusters.count; ++c) if (s->skin_clusters.data[c]->bone_node == jnode[i]) dmax = std::min(dmax, norm(fv(s->skin_clusters.data[c]->bind_to_world.cols[3]) - J[i]));
    if (dmax > 1e29) P("[limb] joint %-18s world (file pose) %9.4f %9.4f %9.4f ; no skin cluster for this bone", jn[i], J[i].x, J[i].y, J[i].z);
    else P("[limb] joint %-18s world (file pose) %9.4f %9.4f %9.4f ; |node_to_world - cluster bind_to_world| = %.3g m", jn[i], J[i].x, J[i].y, J[i].z, dmax);
  }
  const bool hang = rq.limb == "arm" && rq.arm_pose == "hang" && !rq.worst_axes;
  Q Qp{0, 0, 0, 1};
  if (hang) Qp = qaxis({0, 1, 0}, 3.14159265358979323846 / 2);  // x -> -z
  const V P0 = J[0];
  auto pos_pose = [&](V p) { return P0 + qrot(Qp, p - P0); };
  for (int i = 0; i < 4; ++i) J[i] = pos_pose(J[i]);
  for (auto& vec : gv) for (V& p : vec) p = pos_pose(p);
  P("[limb] start pose: %s", hang ? "arm rotated down about the shoulder (file T-pose rotated +90 deg about y; the pose is an assumption, link lengths and joint offsets are from the file)"
                                  : "file bind pose (leg: standing; arm: T-pose, horizontal)");

  // ---- link frames
  const V ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};
  V dir[3], nrm[3], zl[3];
  double L[3];
  double max_off_plane = 0;
  for (int i = 0; i < 3; ++i) {
    const V d = J[i + 1] - J[i];
    L[i] = norm(d);
    dir[i] = unit(d);
    max_off_plane = std::max(max_off_plane, std::fabs(dot(dir[i], ex)));
    const V ref = std::fabs(dot(dir[i], ex)) < 0.9 ? ex : ez;
    nrm[i] = unit(ref - dir[i] * dot(ref, dir[i]));
    zl[i] = cross(dir[i], nrm[i]);
  }
  P("[limb] link lengths (file units, m): %.4f %.4f %.4f ; max |link direction . world x| = %.4f (0 = the chain lies in one plane with normal world x)", L[0], L[1], L[2], max_off_plane);

  // ---- hinge axes: anatomical candidate, checked against the local axes of the bone
  const char* hn_leg[4] = {"hip abduction", "hip pitch", "knee", "ankle"};
  const char* hn_arm[4] = {"shoulder abduction", "shoulder pitch", "elbow", "wrist"};
  const char** hn = rq.limb == "leg" ? hn_leg : hn_arm;
  V axis[4];
  const int bone_of_hinge[4] = {0, 0, 1, 2};
  for (int h = 0; h < 4; ++h) {
    const V cand = (h == 0) ? ey : ex;
    const ufbx_matrix& M = jnode[bone_of_hinge[h]]->node_to_world;
    const V la[3] = {unit(qrot(Qp, fv(M.cols[0]))), unit(qrot(Qp, fv(M.cols[1]))), unit(qrot(Qp, fv(M.cols[2])))};
    int bi = 0;
    for (int k = 1; k < 3; ++k) if (std::fabs(dot(la[k], cand)) > std::fabs(dot(la[bi], cand))) bi = k;
    const double dd = dot(la[bi], cand);
    const bool file_ok = h != 0 && std::fabs(dd) >= 0.95;
    axis[h] = file_ok ? la[bi] * (dd < 0 ? -1.0 : 1.0) : cand;
    P("[limb] hinge %d %-18s: anatomical candidate (%g,%g,%g); bone %s best local axis %c |dot| %.4f -> %s", h, hn[h], cand.x, cand.y, cand.z, jn[bone_of_hinge[h]], "XYZ"[bi], std::fabs(dd),
      h == 0 ? "no separate DOF in the file (ball joint = one bone): anatomical forward axis used" : file_ok ? "file axis used (sign aligned to the candidate)" : "no usable file axis: anatomical axis used");
  }
  if (rq.worst_axes) { for (int h = 0; h < 4; ++h) axis[h] = ez; P("[limb] WORST-AXIS case: every hinge axis forced to world z (parallel to gravity), as in the original chain scenario"); }

  // ---- masses by bone-AABB volume
  double gvol[3], mass[3];
  for (int g = 0; g < 3; ++g) { gvol[g] = vol[rq.limb + ":g" + std::to_string(g)].vol(); mass[g] = W * gvol[g] / vtot; }
  P("[limb] link volume (bone-AABB, file frame) %.4f %.4f %.4f m^3 of %.4f total -> shares %.5f %.5f %.5f ; masses %.4g %.4g %.4g kg (sum %.4g of %.4g)", gvol[0], gvol[1], gvol[2], vtot, gvol[0] / vtot,
    gvol[1] / vtot, gvol[2] / vtot, mass[0], mass[1], mass[2], mass[0] + mass[1] + mass[2], W);

  // ---- bodies (scaled). body 0 = gimbal between the abduction and the pitch hinge
  out = LimbSpec{};
  out.body.resize(4);
  out.hinge.resize(4);
  {
    LimbBody& g = out.body[0];
    g.name = std::string(rq.limb == "leg" ? "hip" : "shoulder") + "-gimbal";
    put(g.c, J[0] * sc); putq(g.q, {0, 0, 0, 1});
    g.half[0] = g.half[1] = g.half[2] = 0.06 * L[0] * sc;
    g.mass = 0.1 * mass[0];
  }
  const char* ln_leg[3] = {"thigh", "shank", "foot"};
  const char* ln_arm[3] = {"upperarm", "forearm", "hand"};
  for (int i = 0; i < 3; ++i) {
    LimbBody& bd = out.body[size_t(i + 1)];
    bd.name = rq.limb == "leg" ? ln_leg[i] : ln_arm[i];
    double mn[3] = {1e30, 1e30, 1e30}, mx[3] = {-1e30, -1e30, -1e30};
    for (const V& p : gv[i]) {
      const V r = p - J[i];
      const double c[3] = {dot(r, dir[i]), dot(r, nrm[i]), dot(r, zl[i])};
      for (int k = 0; k < 3; ++k) { mn[k] = std::min(mn[k], c[k]); mx[k] = std::max(mx[k], c[k]); }
    }
    const double wid = mx[1] - mn[1], dep = mx[2] - mn[2];
    P("[limb] link %-8s: joint-to-joint %.4f m; mesh extent along the link %.4f..%.4f, across %.4f x %.4f m (box uses the across extents; file units)", bd.name.c_str(), L[i], mn[0], mx[0], wid, dep);
    put(bd.c, (J[i] + J[i + 1]) * (0.5 * sc));
    putq(bd.q, q_from_frame(dir[i], nrm[i], zl[i]));
    bd.half[0] = 0.5 * L[i] * sc;
    bd.half[1] = std::max(0.5 * wid, 0.02 * L[i]) * sc;
    bd.half[2] = std::max(0.5 * dep, 0.02 * L[i]) * sc;
    bd.mass = mass[i];
  }
  for (auto& bd : out.body) { for (double& x : bd.v) x = 0; for (double& x : bd.w) x = 0; }
  put(out.tip, J[3] * sc);
  const double lim[4][2] = {{-0.6, 0.6}, {-1.5, 1.5}, {-1.5, 1.5}, {-1.5, 1.5}};  // ASSUMPTION: plausible ranges, not from the file
  for (int h = 0; h < 4; ++h) {
    LimbHinge& hg = out.hinge[size_t(h)];
    hg.name = hn[h];
    put(hg.anchor, (h <= 1 ? J[0] : h == 2 ? J[1] : J[2]) * sc);
    put(hg.axis, axis[h]);
    hg.lo = lim[h][0]; hg.hi = lim[h][1];
    hg.kp = hg.kd = hg.max_torque = 0; hg.drive = false;
  }
  P("[limb] gimbal body: cube half %.3f m, mass %.4g kg (assumption: 10%% of the %s mass, so the two-hinge hip/shoulder has a small intermediate body)", out.body[0].half[0], out.body[0].mass, out.body[1].name.c_str());

  // ---- case set-up
  const bool free_pitch = (rq.limb_case == "swing" || rq.limb_case == "wall");
  if (rq.limb_case == "swing" && !rq.worst_axes) {  // raise: rotate everything below hinge 1 about it
    const double th = rq.raise;
    const V a = V_of(out.hinge[1].axis), c0 = V_of(out.hinge[1].anchor);
    const Q rr = qaxis(a, th);
    for (size_t i = 1; i < out.body.size(); ++i) {
      put(out.body[i].c, c0 + qrot(rr, V_of(out.body[i].c) - c0));
      const Q q0{out.body[i].q[0], out.body[i].q[1], out.body[i].q[2], out.body[i].q[3]};
      putq(out.body[i].q, qmul(rr, q0));
    }
    for (size_t h = 2; h < out.hinge.size(); ++h) {
      put(out.hinge[h].anchor, c0 + qrot(rr, V_of(out.hinge[h].anchor) - c0));
      put(out.hinge[h].axis, qrot(rr, V_of(out.hinge[h].axis)));
    }
    put(out.tip, c0 + qrot(rr, V_of(out.tip) - c0));
    out.hinge[1].lo -= th;  // limits are relative to the start pose
    out.hinge[1].hi -= th;
    P("[limb] swing: pitch hinge started at %.3f rad from the file pose (limits relative to the start pose %.3f .. %.3f); pitch hinge not driven, the other three PD-held at their start angle", th, out.hinge[1].lo, out.hinge[1].hi);
  }
  // PD gains per hinge from the downstream inertia and the static gravity torque (start pose)
  const double g = rq.gravity;
  for (size_t h = 0; h < out.hinge.size(); ++h) {
    LimbHinge& hg = out.hinge[h];
    const V a = unit(V_of(hg.axis)), an = V_of(hg.anchor);
    double I = 0, tg = 0, lever = 0;
    for (size_t i = (h == 0 ? 0 : h == 1 ? 1 : h - 0); i < out.body.size(); ++i) {  // hinge 0 moves bodies 0..3, hinge 1 bodies 1..3, hinge h bodies h..3
      const LimbBody& bd = out.body[i];
      const V r = V_of(bd.c) - an;
      const double rp2 = dot(r, r) - dot(r, a) * dot(r, a);
      const Q q{bd.q[0], bd.q[1], bd.q[2], bd.q[3]};
      const V al = qrot(qconj(q), a);
      const double hx = bd.half[0], hy = bd.half[1], hz = bd.half[2];
      const double Ix = bd.mass / 3.0 * (hy * hy + hz * hz), Iy = bd.mass / 3.0 * (hx * hx + hz * hz), Iz = bd.mass / 3.0 * (hx * hx + hy * hy);
      I += Ix * al.x * al.x + Iy * al.y * al.y + Iz * al.z * al.z + bd.mass * rp2;
      tg += dot(a, cross(r, V{0, 0, -bd.mass * g}));
      lever += bd.mass * g * std::sqrt(rp2);
    }
    const double w0 = 2.0 * 3.14159265358979323846 * rq.pd_freq;
    hg.kp = I * w0 * w0;
    hg.kd = 2.0 * rq.pd_zeta * std::sqrt(hg.kp * I);
    hg.max_torque = std::max(rq.torque_factor * std::fabs(tg), 0.25 * lever);
    hg.drive = !(free_pitch && h == 1);
    P("[limb] hinge %zu %-18s: downstream inertia %.4g kg m^2, static gravity torque %.4g N m, PD kp %.4g kd %.4g max torque %.4g N m (assumed: %.1f Hz, zeta %.1f, cap = max(%.1f x static, 0.25 x sum m g r_perp)), drive %s", h,
      hg.name.c_str(), I, tg, hg.kp, hg.kd, hg.max_torque, rq.pd_freq, rq.pd_zeta, rq.torque_factor, hg.drive ? "on" : "OFF");
  }
  auto corner = [&](const LimbBody& bd, int sx, int sy, int sz) {
    const Q q{bd.q[0], bd.q[1], bd.q[2], bd.q[3]};
    return V_of(bd.c) + qrot(q, {sx * bd.half[0], sy * bd.half[1], sz * bd.half[2]});
  };
  const V an1 = V_of(out.hinge[1].anchor), a1 = unit(V_of(out.hinge[1].axis));
  const V rt = V_of(out.tip) - an1;
  const double rho = norm(rt - a1 * dot(rt, a1));
  if (rq.limb_case == "wall" && !rq.worst_axes) {
    const V v1 = cross(a1, rt);  // velocity direction for omega = +1
    const double sgn = v1.y < 0 ? 1.0 : -1.0;
    const double om = sgn * rq.tip_speed / rho;
    const V vd = unit(v1 * sgn);
    out.launch_hinge = 1;
    out.launch_omega = om;
    for (size_t i = 1; i < out.body.size(); ++i) {
      put(out.body[i].w, a1 * om);
      put(out.body[i].v, cross(a1 * om, V_of(out.body[i].c) - an1));
    }
    double dmax = -1e30;
    for (const LimbBody& bd : out.body)
      for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) dmax = std::max(dmax, dot(corner(bd, sx, sy, sz), vd));
    const double thick = 0.025;
    const double face = dmax + rq.tip_speed * rq.wall_time;
    out.wall = true;
    const V tipp = V_of(out.tip);
    put(out.wall_c, tipp + vd * (face + thick - dot(tipp, vd)));  // centred laterally on the tip
    const V xx = unit(std::fabs(vd.z) < 0.9 ? cross(ez, vd) : cross(ex, vd));
    putq(out.wall_q, q_from_frame(xx, vd, cross(xx, vd)));  // local y = launch direction
    const double ext = 3.0 * norm(rt) + 10.0;
    out.wall_half[0] = ext; out.wall_half[1] = thick; out.wall_half[2] = ext;
    put(out.wall_n, vd * -1.0);
    P("[limb] wall case: pitch omega %.5f rad/s => tip speed %.2f m/s (rho %.3f m); launch direction (%.3f %.3f %.3f); wall face %.2f m beyond the leading corner of the limb (%.0f ms at tip speed), 5 cm thick, half extent %.0f m", om,
      rq.tip_speed, rho, vd.x, vd.y, vd.z, rq.tip_speed * rq.wall_time, rq.wall_time * 1000.0, ext);
  }
  if (rq.limb_case == "impact") {
    double ymin = 1e30;
    for (const LimbBody& bd : out.body)
      for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) ymin = std::min(ymin, corner(bd, sx, sy, sz).y);
    const double half = 0.5;
    out.proj = true;
    out.proj_half[0] = out.proj_half[1] = out.proj_half[2] = half;
    out.proj_mass = rq.impact_mass;
    out.proj_c[0] = out.body.back().c[0]; out.proj_c[1] = ymin - half - rq.impact_speed * rq.wall_time; out.proj_c[2] = out.body.back().c[2];
    out.proj_v[0] = 0; out.proj_v[1] = rq.impact_speed; out.proj_v[2] = 0;
    P("[limb] impact case: %.0f kg cube (1 m) at %.1f m/s along +y, starting %.2f m in front of the frontmost corner of the limb, aimed at the centre of the last link (x and z of its centre; aiming at the tip itself grazed the lower box edge in a first try); the 30 m/s is an assumption", rq.impact_mass,
      rq.impact_speed, rq.impact_speed * rq.wall_time);
  }
  char lb[256];
  std::snprintf(lb, sizeof lb, "%s %s size_label=%s scale=%.3f case=%s%s", rq.limb.c_str(), rq.arm_pose.c_str(), rq.size_label.c_str(), sc, rq.limb_case.c_str(), rq.worst_axes ? " worst-axes" : "");
  out.label = lb;
  P("[limb] bodies (centre / half extents / mass):");
  for (const LimbBody& bd : out.body) P("  %-16s c (%.2f %.2f %.2f) half (%.3f %.3f %.3f) mass %.4g kg", bd.name.c_str(), bd.c[0], bd.c[1], bd.c[2], bd.half[0], bd.half[1], bd.half[2], bd.mass);
  P("  tip (%.2f %.2f %.2f)", out.tip[0], out.tip[1], out.tip[2]);
  ufbx_free_scene(s);
  rep = R.str();
  return true;
}
}  // namespace artic
