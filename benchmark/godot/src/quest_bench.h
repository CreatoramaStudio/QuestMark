#pragma once
// Quest engine benchmark, Godot side: C++ GDExtension port of the former main.gd (kept in ../legacy/main.gd).
// Spec: benchmark/SPEC.md. Notes: benchmark/godot/NOTES.md.
// Usage (args after "--"): --scenario=pit-2000|raycast-1000|raycast-10000|chains|mixed --mode=physics|render
//   --run=1 --out=<dir> [--warmup=5 --measure=20] [--diag-busy-ms=<ms>]
// Scenarios E/F: --scenario=unlock-render-mixed|unlock-physics-mixed|unlock-physics-pit-2000|scale (scale: --bodies=N --mode=physics|render [--unpaced=1])
// Scenario G: --scenario=debris|debris-pooled --rate=<events per second> --mode=physics|render (warm-up defaults to 10 s for G)
// Body API: --body-api=nodes|rid (default nodes). rid = bodies and chain joints made on PhysicsServer3D, MultiMesh drawing; also for debris (G): pieces are server bodies, drawn with one MultiMesh.
// Jolt limits: --jolt-limits=old|raised|bodies (default by scenario: F and G raised, others old; see register_types.cpp), --jolt-max-bodies/--jolt-max-pairs/--jolt-max-constraints=<n> override

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/xr_interface_extension.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <deque>
#include <vector>

// Jolt limit profile applied by register_types.cpp before the physics space exists (for the env file)
extern const char *g_jolt_profile;

// Tick-start mark: an XR interface that is never rendered with. XRServer::_process() runs every frame right before the physics loop
// (main.cpp) and calls process() on every initialized interface, so this stores a timestamp at the start of the frame's physics
// section. QuestBench uses it to also cover sync() + flush_queries() of the first tick (physics_ms_def 2). Main thread only.
class QuestTickMark : public godot::XRInterfaceExtension {
	GDCLASS(QuestTickMark, godot::XRInterfaceExtension)

public:
	static uint64_t mark_us;
	static uint64_t mark_frame; // Engine::get_process_frames() at the mark
	godot::StringName _get_name() const override { return "QuestTickMark"; }
	uint32_t _get_capabilities() const override { return 0; }
	bool _is_initialized() const override { return true; }
	bool _initialize() override { return true; }
	void _uninitialize() override {}
	void _process() override;

protected:
	static void _bind_methods() {}
};

class QuestBench : public godot::Node3D {
	GDCLASS(QuestBench, godot::Node3D)

public:
	void _ready() override;
	void _physics_process(double p_delta) override;
	void _process(double p_delta) override;
	void _exit_tree() override;

protected:
	static void _bind_methods() {}

private:
	struct Row {
		int frame;
		double wall_ms;
		double step_ms;
		double query_ms;
		int awake;
		double event_ms = 0.0; // scenario G only
		double despawn_ms = 0.0;
		int live = 0;
		int steps = 0; // physics steps in this frame
		double step_mean_ms = 0.0; // step_ms / steps
		double tick_start_ms = 0.0; // tick-start window of the first tick (inside step_ms when the mark is added), 0 without a tick
	};

	// options
	godot::String scenario = "pit-2000";
	godot::String mode = "physics";
	godot::String run_id = "1";
	godot::String flavour;
	godot::String space_class;
	bool engine_printed = false;
	godot::String out_dir = "results";
	double warmup_s = 5.0;
	double measure_s = 20.0;
	double diag_busy_ms = 0.0;
	bool render = false;
	bool unpaced = false;
	int diag_tps = 200000; // unpaced: ticks_per_second = 1000 x 200 Hz, time_scale 1000
	bool diag_camera = false; // --diag-camera=1: print the camera state once at startup (render mode)
	godot::String camera_mode = "content"; // --camera=content|sky (SPEC "Camera")
	bool sleeping_off = false; // effective value (SPEC "Profiles")
	godot::String profile = "game", sleeping_opt, shadows_opt, vsync_opt; // --profile=game|stress, --sleeping=default|off, --shadows=on|off, --vsync=on|off (empty = profile default)
	int shadow_map = 2048, msaa = 4; // --shadow-map=<px>, --msaa=0|2|4|8
	bool shadows_on = true, vsync_on = false, unlock_render = false;
	godot::String render_env_lines; // env.txt lines describing the render setup actually applied (filled by _add_camera_and_light)
	// content bounds (AABB of the content, SPEC "Camera"), camera values for env.txt
	bool bounds_valid = false;
	godot::Vector3 bmin, bmax;
	godot::Vector3 cam_pos, cam_look_at;
	double cam_radius = 0.0;
	std::vector<godot::RID> chain_rids;
	godot::String base_scenario, file_label, file_mode;
	int bodies = 2000;
	double pit_interior = 0.0; // 0 = no pit (raycast, chains)
	uint64_t ticks_rec = 0;
	double delta_min = 1e9, delta_max = 0.0;
	uint64_t phys_frames_start = 0;
	std::vector<double> row_cum_ms;
	std::vector<char> row_ticked;
	double cum_ms = 0.0;

