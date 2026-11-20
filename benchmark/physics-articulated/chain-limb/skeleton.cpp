// Chain-limb test: skeleton reader (ufbx). Loading approach: ufbx_load_file, joint set = bone nodes + skin cluster bones).
#include "skeleton.h"

#include <cmath>
#include <cstdio>
#include <unordered_set>

#include "ufbx.h"

namespace artic {
int dump_skeleton(const std::string& path) {
  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_z_up;
  opts.target_unit_meters = 1.0f;
  ufbx_error err;
  ufbx_scene* s = ufbx_load_file(path.c_str(), &opts, &err);
  if (!s) { std::fprintf(stderr, "ufbx: %s\n", err.description.data); return 1; }
  std::printf("# file %s nodes %zu bones %zu skin_clusters %zu\n", path.c_str(), s->nodes.count, s->bones.count, s->skin_clusters.count);
  std::unordered_set<const ufbx_node*> joint_set;
  for (size_t i = 0; i < s->nodes.count; ++i) if (s->nodes.data[i]->bone) joint_set.insert(s->nodes.data[i]);
  for (size_t i = 0; i < s->skin_clusters.count; ++i) if (s->skin_clusters.data[i]->bone_node) joint_set.insert(s->skin_clusters.data[i]->bone_node);
  for (size_t i = 0; i < s->nodes.count; ++i) {
    const ufbx_node* n = s->nodes.data[i];
    if (!joint_set.count(n)) continue;
    const ufbx_matrix& m = n->node_to_world;
    std::printf("%-26s parent %-22s pos %9.4f %9.4f %9.4f | X %6.3f %6.3f %6.3f Y %6.3f %6.3f %6.3f Z %6.3f %6.3f %6.3f\n", n->name.data,
                n->parent ? n->parent->name.data : "-", m.cols[3].x, m.cols[3].y, m.cols[3].z, m.cols[0].x, m.cols[0].y, m.cols[0].z, m.cols[1].x, m.cols[1].y,
                m.cols[1].z, m.cols[2].x, m.cols[2].y, m.cols[2].z);
  }
  ufbx_free_scene(s);
  return 0;
}
}  // namespace artic
