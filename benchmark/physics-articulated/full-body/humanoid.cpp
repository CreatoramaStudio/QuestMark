// Full-body test: whole-humanoid builder. Reads the humanoid skeleton and mesh (bind pose) with ufbx (loading approach as in the
// limb builder of the chain-limb test, whose derivation steps it reuses) and derives one rigid-body tree: torso (root, free), head (fixed joint),
// two legs (hip gimbal = abduction + pitch hinge, thigh, knee, shank, ankle gimbal = pitch + roll hinge, foot) and two arms
// (shoulder gimbal = abduction + pitch hinge, upper arm, elbow, forearm, wrist, hand). Every derivation step is printed into the report.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "humanoid.h"
#include "ufbx.h"

namespace artic_body {
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
Q qaxis(V ax, double a) { const V u = unit(ax) * std::sin(0.5 * a); return {u.x, u.y, u.z, std::cos(0.5 * a)}; }
V qrot(Q q, V v) { const V u{q.x, q.y, q.z}; const V t = cross(u, v) * 2.0; return v + t * q.w + cross(u, t); }
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

// group key of a bone: "leg<s>:0|1|2", "arm<s>:0|1|2", "head", "torso" (s = L or R)
std::string key_of(const std::string& n) {
  auto pre = [&](const char* p) { return n.rfind(p, 0) == 0; };
  for (const char s : {'L', 'R'}) {
    const std::string sf = std::string("_") + s;
    if (n == "UpperLeg" + sf) return std::string("leg") + s + ":0";
    if (n == "LowerLeg" + sf) return std::string("leg") + s + ":1";
    if (n == "Ankle" + sf || n == "AnkleReverse" + sf || n == "Ball" + sf || n == "Toes" + sf) return std::string("leg") + s + ":2";
    if (n == "Shoulder" + sf) return std::string("arm") + s + ":0";
    if (n == "Elbow" + sf) return std::string("arm") + s + ":1";
    if (n == "Hand" + sf || pre(("Thumb" + sf).c_str()) || pre(("IndexFinger" + sf).c_str()) || pre(("Finger" + sf).c_str())) return std::string("arm") + s + ":2";
  }
  if (n == "Head" || n == "Eyes" || n == "Eyebrows" || n == "Jaw") return "head";
  return "torso";
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

int dump_skeleton(const std::string& path) {
  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_z_up;
  opts.target_unit_meters = 1.0f;
  ufbx_error err;
  ufbx_scene* s = ufbx_load_file(path.c_str(), &opts, &err);
  if (!s) { std::fprintf(stderr, "ufbx: %s\n", err.description.data); return 1; }
  std::unordered_set<const ufbx_node*> js;
  for (size_t i = 0; i < s->nodes.count; ++i) if (s->nodes.data[i]->bone) js.insert(s->nodes.data[i]);
  for (size_t i = 0; i < s->nodes.count; ++i) {
    const ufbx_node* n = s->nodes.data[i];
    if (!js.count(n)) continue;
    std::printf("%-26s key %-8s pos %9.4f %9.4f %9.4f\n", n->name.data, key_of(n->name.data).c_str(), n->node_to_world.cols[3].x, n->node_to_world.cols[3].y, n->node_to_world.cols[3].z);
  }
  ufbx_free_scene(s);
  return 0;
}

bool build_humanoid(const Request& rq, Humanoid& out) {
  std::ostringstream R;
  char b[800];
  auto P = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof b, fmt, a...); R << b << "\n"; };
  const double PI = 3.14159265358979323846;
  // ---- size_label (from the small copy sizes.json)
  std::string gj;
  { std::ifstream f(rq.sizes_json, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); gj = ss.str(); }
  const std::string row = json_find_row(gj, rq.size_label);
  double height_m = 0, weight_t = 0;
  if (row.empty() || !json_num(row, "height_m", height_m) || !json_num(row, "weight_tons", weight_t)) { out.report = "size_label not found in " + rq.sizes_json + ": " + rq.size_label; return false; }
  const bool verified = row.find("\"verified\": true") != std::string::npos;
  const double W = weight_t * rq.ton_kg;
  P("[humanoid] size_label %s: height %.2f m, weight %.0f (tons; unit read as %.1f kg) = %.4g kg, row flag verified=%s (public fan-wiki figure, see sizes.json)", rq.size_label.c_str(), height_m, weight_t, rq.ton_kg, W,
    verified ? "true" : "false");
  // ---- scene
  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_z_up;
  opts.target_unit_meters = 1.0f;
  ufbx_error err;
  ufbx_scene* s = ufbx_load_file(rq.fbx.c_str(), &opts, &err);
  if (!s) { out.report = std::string("ufbx: ") + err.description.data; return false; }
  std::map<std::string, const ufbx_node*> node;
  for (size_t i = 0; i < s->nodes.count; ++i) node[s->nodes.data[i]->name.data] = s->nodes.data[i];
  // ---- vertices (dominant-weight bone) per group key
  std::map<std::string, Box> vol;
  std::map<std::string, std::vector<V>> gv;
  Box all;
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
      const V w = fv(ufbx_transform_position(&n->geometry_to_world, m->vertices.data[v]));
      const std::string key = key_of(cl->bone_node->name.data);
      vol[key].add(w);
      gv[key].push_back(w);
      all.add(w);
      ++nverts;
    }
  }
  const double humanoid_h = all.hi.z - all.lo.z;
  double vtot = 0;
  for (auto& kv : vol) vtot += kv.second.vol();
  P("[humanoid] file %s: %zu skinned meshes, %zu vertices, humanoid AABB height %.4f m (z %.4f .. %.4f), %zu group volumes (AABB per group key), sum %.4f m^3", rq.fbx.c_str(), nmesh, nverts, humanoid_h, all.lo.z,
    all.hi.z, vol.size(), vtot);
  const double sc = height_m / humanoid_h;
  const double dz = -all.lo.z * sc + 0.01;  // lowest vertex 1 cm above the ground (ground top z = 0)
  P("[humanoid] scale factor to the target size: %.4f (skeleton height %.4f m -> %.2f m); humanoid placed so its lowest vertex is 1 cm above the ground plane z = 0", sc, humanoid_h, height_m);
  auto Wp = [&](V p) { return V{p.x * sc, p.y * sc, p.z * sc + dz}; };
  const V ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};
  out = Humanoid{};
  out.scale = sc;
  out.height = height_m;
  std::vector<Body>& B = out.b;
  // ---- torso (root): AABB of every vertex that belongs to no limb or head group
  {
    const Box& bx = vol["torso"];
    Body t;
    t.name = "torso";
    put(t.c, Wp((bx.lo + bx.hi) * 0.5));
    for (int k = 0; k < 3; ++k) t.half[k] = 0.5 * ((&bx.hi.x)[k] - (&bx.lo.x)[k]) * sc;
    t.mass = W * bx.vol() / vtot;
    B.push_back(t);
    P("[humanoid] torso/pelvis: AABB of all non-limb non-head vertices, centre (%.2f %.2f %.2f) half (%.2f %.2f %.2f) m, volume share %.4f", t.c[0], t.c[1], t.c[2], t.half[0], t.half[1], t.half[2], bx.vol() / vtot);
  }
  // ---- head (fixed joint to the torso)
  {
    const Box& bx = vol["head"];
    Body h;
    h.name = "head";
    put(h.c, Wp((bx.lo + bx.hi) * 0.5));
    for (int k = 0; k < 3; ++k) h.half[k] = 0.5 * ((&bx.hi.x)[k] - (&bx.lo.x)[k]) * sc;
    h.mass = W * bx.vol() / vtot;
    h.parent = 0;
    h.jtype = 2;
    put(h.anchor, Wp(fv(node["Head"]->node_to_world.cols[3])));
    B.push_back(h);
    P("[humanoid] head: AABB of Head/Eyes/Eyebrows/Jaw vertices, centre (%.2f %.2f %.2f) half (%.2f %.2f %.2f) m, volume share %.4f, FIXED joint at the Head bone", h.c[0], h.c[1], h.c[2], h.half[0], h.half[1], h.half[2],
      bx.vol() / vtot);
  }
  const double lim_abd[2] = {-0.6, 0.6}, lim_pit[2] = {-1.5, 1.5};  // ASSUMPTION: plausible ranges, not from the file (as in the chain-limb test)
  auto add_hinge_body = [&](Body bd, int parent, V anchor_file, V axis, const double lim[2]) {
    bd.parent = parent;
    bd.jtype = 1;
    put(bd.anchor, Wp(anchor_file));
    put(bd.axis, axis);
    bd.lo = lim[0];
    bd.hi = lim[1];
    B.push_back(bd);
    return static_cast<int>(B.size()) - 1;
  };
  // ---- limbs
  for (int side = 0; side < 2; ++side) {
    const char sc_ = side == 0 ? 'L' : 'R';
    for (int arm = 0; arm < 2; ++arm) {
      const std::string lk = std::string(arm ? "arm" : "leg") + sc_;
      const std::string sfx = std::string("_") + sc_;
      const char* jn_leg[4] = {"UpperLeg", "LowerLeg", "Ankle", "Toes"};
      const char* jn_arm[4] = {"Shoulder", "Elbow", "Hand", "IndexFinger"};
      V J[4];
      for (int i = 0; i < 4; ++i) {
        std::string nm = std::string(arm ? jn_arm[i] : jn_leg[i]) + sfx;
        if (arm && i == 3) nm += "_04";
        if (!node.count(nm)) { out.report = "bone missing: " + nm; ufbx_free_scene(s); return false; }
        J[i] = fv(node[nm]->node_to_world.cols[3]);
      }
      std::vector<V> gvv[3];
      for (int g = 0; g < 3; ++g) gvv[g] = gv[lk + ":" + std::to_string(g)];
      if (arm) {  // hanging pose: rotate about the shoulder by +-90 deg about y (file T-pose arm along +-x -> straight down)
        const Q Qp = qaxis({0, 1, 0}, (side == 0 ? 1.0 : -1.0) * PI / 2);
        const V P0 = J[0];
        for (V& j : J) j = P0 + qrot(Qp, j - P0);
        for (auto& vec : gvv) for (V& p : vec) p = P0 + qrot(Qp, p - P0);
      }
      double mass[3];
      for (int g = 0; g < 3; ++g) mass[g] = W * vol[lk + ":" + std::to_string(g)].vol() / vtot;
      V dir[3], nrm[3], zl[3];
      double L[3];
      for (int i = 0; i < 3; ++i) {
        const V d = J[i + 1] - J[i];
        L[i] = norm(d);
        dir[i] = unit(d);
        const V ref = std::fabs(dot(dir[i], ex)) < 0.9 ? ex : ez;
        nrm[i] = unit(ref - dir[i] * dot(ref, dir[i]));
        zl[i] = cross(dir[i], nrm[i]);
      }
      auto cube = [&](const char* nm, V c, double h, double m) {
        Body g;
        g.name = nm;
        put(g.c, Wp(c));
        g.half[0] = g.half[1] = g.half[2] = h * sc;
        g.mass = m;
        return g;
      };
      auto link_box = [&](const char* nm, int i) {  // oriented box along joint-to-joint, across extents from the vertices (as in the chain-limb test)
        Body bd;
        bd.name = nm;
        double mn[3] = {1e30, 1e30, 1e30}, mx[3] = {-1e30, -1e30, -1e30};
        for (const V& p : gvv[i]) {
          const V r = p - J[i];
          const double c[3] = {dot(r, dir[i]), dot(r, nrm[i]), dot(r, zl[i])};
          for (int k = 0; k < 3; ++k) { mn[k] = std::min(mn[k], c[k]); mx[k] = std::max(mx[k], c[k]); }
        }
        put(bd.c, Wp((J[i] + J[i + 1]) * 0.5));
        putq(bd.q, q_from_frame(dir[i], nrm[i], zl[i]));
        bd.half[0] = 0.5 * L[i] * sc;
        bd.half[1] = std::max(0.5 * (mx[1] - mn[1]), 0.02 * L[i]) * sc;
        bd.half[2] = std::max(0.5 * (mx[2] - mn[2]), 0.02 * L[i]) * sc;
        bd.mass = mass[i];
        return bd;
      };
      const std::string pre = (arm ? "arm" : "leg") + std::string(1, sc_) + "-";
      if (!arm) {
        const int hg = add_hinge_body(cube((pre + "hip-gimbal").c_str(), J[0], 0.06 * L[0], 0.1 * mass[0]), 0, J[0], ey, lim_abd);
        const int th = add_hinge_body(link_box((pre + "thigh").c_str(), 0), hg, J[0], ex, lim_pit);
        const int sk = add_hinge_body(link_box((pre + "shank").c_str(), 1), th, J[1], ex, lim_pit);
        const int ag = add_hinge_body(cube((pre + "ankle-gimbal").c_str(), J[2], 0.06 * L[1], 0.1 * mass[1]), sk, J[2], ex, lim_pit);
        Body ft;
        ft.name = pre + "foot";
        Box fb;
        for (const V& p : gvv[2]) fb.add(p);
        put(ft.c, Wp((fb.lo + fb.hi) * 0.5));  // flat-soled world-aligned box (the file foot is tilted; an oriented box would stand on an edge)
        for (int k = 0; k < 3; ++k) ft.half[k] = 0.5 * ((&fb.hi.x)[k] - (&fb.lo.x)[k]) * sc;
        ft.mass = mass[2];
        const int fo = add_hinge_body(ft, ag, J[2], ey, lim_abd);
        out.hip_abd[side] = hg; out.hip_pitch[side] = th; out.knee[side] = sk; out.ankle_pitch[side] = ag; out.ankle_roll[side] = fo; out.foot[side] = fo;
        out.thigh0[side][0] = (J[1] - J[0]).y * sc; out.thigh0[side][1] = (J[1] - J[0]).z * sc;
        out.shank0[side][0] = (J[2] - J[1]).y * sc; out.shank0[side][1] = (J[2] - J[1]).z * sc;
        out.hip_x[side] = J[0].x * sc;
        if (side == 0) { out.hip_height = Wp(J[0]).z; out.foot_len = (fb.hi.y - fb.lo.y) * sc; }
        P("[humanoid] leg %c: joints (file m) hip (%.3f %.3f %.3f) knee (%.3f %.3f %.3f) ankle (%.3f %.3f %.3f); lengths %.3f %.3f; foot AABB y %.3f..%.3f z %.3f..%.3f; masses %.4g %.4g %.4g kg", sc_, J[0].x, J[0].y,
          J[0].z, J[1].x, J[1].y, J[1].z, J[2].x, J[2].y, J[2].z, L[0], L[1], fb.lo.y, fb.hi.y, fb.lo.z, fb.hi.z, mass[0], mass[1], mass[2]);
      } else {
        const int sg = add_hinge_body(cube((pre + "shoulder-gimbal").c_str(), J[0], 0.06 * L[0], 0.1 * mass[0]), 0, J[0], ey, lim_abd);
        const int ua = add_hinge_body(link_box((pre + "upperarm").c_str(), 0), sg, J[0], ex, lim_pit);
        const int fa = add_hinge_body(link_box((pre + "forearm").c_str(), 1), ua, J[1], ex, lim_pit);
        const int hd = add_hinge_body(link_box((pre + "hand").c_str(), 2), fa, J[2], ex, lim_pit);
        out.sh_abd[side] = sg; out.sh_pitch[side] = ua; out.elbow[side] = fa; out.wrist[side] = hd; out.hand[side] = hd;
        put(out.arm_tip[side], Wp(J[3]));
        P("[humanoid] arm %c (hanging): joints (file m, rotated) shoulder (%.3f %.3f %.3f) elbow (%.3f %.3f %.3f) wrist (%.3f %.3f %.3f) tip (%.3f %.3f %.3f); masses %.4g %.4g %.4g kg", sc_, J[0].x, J[0].y, J[0].z, J[1].x,
          J[1].y, J[1].z, J[2].x, J[2].y, J[2].z, J[3].x, J[3].y, J[3].z, mass[0], mass[1], mass[2]);
      }
    }
  }
  // ---- normalise masses to the sourced weight (gimbals added 10 % of a link each)
  double msum = 0;
  for (const Body& bd : B) msum += bd.mass;
  for (Body& bd : B) bd.mass *= W / msum;
  out.total_mass = W;
  P("[humanoid] masses: bone-AABB-volume shares, gimbal bodies 10%% of the adjacent link; raw sum %.4g kg normalised to the sourced %.4g kg (factor %.4f)", msum, W, W / msum);
  // ---- PD gains per hinge from inertia about the axis (subtree and rest of the humanoid) and static gravity torque
  const double g = rq.gravity;
  const size_t n = B.size();
  std::vector<std::vector<size_t>> sub(n);  // subtree of each body (including itself)
  for (size_t i = 0; i < n; ++i) { size_t k = i; while (true) { sub[k].push_back(i); if (B[k].parent < 0) break; k = static_cast<size_t>(B[k].parent); } }
  // sub[k] now holds the DESCENDANTS of k: body i is added to every ancestor k (including itself)
  const double w0 = 2.0 * PI * rq.pd_freq;
  auto is_leg_joint = [&](size_t i) { for (int sd = 0; sd < 2; ++sd) if (int(i) == out.hip_abd[sd] || int(i) == out.hip_pitch[sd] || int(i) == out.knee[sd] || int(i) == out.ankle_pitch[sd] || int(i) == out.ankle_roll[sd]) return true; return false; };
  for (size_t h = 1; h < n; ++h) {
    Body& hb = B[h];
    if (hb.jtype != 1) continue;
    const V a = unit(V_of(hb.axis)), an = V_of(hb.anchor);
    auto inertia_of = [&](const Body& bd) {
      const V r = V_of(bd.c) - an;
      const double rp2 = dot(r, r) - dot(r, a) * dot(r, a);
      const Q q{bd.q[0], bd.q[1], bd.q[2], bd.q[3]};
      const V al = qrot(qconj(q), a);
      const double hx = bd.half[0], hy = bd.half[1], hz = bd.half[2];
      const double Ix = bd.mass / 3.0 * (hy * hy + hz * hz), Iy = bd.mass / 3.0 * (hx * hx + hz * hz), Iz = bd.mass / 3.0 * (hx * hx + hy * hy);
      return Ix * al.x * al.x + Iy * al.y * al.y + Iz * al.z * al.z + bd.mass * rp2;
    };
    std::vector<char> in_sub(n, 0);
    for (size_t i : sub[h]) in_sub[i] = 1;
    double Is = 0, Ir = 0, tg = 0, lever = 0;
    for (size_t i = 0; i < n; ++i) {
      const double Ii = inertia_of(B[i]);
      if (in_sub[i]) {
        Is += Ii;
        const V r = V_of(B[i].c) - an;
        tg += dot(a, cross(r, V{0, 0, -B[i].mass * g}));
        lever += B[i].mass * g * std::sqrt(std::max(0.0, dot(r, r) - dot(r, a) * dot(r, a)));
      } else Ir += Ii;
    }
    const double red = Is * Ir / (Is + Ir);
    const bool leg = is_leg_joint(h);
    const double Ieff = leg ? std::max(red, rq.stand_mult * Ir) : red;
    hb.inertia_sub = Is; hb.inertia_rest = Ir; hb.static_tq = tg;
    hb.kp = Ieff * w0 * w0;
    hb.kd = 2.0 * rq.pd_zeta * Ieff * w0;
    hb.max_torque = std::max(rq.torque_factor * std::fabs(tg), 0.25 * lever);
    if (leg) hb.max_torque = std::max(hb.max_torque, rq.leg_cap * W * g * out.hip_height);
    hb.drive = !(rq.free_shoulder_pitch && int(h) == out.sh_pitch[0]);
    P("[humanoid] joint %2zu %-22s parent %2d: I_sub %.4g I_rest %.4g (reduced %.4g, used %.4g) kg m^2, static gravity torque %.4g N m, PD kp %.4g kd %.4g max torque %.4g N m, drive %s", h, hb.name.c_str(), hb.parent, Is, Ir, red,
      Ieff, tg, hb.kp, hb.kd, hb.max_torque, hb.drive ? "on" : "OFF");
  }
  P("[humanoid] PD assumptions: %.1f Hz, zeta %.1f, hinge torque cap = max(%.1f x static, 0.25 x sum m g r_perp); leg joints: effective inertia max(reduced, %.2f x inertia of the rest of the humanoid), cap floor %.2f x M g x hip height (%.2f m)",
    rq.pd_freq, rq.pd_zeta, rq.torque_factor, rq.stand_mult, rq.leg_cap, out.hip_height);
  P("[humanoid] bodies (centre / half extents / mass / parent / joint):");
  for (size_t i = 0; i < n; ++i)
    P("  %2zu %-22s c (%.2f %.2f %.2f) half (%.3f %.3f %.3f) mass %.4g kg parent %d %s", i, B[i].name.c_str(), B[i].c[0], B[i].c[1], B[i].c[2], B[i].half[0], B[i].half[1], B[i].half[2], B[i].mass, B[i].parent,
      B[i].jtype == 0 ? "root" : B[i].jtype == 2 ? "fixed" : "hinge");
  ufbx_free_scene(s);
  out.report = R.str();
  return true;
}
}  // namespace artic_body