	godot::Ref<godot::RandomNumberGenerator> rng;
	godot::Ref<godot::PhysicsMaterial> phys_mat;
	godot::Ref<godot::BoxShape3D> box_shape;
	godot::Ref<godot::SphereShape3D> sphere_shape;
	godot::Ref<godot::CapsuleShape3D> capsule_shape;
	godot::Ref<godot::Mesh> box_mesh, sphere_mesh, capsule_mesh;
	godot::Ref<godot::Mesh> unit_box_mesh; // render mode: one shared 1 m box, scaled per static box (like Stride unitBoxModel)

	int ray_count = 0;
	std::vector<godot::Vector3> ray_from, ray_to;
	godot::Ref<godot::PhysicsRayQueryParameters3D> ray_params;
	int64_t total_hits = 0;
	double query_ms_last = 0.0;
	double query_ms_frame = 0.0; // sum of the ray batches of the steps in this frame
	std::vector<double> step_list, steps_pf_list; // per-step stopwatch values and steps per frame over the measured window

	std::vector<godot::RigidBody3D *> impulse_bodies;
	bool impulse_done = false;
	std::vector<godot::Vector3> impulse_pos0;

	// physics step stopwatch: end of our last _physics_process in a frame to the start of _process
	uint64_t phys_end_us = 0;
	int tick_in_frame = 0;
	uint64_t step_acc_us = 0;
	// tick-start mark (physics_ms_def 2): QuestTickMark stamp to the start of the first tick's _physics_process
	bool tick_mark_opt = true; // --tick-mark=0|1
	bool tick_mark_added = false;
	godot::Ref<QuestTickMark> tick_mark;
	uint64_t lead_pending_us = 0, lead_frame_us = 0;
	int64_t lead_missing = 0;
	std::vector<double> tick_start_list;
	std::vector<godot::RID> body_rids;
	// --body-api=rid: free dynamic bodies and chain joints are created directly on PhysicsServer3D (no RigidBody3D / PinJoint3D nodes, no
	// state-sync callback); render mode draws them with one MultiMesh per shape kind, filled from body_get_state once per frame.
	bool body_rid = false;
	struct MMGroup {
		godot::RID mm, inst;
		godot::Ref<godot::Mesh> mesh;
		std::vector<godot::RID> bodies;
		godot::PackedFloat32Array buf;
	};
	MMGroup mm_groups[3]; // 0 box, 1 sphere, 2 capsule
	std::vector<godot::RID> impulse_rids, joint_rids;
	godot::RID _make_dynamic_rid(int p_kind, const godot::Vector3 &p_pos);
	godot::RID _make_piece_rid(const godot::Vector3 &p_pos);
	std::deque<godot::RID> deb_rids; // rid mode, scenario G: every piece on the server (live and parked), oldest first
	int deb_cap = 0; // MultiMesh capacity for the debris pieces
	void _setup_multimeshes();
	void _sync_multimeshes();
	int awake_last = 0;
	double script_ms_last = 0.0;
	std::vector<double> mon_phys, awake_cost, script_cost, phys_ticks;

	// recording
	uint64_t last_us = 0;
	uint64_t phase_start_us = 0;
	bool recording = false;
	bool finished = false;
	int frame_idx = 0;
	std::vector<double> wall, phys;
	std::vector<Row> rows;

	// ---- scenario G (debris) ----
	struct DebrisEvent {
		double t_event;
		int block;
		std::vector<godot::RigidBody3D *> pieces;
		std::vector<godot::RID> rids;
		std::vector<godot::Vector3> parks; // pooled: where each piece is parked
	};
	bool is_debris = false, pooled = false, warmup_given = false;
	double rate = 8.0;
	std::vector<godot::StaticBody3D *> blocks; // nullptr = block removed
	std::vector<int> block_order, live_at;
	std::deque<DebrisEvent> live_events;
	std::vector<godot::RigidBody3D *> pool_free;
	std::vector<godot::RID> pool_free_rid; // rid mode pool
	int64_t next_event_k = 0;
	int64_t tick_global = 0, events_total = 0, events_on_intact = 0, events_on_used = 0, removals_total = 0, pool_misses = 0, pool_size0 = 0, live_pieces = 0;
	int64_t events_rec = 0, removals_rec = 0;
	uint64_t frame_event_us = 0, frame_despawn_us = 0;
	bool frame_event = false, frame_despawn = false;
	std::vector<double> event_ms_list, despawn_ms_list, wall_event, wall_despawn, wall_nonevent;
	std::vector<char> row_event, row_despawn;
	double vel_after_spawn = -1.0, vel_next_tick = -1.0;
	godot::RID vel_probe;
	void _build_debris_world();
	godot::RigidBody3D *_make_piece();
	void _spawn_event(double p_t);
	void _reshuffle_blocks();
	void _despawn_due();
	void _collect_rids(std::vector<godot::RID> &r_out);

	void _parse_args();
	void _bound(const godot::Vector3 &p_min, const godot::Vector3 &p_max);
	void _compute_camera();
	void _add_camera_and_light();
	void _unpaced_render_off();
	bool need_unpaced_label = false;
	godot::StaticBody3D *_static_box(const godot::Vector3 &p_size, const godot::Vector3 &p_pos);
	void _build_pit(double p_interior, double p_wall_h = 6.0);
	godot::RigidBody3D *_make_dynamic(int p_kind, const godot::Vector3 &p_pos);
	double _jit();
	void _build_pit_bodies(bool p_ring, int p_count = 2000);
	void _build_raycast_statics();
	void _build_rays();
	void _build_chains();
	int _count_awake();
	void _finish();
};
