#include "quest_bench.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/physics_body3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/pin_joint3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/xr_server.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#endif
#include <cmath>
#include <cstdio>

using namespace godot;

uint64_t QuestTickMark::mark_us = 0;
uint64_t QuestTickMark::mark_frame = UINT64_MAX;

void QuestTickMark::_process() {
	mark_us = Time::get_singleton()->get_ticks_usec();
	mark_frame = Engine::get_singleton()->get_process_frames();
}

namespace {
const int SEED = 12345;
const double RAY_LEN = 30.0;
const double SPACING = 0.6;
const int PHYS_HZ = 200; // fixed physics rate (SPEC "Physics rate"): dt = 0.005 s; paced frames run 3 or 4 steps
const int MAX_STEPS_PER_FRAME = 64;
const double TAU_D = 6.283185307179586;

inline uint64_t now_us() {
	return Time::get_singleton()->get_ticks_usec();
}

// nearest rank on sorted values, as in main.gd
double pct(const std::vector<double> &p_sorted, double p) {
	if (p_sorted.empty()) {
		return 0.0;
	}
	int idx = (int)std::ceil(p * (double)p_sorted.size()) - 1;
	idx = std::max(0, std::min(idx, (int)p_sorted.size() - 1));
	return p_sorted[idx];
}

String stats(const String &p_label, std::vector<double> p_a) {
	std::sort(p_a.begin(), p_a.end());
	char buf[256];
	snprintf(buf, sizeof(buf), ": median=%.4f p95=%.4f p99=%.4f max=%.4f\n", pct(p_a, 0.5), pct(p_a, 0.95), pct(p_a, 0.99), pct(p_a, 1.0));
	return p_label + String(buf);
}

// process working set and peak working set in MB (0 when unavailable)
void working_set_mb(double &r_cur, double &r_peak) {
	r_cur = 0.0;
	r_peak = 0.0;
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS pmc;
	pmc.cb = sizeof(pmc);
	if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		r_cur = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
		r_peak = (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
	}
#endif
}

String setting(const char *p_name) {
	return String(ProjectSettings::get_singleton()->get_setting(p_name, "<unset: engine default>"));
}
} // namespace

// ---------- setup ----------
void QuestBench::_parse_args() {
	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	for (int i = 0; i < args.size(); i++) {
		String a = args[i];
		if (!a.begins_with("--") || !a.contains("=")) {
			continue;
		}
		PackedStringArray kv = a.substr(2).split("=", true, 1);
		String k = kv[0];
		String v = kv[1];
		if (k == "scenario") {
			scenario = v;
		} else if (k == "mode") {
			mode = v;
		} else if (k == "run") {
			run_id = v;
		} else if (k == "out") {
			out_dir = v;
		} else if (k == "warmup") {
			warmup_s = v.to_float();
			warmup_given = true;
		} else if (k == "measure") {
			measure_s = v.to_float();
		} else if (k == "flavour") {
			flavour = v;
		} else if (k == "camera") {
			camera_mode = v;
		} else if (k == "sleeping") {
			sleeping_opt = v;
		} else if (k == "profile") {
			profile = v;
		} else if (k == "shadows") {
			shadows_opt = v;
		} else if (k == "shadow-map") {
			shadow_map = v.to_int();
		} else if (k == "msaa") {
			msaa = v.to_int();
		} else if (k == "vsync") {
			vsync_opt = v;
		} else if (k == "diag-busy-ms") {
			diag_busy_ms = v.to_float();
		} else if (k == "bodies") {
			bodies = v.to_int();
		} else if (k == "diag-camera") {
			diag_camera = true;
		} else if (k == "diag-tps") {
			diag_tps = v.to_int();
		} else if (k == "rate") {
			rate = v.to_float();
		} else if (k == "tick-mark") {
			tick_mark_opt = (v != "0" && v != "false");
		} else if (k == "unpaced") {
			unpaced = (v == "1" || v == "true");
		}
	}
}

void QuestBench::_ready() {
	_parse_args();
	if (tick_mark_opt && XRServer::get_singleton() != nullptr) {
		tick_mark.instantiate();
		XRServer::get_singleton()->add_interface(tick_mark);
		tick_mark_added = true;
	}
	base_scenario = scenario;
	file_label = scenario;
	if (scenario == "unlock-render-mixed") {
		base_scenario = "mixed";
		mode = "render";
		unlock_render = true;
		unpaced = false;
	} else if (scenario == "unlock-physics-mixed") {
		base_scenario = "mixed";
		mode = "physics";
		unpaced = true;
	} else if (scenario == "unlock-physics-pit-2000") {
		base_scenario = "pit-2000";
		mode = "physics";
		unpaced = true;
	} else if (scenario == "debris" || scenario == "debris-pooled") {
		base_scenario = "debris";
		is_debris = true;
		pooled = (scenario == "debris-pooled");
		unpaced = false;
		char rb[64];
		snprintf(rb, sizeof(rb), "-%g", rate);
		file_label = scenario + String(rb);
		if (!warmup_given) {
			warmup_s = 10.0; // SPEC G: the debris count needs about 8 s to reach steady state
		}
	} else if (scenario == "scale") {
		if (unpaced) {
			mode = "physics";
		}
		file_label = "scale-" + String::num_int64(bodies);
	} else {
		unpaced = false;
	}
	file_mode = (scenario == "scale" && unpaced) ? String("unlock-physics") : mode;
	render = (mode == "render");
	// Profiles (SPEC "Profiles"): game = shipped-game settings, stress = same render settings with sleeping off for A, D, E, F.
	if (profile != "game" && profile != "stress") {
		UtilityFunctions::push_error("bad --profile value (game|stress): ", profile);
		profile = "game";
	}
	{
		bool throughput_scenario = (base_scenario == "pit-2000" || base_scenario == "mixed" || base_scenario == "scale"); // A, D, E, F
		if (sleeping_opt == "off") {
			sleeping_off = true;
		} else if (sleeping_opt == "default") {
			sleeping_off = false;
		} else {
			sleeping_off = (profile == "stress" && throughput_scenario);
		}
		shadows_on = (shadows_opt != "off");
		// vsync: off in every run (SPEC "Frame rate": render loop unlocked, physics fixed 200 Hz)
		bool vsync_default = false;
		vsync_on = vsync_opt.is_empty() ? vsync_default : (vsync_opt == "on");
	}
	// Physics rate (SPEC "Physics rate"): fixed 200 Hz, up to 64 steps per frame so none are dropped above about 3 fps, jitter fix off so the step count per frame is not smoothed.
	Engine::get_singleton()->set_physics_ticks_per_second(PHYS_HZ);
	Engine::get_singleton()->set_max_physics_steps_per_frame(MAX_STEPS_PER_FRAME);
	Engine::get_singleton()->set_physics_jitter_fix(0.0);
	DisplayServer::get_singleton()->window_set_vsync_mode(vsync_on ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
	if (render) {
		Engine::get_singleton()->set_max_fps(0);
	} else if (unpaced) {
		// Unpaced physics (SPEC E/F): no wall pacing. Godot passes physics_step * time_scale to the physics step, so
		// ticks_per_second = 200 x K and time_scale = K keeps the simulated step at 1/200 s while the loop owes K x 200 ticks
		// per wall second. K = 1000, with max_physics_steps_per_frame = 1: every frame runs exactly one tick (frame time is
		// always above 1/60000 s), excess owed time is dropped. The delta seen in _physics_process is recorded as proof.
		Engine::get_singleton()->set_max_fps(0);
		Engine::get_singleton()->set_max_physics_steps_per_frame(1);
		Engine::get_singleton()->set_physics_ticks_per_second(diag_tps);
		Engine::get_singleton()->set_time_scale((double)diag_tps / (double)PHYS_HZ);
		// Not minimized on purpose: a minimized window (also under --headless) made the loop sleep about 6.9 ms per frame (145 fps cap, measured).
		// Headless has its own sleep (about 6.9 ms per frame, 145 fps cap, measured), so unpaced runs need a real window (no --headless);
		// the render loop is switched off so nothing is drawn, but only 1 s (wall clock, one-shot timer, set up in _ready, no per-frame work)
		// after the world is built: until then a status label is drawn so the window is not an unexplained dark grey rectangle.
		need_unpaced_label = true;
	} else {
		// Physics mode: no meshes, minimized window, loop unlocked.
		Engine::get_singleton()->set_max_fps(0);
		DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_MINIMIZED);
	}
	rng.instantiate();
	rng->set_seed(SEED);
	phys_mat.instantiate();
	phys_mat->set_friction(0.5);
	phys_mat->set_bounce(0.1);
	box_shape.instantiate();
	box_shape->set_size(Vector3(0.5, 0.5, 0.5));
	sphere_shape.instantiate();
	sphere_shape->set_radius(0.25);
	capsule_shape.instantiate();
	capsule_shape->set_radius(0.15);
	capsule_shape->set_height(0.5); // Godot: total height including caps
	if (render) {
		Ref<BoxMesh> bm;
		bm.instantiate();
		bm->set_size(Vector3(0.5, 0.5, 0.5));
		box_mesh = bm;
		Ref<SphereMesh> sm;
		sm.instantiate();
		sm->set_radius(0.25);
		sm->set_height(0.5);
		sphere_mesh = sm;
		Ref<CapsuleMesh> cm;
		cm.instantiate();
		cm->set_radius(0.15);
		cm->set_height(0.5);
		capsule_mesh = cm;
		Ref<BoxMesh> ub;
		ub.instantiate();
		ub->set_size(Vector3(1, 1, 1));
		unit_box_mesh = ub;
	}
	if (base_scenario == "pit-2000") {
		pit_interior = 22.0;
		_build_pit(22.0);
		_build_pit_bodies(false);
	} else if (base_scenario == "scale") {
		// Scenario A bodies with N changed (SPEC F layout rule, same as Stride): pit interior = max(22, 22 x sqrt(N / 2000) rounded up to an even number of metres),
		// wall height = max(6, ceil(half the initial column height)), column height = 1 + layers x 0.6, layers = ceil(N / 144).
		pit_interior = std::max(22.0, std::ceil(22.0 * std::sqrt((double)bodies / 2000.0) / 2.0) * 2.0);
		double scale_wall_h = std::max(6.0, std::ceil((1.0 + (double)((bodies + 143) / 144) * 0.6) * 0.5));
		_build_pit(pit_interior, scale_wall_h);
		_build_pit_bodies(false, bodies);
	} else if (base_scenario == "debris") {
		pit_interior = 22.0;
		_build_pit(22.0);
		_build_debris_world();
	} else if (base_scenario == "raycast-1000") {
		ray_count = 1000;
		_build_raycast_statics();
		_build_rays();
	} else if (base_scenario == "raycast-10000") {
		ray_count = 10000;
		_build_raycast_statics();
		_build_rays();
	} else if (base_scenario == "chains") {
		_build_chains();
	} else if (base_scenario == "mixed") {
		ray_count = 1000;
		pit_interior = 32.0;
		_build_pit(32.0);
		_build_chains();
		_build_pit_bodies(true);
		_build_rays();
	} else {
		UtilityFunctions::push_error("unknown scenario: ", scenario);
		get_tree()->quit(2);
		return;
	}
	// camera after the scenario has set its geometry (the bounds come from the builders)
	_compute_camera();
	if (render) {
		_add_camera_and_light();
	}
	if (need_unpaced_label) {
		Label *lb = memnew(Label);
		lb->set_text(String("Unpaced physics run (") + scenario + "): the render loop is switched off on purpose, no frames are drawn.\nThis window stays dark grey until the run ends.");
		lb->set_position(Vector2(16, 16));
		add_child(lb);
		get_tree()->create_timer(1.0, true, false, true)->connect("timeout", callable_mp(this, &QuestBench::_unpaced_render_off));
	}
	last_us = now_us();
	phase_start_us = last_us;
}

// ---------- builders ----------
void QuestBench::_unpaced_render_off() {
	RenderingServer::get_singleton()->set_render_loop_enabled(false);
}

void QuestBench::_bound(const Vector3 &p_min, const Vector3 &p_max) {
	if (!bounds_valid) {
		bmin = p_min;
		bmax = p_max;
		bounds_valid = true;
		return;
	}
	bmin = Vector3(std::min(bmin.x, p_min.x), std::min(bmin.y, p_min.y), std::min(bmin.z, p_min.z));
	bmax = Vector3(std::max(bmax.x, p_max.x), std::max(bmax.y, p_max.y), std::max(bmax.z, p_max.z));
}

// Shared camera rule (SPEC "Camera"): content bounding sphere = centre and half diagonal of the content AABB; the camera looks
// at the centre from direction u = normalize(0, 1.5, 0.9) at D = 1.15 * R / sin(30 deg).
void QuestBench::_compute_camera() {
	if (camera_mode != "content" && camera_mode != "sky") {
		UtilityFunctions::push_error("bad --camera value (content|sky): ", camera_mode);
		camera_mode = "content";
	}
	Vector3 c = (bmin + bmax) * 0.5;
	cam_radius = (bmax - bmin).length() * 0.5;
	Vector3 u = Vector3(0, 1.5, 0.9).normalized();
	double d = 1.15 * cam_radius / std::sin(30.0 * 3.14159265358979323846 / 180.0);
	cam_pos = c + u * d;
	cam_look_at = (camera_mode == "sky") ? cam_pos + Vector3(0, 1, 0) : c;
}

void QuestBench::_add_camera_and_light() {
	DirectionalLight3D *light = memnew(DirectionalLight3D);
	add_child(light);
	// Light direction equal to the Stride scene (yaw 30, pitch -50 degrees: direction = (-sin30 cos50, -sin50, -cos30 cos50), SPEC "Settings parity").
	light->look_at_from_position(Vector3(), Vector3(-0.5 * 0.6427876097, -0.7660444431, -0.8660254038 * 0.6427876097));
	// Profile render settings (SPEC "Profiles"): one directional light with shadows (4 splits, shadow map size from --shadow-map, max distance
	// covers the content from the camera), MSAA from --msaa. Everything else stays at the engine default and is read back below for env.txt.
	double shadow_dist = std::ceil(cam_radius * 1.15 / std::sin(30.0 * 3.14159265358979323846 / 180.0) + cam_radius);
	light->set_shadow(shadows_on);
	light->set_shadow_mode(DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS);
	light->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, shadow_dist);
	RenderingServer::get_singleton()->directional_shadow_atlas_set_size(shadow_map, false);
	Viewport *vp = get_viewport();
	Viewport::MSAA msaa_enum = msaa >= 8 ? Viewport::MSAA_8X : msaa >= 4 ? Viewport::MSAA_4X : msaa >= 2 ? Viewport::MSAA_2X : Viewport::MSAA_DISABLED;
	vp->set_msaa_3d(msaa_enum);
	// Background and ambient light close to the Stride side (cornflower blue clear colour, some ambient so wall sides are not black).
	Ref<Environment> env;
	env.instantiate();
	env->set_background(Environment::BG_COLOR);
	env->set_bg_color(Color(0.392, 0.584, 0.929));
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_ambient_light_color(Color(1, 1, 1));
	env->set_ambient_light_energy(0.5);
	WorldEnvironment *we = memnew(WorldEnvironment);
	we->set_environment(env);
	add_child(we);
	Camera3D *cam = memnew(Camera3D);
	add_child(cam);
	// shared camera rule (SPEC "Camera"): perspective, vertical FOV 60, near 0.1, far 2000
	cam->set_projection(Camera3D::PROJECTION_PERSPECTIVE);
	cam->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);
	cam->set_fov(60.0);
	cam->set_near(0.1);
	cam->set_far(2000.0);
	if (camera_mode == "sky") {
		// control: same position, straight up (forward = +Y, up = -Z), no content in view
		cam->set_global_transform(Transform3D(Basis(Vector3(1, 0, 0), 3.14159265358979323846 / 2.0), cam_pos));
	} else {
		cam->look_at_from_position(cam_pos, cam_look_at, Vector3(0, 1, 0));
	}
	// read back what is in effect (no guessed defaults)
	{
		Vector3 ld = -light->get_global_transform().basis.get_column(2);
		Color lc = light->get_color();
		Color ac = env->get_ambient_light_color();
		auto f3 = [](const Vector3 &v) { return String("(%.4f, %.4f, %.4f)") % Array::make(v.x, v.y, v.z); };
		auto fc = [](const Color &c) { return String("(%.3f, %.3f, %.3f)") % Array::make(c.r, c.g, c.b); };
		String L;
		L += "parity.shadows: " + String(light->has_shadow() ? "on" : "off") + "\n";
		L += "shadows: " + String(light->has_shadow() ? "on" : "off") + "\n";
		L += "parity.shadow_technique: Godot directional shadow map, parallel-split cascades (engine default filtering " + String::num_int64(ProjectSettings::get_singleton()->get_setting("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality", -1)) + ")\n";
		L += "parity.shadow_map_size: " + String::num_int64(shadow_map) + "\n";
		L += "shadow_map_size: " + String::num_int64(shadow_map) + "\n";
		L += "parity.shadow_cascades: " + String::num_int64((int)light->get_shadow_mode() == (int)DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS ? 4 : (int)light->get_shadow_mode() == (int)DirectionalLight3D::SHADOW_PARALLEL_2_SPLITS ? 2 : 1) + "\n";
		L += String("parity.shadow_max_distance: %.1f\n") % Array::make(light->get_param(Light3D::PARAM_SHADOW_MAX_DISTANCE));
		L += String("parity.shadow_bias_normal_bias: %.3f / %.3f (engine defaults, read back)\n") % Array::make(light->get_param(Light3D::PARAM_SHADOW_BIAS), light->get_param(Light3D::PARAM_SHADOW_NORMAL_BIAS));
		L += "parity.light_direction: " + f3(ld.normalized()) + "\n";
		L += String("parity.light_intensity: %.3f\n") % Array::make(light->get_param(Light3D::PARAM_ENERGY));
		L += "parity.light_color: " + fc(lc) + "\n";
		L += "parity.ambient_color: " + fc(ac) + "\n";
		L += String("parity.ambient_energy: %.3f\n") % Array::make(env->get_ambient_light_energy());
		L += "parity.tonemap: " + String::num_int64((int)env->get_tonemapper()) + " (Godot Environment.tonemapper enum, 0 = linear) exposure " + String::num(env->get_tonemap_exposure(), 3) + "\n";
		L += "parity.msaa: " + String::num_int64(msaa_enum == Viewport::MSAA_8X ? 8 : msaa_enum == Viewport::MSAA_4X ? 4 : msaa_enum == Viewport::MSAA_2X ? 2 : 0) + "x (viewport msaa_3d read back " + String::num_int64((int)vp->get_msaa_3d()) + ")\n";
		L += "msaa: " + String::num_int64(msaa_enum == Viewport::MSAA_8X ? 8 : msaa_enum == Viewport::MSAA_4X ? 4 : msaa_enum == Viewport::MSAA_2X ? 2 : 0) + "x\n";
		L += "parity.render_scale: " + String::num(vp->get_scaling_3d_scale(), 3) + "\n";
		L += "parity.material: engine default material on every mesh (no material assigned; Godot default StandardMaterial3D-like white, roughness 1 / metallic 0 not read back)\n";
		L += "parity.cast_shadow_meshes: GeometryInstance3D default (cast shadows on), receive is automatic\n";
		render_env_lines = L;
	}
	if (diag_camera) {
		Transform3D t = cam->get_global_transform();
		Vector3 f = -t.basis.get_column(2);
		UtilityFunctions::print("diag_camera: scenario=", scenario, " pit_interior=", pit_interior, " pos=", t.origin, " forward=", f, " euler_deg=", t.basis.get_euler() * (180.0 / 3.14159265358979), " fov=", cam->get_fov(), " keep_aspect=", (int)cam->get_keep_aspect_mode(), " near=", cam->get_near(), " far=", cam->get_far(), " projection=", (int)cam->get_projection(), " viewport=", get_viewport()->get_visible_rect().size);
	}
}

StaticBody3D *QuestBench::_static_box(const Vector3 &p_size, const Vector3 &p_pos) {
	StaticBody3D *b = memnew(StaticBody3D);
	b->set_physics_material_override(phys_mat);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<BoxShape3D> s;
	s.instantiate();
	s->set_size(p_size);
	cs->set_shape(s);
	b->add_child(cs);
	if (render) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(unit_box_mesh);
		mi->set_scale(p_size);
		b->add_child(mi);
	}
	_bound(p_pos - p_size * 0.5, p_pos + p_size * 0.5);
	b->set_position(p_pos);
	add_child(b);
	return b;
}

void QuestBench::_build_pit(double p_interior, double p_wall_h) {
	_static_box(Vector3(p_interior + 2.0, 1.0, p_interior + 2.0), Vector3(0, -0.5, 0));
	double h = p_interior / 2.0 + 0.5;
	double wy = p_wall_h / 2.0;
	_static_box(Vector3(1, p_wall_h, p_interior + 2.0), Vector3(h, wy, 0));
	_static_box(Vector3(1, p_wall_h, p_interior + 2.0), Vector3(-h, wy, 0));
	_static_box(Vector3(p_interior, p_wall_h, 1), Vector3(0, wy, h));
	_static_box(Vector3(p_interior, p_wall_h, 1), Vector3(0, wy, -h));
}

RigidBody3D *QuestBench::_make_dynamic(int p_kind, const Vector3 &p_pos) {
	RigidBody3D *b = memnew(RigidBody3D);
	b->set_mass(1.0);
	if (sleeping_off) {
		b->set_can_sleep(false);
	}
	b->set_physics_material_override(phys_mat);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<Mesh> mesh;
	switch (p_kind) {
		case 0:
			cs->set_shape(box_shape);
			mesh = box_mesh;
			break;
		case 1:
			cs->set_shape(sphere_shape);
			mesh = sphere_mesh;
			break;
		default:
			cs->set_shape(capsule_shape);
			mesh = capsule_mesh;
			break;
	}
	b->add_child(cs);
	if (render) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(mesh);
		b->add_child(mi);
	}
	b->set_position(p_pos);
	body_rids.push_back(b->get_rid());
	return b;
}

double QuestBench::_jit() {
	return rng->randf_range(-0.05, 0.05);
}

void QuestBench::_build_pit_bodies(bool p_ring, int p_count) {
	int n = 0;
	if (!p_ring) {
		// 12 x 12 columns (6.6 m span, inside the 22 m pit), layers from y = 1 until 2000 bodies; same rule as Stride BuildPit(22, 12, 0).
		int layer0 = 0;
		while (n < p_count) {
			for (int i = 0; i < 12; i++) {
				for (int j = 0; j < 12; j++) {
					if (n >= p_count) {
						break;
					}
					// jitter draws in x, y, z order (C++ argument evaluation order is unspecified, so sequence them)
					double jx = _jit();
					double jy = _jit();
					double jz = _jit();
					Vector3 p((i - 5.5) * SPACING + jx, 1.0 + layer0 * SPACING + jy, (j - 5.5) * SPACING + jz);
					add_child(_make_dynamic(n % 2, p));
					n += 1;
				}
			}
			layer0 += 1;
		}
		// spawn volume (SPEC "Camera"): columns +-(5.5 x 0.6 + 0.3), layers up to y = 1 + (L - 1) x 0.6 + 0.3
		_bound(Vector3(-3.6, 0.7, -3.6), Vector3(3.6, 1.0 + (layer0 - 1) * SPACING + 0.3, 3.6));
		return;
	}
	// mixed: columns on a +-9 m grid, skipping |x|<6 and |z|<6 (chain area), layers until 2000 bodies.
	std::vector<Vector2> cols;
	int steps = (int)(9.0 / SPACING);
	for (int i = -steps; i <= steps; i++) {
		for (int j = -steps; j <= steps; j++) {
			double x = i * SPACING;
			double z = j * SPACING;
			if (std::fabs(x) < 6.0 && std::fabs(z) < 6.0) {
				continue;
			}
			cols.push_back(Vector2(x, z));
		}
	}
	int layer = 0;
	while (n < p_count) {
		for (const Vector2 &c : cols) {
			if (n >= p_count) {
				break;
			}
			double jx = _jit();
			double jy = _jit();
			double jz = _jit();
			Vector3 p(c.x + jx, 1.0 + layer * SPACING + jy, c.y + jz);
			add_child(_make_dynamic(n % 2, p));
			n += 1;
		}
		layer += 1;
	}
	// spawn volume (SPEC "Camera"): ring columns within +-(steps x 0.6 + 0.3), layers up to y = 1 + (L - 1) x 0.6 + 0.3
	_bound(Vector3(-(steps * SPACING + 0.3), 0.7, -(steps * SPACING + 0.3)), Vector3(steps * SPACING + 0.3, 1.0 + (layer - 1) * SPACING + 0.3, steps * SPACING + 0.3));
}

void QuestBench::_build_raycast_statics() {
	// content volume (SPEC "Camera"): 40 x 20 x 40 volume of the statics plus 0.5 m for the shape extents
	_bound(Vector3(-20.5, -0.5, -20.5), Vector3(20.5, 20.5, 20.5));
	for (int i = 0; i < 2000; i++) {
		double px = rng->randf_range(-20, 20);
		double py = rng->randf_range(0, 20);
		double pz = rng->randf_range(-20, 20);
		Vector3 pos(px, py, pz);
		double ax = rng->randf_range(-1, 1);
		double ay = rng->randf_range(-1, 1);
		double az = rng->randf_range(-1, 1);
		Vector3 axis(ax, ay, az);
		if (axis.length_squared() < 0.0001) {
			axis = Vector3(0, 1, 0);
		}
		double angle = rng->randf_range(0.0, TAU_D);
		Basis bas(axis.normalized(), angle);
		StaticBody3D *b = memnew(StaticBody3D);
		b->set_physics_material_override(phys_mat);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		Ref<Mesh> mesh;
		switch (i % 3) {
			case 0:
				cs->set_shape(box_shape);
				mesh = box_mesh;
				break;
			case 1:
				cs->set_shape(capsule_shape);
				mesh = capsule_mesh;
				break;
			default:
				cs->set_shape(sphere_shape);
				mesh = sphere_mesh;
				break;
		}
		b->add_child(cs);
		if (render) {
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(mesh);
			b->add_child(mi);
		}
		b->set_transform(Transform3D(bas, pos));
		add_child(b);
	}
}

void QuestBench::_build_rays() {
	ray_from.resize(ray_count);
	ray_to.resize(ray_count);
	for (int i = 0; i < ray_count; i++) {
		double ox = rng->randf_range(-20, 20);
		double oy = rng->randf_range(0, 20);
		double oz = rng->randf_range(-20, 20);
		Vector3 o(ox, oy, oz);
		double dx = rng->randf_range(-1, 1);
		double dy = rng->randf_range(-1, 1);
		double dz = rng->randf_range(-1, 1);
		Vector3 d(dx, dy, dz);
		if (d.length_squared() < 0.0001) {
			d = Vector3(0, -1, 0);
		}
		ray_from[i] = o;
		ray_to[i] = o + d.normalized() * RAY_LEN;
	}
	ray_params = PhysicsRayQueryParameters3D::create(Vector3(), Vector3(1, 1, 1));
}

void QuestBench::_build_chains() {
	// chain volume (SPEC "Camera"): links hang x, z within +-(4.5 + 0.15), y from 12 down to 12 - 0.25 - 4.5 - 0.25 = 7.0
	_bound(Vector3(-4.65, 7.0, -4.65), Vector3(4.65, 12.0, 4.65));
	// Static ceiling bar: slab 11 x 0.2 x 11, bottom face at y = 12.
	StaticBody3D *ceiling = _static_box(Vector3(11, 0.2, 11), Vector3(0, 12.1, 0));
	for (int cx = 0; cx < 10; cx++) {
		for (int cz = 0; cz < 10; cz++) {
			double x = (cx - 4.5) * 1.0;
			double z = (cz - 4.5) * 1.0;
			PhysicsBody3D *prev = ceiling;
			for (int l = 0; l < 10; l++) {
				RigidBody3D *link = _make_dynamic(2, Vector3(x, 12.0 - 0.25 - 0.5 * l, z));
				chain_rids.push_back(link->get_rid());
				add_child(link);
				PinJoint3D *j = memnew(PinJoint3D);
				j->set_position(Vector3(x, 12.0 - 0.5 * l, z));
				add_child(j);
				j->set_node_a(prev->get_path());
				j->set_node_b(link->get_path());
				prev = link;
				if (l == 9) {
					impulse_bodies.push_back(link);
				}
			}
		}
	}
}

// ---------- scenario G: debris ----------
void QuestBench::_build_debris_world() {
	// 25 intact blocks 2 x 2 x 2, 5 x 5 grid, spacing 3, centred on the pit, standing on the floor (centre y = 1).
	blocks.assign(25, nullptr);
	live_at.assign(25, 0);
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			blocks[i * 5 + j] = _static_box(Vector3(2, 2, 2), Vector3((i - 2) * 3.0, 1.0, (j - 2) * 3.0));
		}
	}
	block_order.resize(25);
	_reshuffle_blocks();
	if (pooled) {
		// enough pieces for the maximum number of events alive at once (lifetime 8 s) plus slack
		int64_t events_alive = (int64_t)std::ceil(rate * 8.0) + 2;
		pool_size0 = events_alive * 64;
		// same semantics as the Stride side: parked pieces stay in the simulation, asleep, far below the pit (y = -1000)
		for (int64_t i = 0; i < pool_size0; i++) {
			RigidBody3D *p = _make_piece();
			p->set_position(Vector3((double)(i % 64), -1000.0, (double)(i / 64)));
			add_child(p);
			p->set_sleeping(true);
			pool_free.push_back(p);
		}
	}
}

void QuestBench::_reshuffle_blocks() {
	// seeded permutation of the 25 blocks (Fisher-Yates with the benchmark RNG), reshuffled after each 25 draws (as the Stride side)
	for (int i = 0; i < 25; i++) {
		block_order[i] = i;
	}
	for (int i = 24; i > 0; i--) {
		int k = rng->randi_range(0, i);
		std::swap(block_order[i], block_order[k]);
	}
}

RigidBody3D *QuestBench::_make_piece() {
	RigidBody3D *b = memnew(RigidBody3D);
	b->set_mass(0.125);
	if (sleeping_off) {
		b->set_can_sleep(false);
	}
	b->set_physics_material_override(phys_mat);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	cs->set_shape(box_shape); // shared 0.5 box shape
	b->add_child(cs);
	if (render) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(box_mesh); // shared 0.5 box mesh resource, one MeshInstance3D per piece
		b->add_child(mi);
	}
	return b;
}

void QuestBench::_spawn_event(double p_t) {
	uint64_t t0 = now_us();
	if (events_total > 0 && events_total % 25 == 0) {
		_reshuffle_blocks();
	}
	int blk = block_order[events_total % 25];
	Vector3 c((blk / 5 - 2) * 3.0, 1.0, (blk % 5 - 2) * 3.0);
	if (blocks[blk] != nullptr) {
		// remove the intact block (freed)
		remove_child(blocks[blk]);
		memdelete(blocks[blk]);
		blocks[blk] = nullptr;
		events_on_intact++;
	} else {
		// the block of this spot is already gone (its earlier debris is still alive): spawn at the same place, nothing to remove
		events_on_used++;
	}
	live_at[blk]++;
	DebrisEvent ev;
	ev.t_event = p_t;
	ev.block = blk;
	ev.pieces.reserve(64);
	ev.rids.reserve(64);
	for (int ix = 0; ix < 4; ix++) {
		for (int iy = 0; iy < 4; iy++) {
			for (int iz = 0; iz < 4; iz++) {
				Vector3 off((ix - 1.5) * 0.5, (iy - 1.5) * 0.5, (iz - 1.5) * 0.5);
				RigidBody3D *p;
				bool pooled_reused = false;
				if (pooled && !pool_free.empty()) {
					p = pool_free.back();
					pool_free.pop_back();
					pooled_reused = true;
					ev.parks.push_back(p->get_position());
				} else {
					if (pooled) {
						pool_misses++;
					}
					p = _make_piece();
				}
				p->set_transform(Transform3D(Basis(), c + off));
				if (pooled_reused) {
					p->set_sleeping(false);
				} else {
					add_child(p);
				}
				// RNG order per piece: impulse factor, then spin x, y, z
				double f = rng->randf_range(-0.2, 0.2);
				double sx = rng->randf_range(-3.0, 3.0);
				double sy = rng->randf_range(-3.0, 3.0);
				double sz = rng->randf_range(-3.0, 3.0);
				// radial impulse from the block centre: 1.5 m/s (impulse 1.5 x mass) times 1 +- 0.2
				p->apply_central_impulse(off.normalized() * (1.5 * 0.125 * (1.0 + f)));
				p->set_angular_velocity(Vector3(sx, sy, sz));
				ev.pieces.push_back(p);
				ev.rids.push_back(p->get_rid());
			}
		}
	}
	if (vel_probe == RID() && recording) {
		// proof that the impulse reaches the body: speed of a probe piece right after the call and at the next tick
		vel_probe = ev.rids[63];
		vel_after_spawn = ((Vector3)PhysicsServer3D::get_singleton()->body_get_state(vel_probe, PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY)).length();
	}
	live_pieces += 64;
	live_events.push_back(std::move(ev));
	events_total++;
	frame_event_us += now_us() - t0;
	frame_event = true;
	if (recording) {
		events_rec++;
	}
}

void QuestBench::_despawn_due() {
	// lifetime 8 s of simulated time (simT >= event time + 8, simT = ticks completed / 200); all pieces of an event are removed in the same frame
	while (!live_events.empty() && (double)(tick_global - 1) / (double)PHYS_HZ >= live_events.front().t_event + 8.0) {
		uint64_t t0 = now_us();
		DebrisEvent &ev = live_events.front();
		for (size_t pi = 0; pi < ev.pieces.size(); pi++) {
			RigidBody3D *p = ev.pieces[pi];
			if (pooled) {
				// park: zero velocities, teleport below the pit, sleep (stays in the simulation)
				p->set_linear_velocity(Vector3());
				p->set_angular_velocity(Vector3());
				p->set_transform(Transform3D(Basis(), ev.parks[pi]));
				p->set_sleeping(true);
				pool_free.push_back(p);
			} else {
				remove_child(p);
				memdelete(p);
			}
		}
		live_pieces -= (int64_t)ev.pieces.size();
		int blk = ev.block;
		live_at[blk]--;
		if (live_at[blk] == 0 && blocks[blk] == nullptr) {
			// rebuild the block (intact again) once no debris of this spot is left
			blocks[blk] = _static_box(Vector3(2, 2, 2), Vector3((blk / 5 - 2) * 3.0, 1.0, (blk % 5 - 2) * 3.0));
		}
		live_events.pop_front();
		removals_total++;
		frame_despawn_us += now_us() - t0;
		frame_despawn = true;
		if (recording) {
			removals_rec++;
		}
	}
}

void QuestBench::_collect_rids(std::vector<RID> &r_out) {
	if (is_debris) {
		for (const DebrisEvent &ev : live_events) {
			r_out.insert(r_out.end(), ev.rids.begin(), ev.rids.end());
		}
	} else {
		r_out = body_rids;
	}
}

void QuestBench::_exit_tree() {
	if (tick_mark.is_valid()) {
		if (XRServer::get_singleton() != nullptr) {
			XRServer::get_singleton()->remove_interface(tick_mark);
		}
		tick_mark.unref();
		tick_mark_added = false;
	}
}

// ---------- loop ----------
void QuestBench::_physics_process(double p_delta) {
	if (space_class.is_empty()) {
		// runtime proof of the engine in use: the direct space state is created by the active physics server and its class name is the server's own
		space_class = get_world_3d()->get_direct_space_state()->get_class();
	}
	uint64_t t_start = now_us();
	if (tick_in_frame > 0) {
		// second tick in the same frame: the gap since the previous tick's end is that tick's step
		step_acc_us += t_start - phys_end_us;
		if (recording) {
			step_list.push_back((double)(t_start - phys_end_us + lead_pending_us) / 1000.0);
		}
		lead_pending_us = 0;
	} else if (tick_mark_added) {
		// first tick of the frame: tick-start window = QuestTickMark stamp (XRServer::_process, before the physics loop) to here
		// (iteration_prepare, sync, flush_queries and any node ahead of us in physics_process)
		uint64_t lead = 0;
		if (QuestTickMark::mark_frame == Engine::get_singleton()->get_process_frames() && t_start >= QuestTickMark::mark_us) {
			lead = t_start - QuestTickMark::mark_us;
		} else if (recording) {
			lead_missing += 1;
		}
		step_acc_us += lead;
		lead_pending_us = lead;
		lead_frame_us = lead;
	}
	tick_in_frame += 1;
	if (recording) {
		ticks_rec += 1;
		delta_min = std::min(delta_min, p_delta);
		delta_max = std::max(delta_max, p_delta);
	}
	if (!engine_printed) {
		engine_printed = true;
		UtilityFunctions::print("QuestBench engine proof: setting physics/3d/physics_engine=", setting("physics/3d/physics_engine"), " ; direct space state class=", space_class);
	}
	if (!impulse_done && !impulse_bodies.empty()) {
		impulse_done = true;
		for (RigidBody3D *b : impulse_bodies) {
			impulse_pos0.push_back(b->get_global_position());
			b->apply_central_impulse(Vector3(2, 0, 0));
		}
	}
	tick_global++;
	if (is_debris) {
		if (vel_next_tick < 0.0 && vel_after_spawn >= 0.0) {
			vel_next_tick = ((Vector3)PhysicsServer3D::get_singleton()->body_get_state(vel_probe, PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY)).length();
		}
		_despawn_due();
		// event k is due at simulated time k / R (first at t = 0), simT = ticks completed before this one / 200
		double sim_t = (double)(tick_global - 1) / (double)PHYS_HZ;
		while ((double)next_event_k / rate <= sim_t + 1e-9) {
			_spawn_event((double)next_event_k / rate);
			next_event_k++;
		}
	}
	if (ray_count > 0) {
		PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
		int hits = 0;
		uint64_t t0 = now_us();
		for (int i = 0; i < ray_count; i++) {
			ray_params->set_from(ray_from[i]);
			ray_params->set_to(ray_to[i]);
			if (!space->intersect_ray(ray_params).is_empty()) {
				hits++;
			}
		}
		query_ms_last = (double)(now_us() - t0) / 1000.0;
		query_ms_frame += query_ms_last;
		if (recording) {
			total_hits += hits;
		}
	}
	if (diag_busy_ms > 0.0) {
		uint64_t until = now_us() + (uint64_t)(diag_busy_ms * 1000.0);
		while (now_us() < until) {
		}
	}
	phys_end_us = now_us();
	script_ms_last = (double)(phys_end_us - t_start) / 1000.0;
	if (recording) {
		script_cost.push_back(script_ms_last);
	}
}

int QuestBench::_count_awake() {
	int n = 0;
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	if (is_debris) {
		for (const DebrisEvent &ev : live_events) {
			for (const RID &rid : ev.rids) {
				if (!(bool)ps->body_get_state(rid, PhysicsServer3D::BODY_STATE_SLEEPING)) {
					n++;
				}
			}
		}
		return n;
	}
	for (const RID &rid : body_rids) {
		if (!(bool)ps->body_get_state(rid, PhysicsServer3D::BODY_STATE_SLEEPING)) {
			n++;
		}
	}
	return n;
}

void QuestBench::_process(double) {
	if (finished) {
		return;
	}
	uint64_t now = now_us();
	double wall_ms = (double)(now - last_us) / 1000.0;
	last_us = now;
	int steps_now = tick_in_frame;
	double query_now = query_ms_frame;
	query_ms_frame = 0.0;
	bool ticked = tick_in_frame > 0;
	double step_ms = 0.0;
	if (ticked) {
		step_ms = (double)(step_acc_us + (now - phys_end_us)) / 1000.0;
	}
	if (ticked && recording) {
		step_list.push_back((double)(now - phys_end_us + lead_pending_us) / 1000.0); // the last step of the frame
	}
	double tick_start_now = ticked ? (double)lead_frame_us / 1000.0 : 0.0;
	lead_pending_us = lead_frame_us = 0;
	tick_in_frame = 0;
	step_acc_us = 0;
	bool ev_frame = frame_event, dsp_frame = frame_despawn;
	double ev_ms = (double)frame_event_us / 1000.0, dsp_ms = (double)frame_despawn_us / 1000.0;
	frame_event_us = frame_despawn_us = 0;
	frame_event = frame_despawn = false;
	if (!recording) {
		if ((double)(now - phase_start_us) / 1.0e6 >= warmup_s) {
			recording = true;
			phase_start_us = now;
			phys_frames_start = Engine::get_singleton()->get_physics_frames();
		}
		return;
	}
	// Unpaced runs tick on every frame, so the awake count (about 0.3 ms for 3000 bodies) is refreshed every 30th tick only.
	if (ticked && (!unpaced || frame_idx % 30 == 0)) {
		uint64_t t1 = now_us();
		awake_last = _count_awake();
		awake_cost.push_back((double)(now_us() - t1) / 1000.0);
	}
	double mon = Performance::get_singleton()->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0;
	wall.push_back(wall_ms);
	phys.push_back(step_ms);
	mon_phys.push_back(mon);
	if (ticked) {
		phys_ticks.push_back(step_ms);
		steps_pf_list.push_back((double)steps_now);
	}
	cum_ms += wall_ms;
	row_cum_ms.push_back(cum_ms);
	row_ticked.push_back(ticked ? 1 : 0);
	rows.push_back({ frame_idx, wall_ms, step_ms, query_now, awake_last, ev_ms, dsp_ms, (int)live_pieces, steps_now, steps_now > 0 ? step_ms / (double)steps_now : 0.0, tick_start_now });
	if (ticked) {
		tick_start_list.push_back(tick_start_now);
	}
	if (is_debris) {
		row_event.push_back(ev_frame ? 1 : 0);
		row_despawn.push_back(dsp_frame ? 1 : 0);
		if (ev_frame) {
			event_ms_list.push_back(ev_ms);
			wall_event.push_back(wall_ms);
		}
		if (dsp_frame) {
			despawn_ms_list.push_back(dsp_ms);
			wall_despawn.push_back(wall_ms);
		}
		if (!ev_frame) {
			wall_nonevent.push_back(wall_ms);
		}
	}
	frame_idx += 1;
	if ((double)(now - phase_start_us) / 1.0e6 >= measure_s) {
		_finish();
	}
}

// ---------- output ----------
void QuestBench::_finish() {
	finished = true;
	set_process(false);
	set_physics_process(false);
	String abs_dir = out_dir;
	if (out_dir.begins_with("user://") || out_dir.begins_with("res://")) {
		abs_dir = ProjectSettings::get_singleton()->globalize_path(out_dir);
	}
	DirAccess::make_dir_recursive_absolute(abs_dir);
	String ps_class = space_class.is_empty() ? String("<not captured>") : space_class;
	String eng_setting = setting("physics/3d/physics_engine");
	String eng_tag = "unknown";
	if (ps_class.contains("PhysX")) {
		eng_tag = "physx";
	} else if (ps_class.contains("Jolt")) {
		eng_tag = "jolt";
	} else if (ps_class.contains("GodotPhysics")) {
		eng_tag = "godotphysics";
	}
	String base = abs_dir.path_join(String("godot-%s%s%s-%s-%s-%s") % Array::make(eng_tag, flavour.is_empty() ? String("") : String("-") + flavour, profile == "stress" ? String("-stress") : String(""), file_label, file_mode, run_id));
	Ref<FileAccess> f = FileAccess::open(base + ".csv", FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::push_error("cannot open csv: ", base + ".csv");
		get_tree()->quit(3);
		return;
	}
	f->store_line(String("frame,wall_ms,physics_ms,query_ms,gc0,gc1,gc2,gc_pause_ms,awake_bodies") + (is_debris ? ",event_ms,despawn_ms,debris_live" : "") + ",steps_per_frame,physics_step_ms,tick_start_ms");
	char buf[256];
	for (const Row &r : rows) {
		snprintf(buf, sizeof(buf), "%d,%.4f,%.4f,%.4f,0,0,0,0,%d", r.frame, r.wall_ms, r.step_ms, r.query_ms, r.awake);
		if (is_debris) {
			snprintf(buf, sizeof(buf), "%d,%.4f,%.4f,%.4f,0,0,0,0,%d,%.4f,%.4f,%d", r.frame, r.wall_ms, r.step_ms, r.query_ms, r.awake, r.event_ms, r.despawn_ms, r.live);
		}
		char buf2[96];
		snprintf(buf2, sizeof(buf2), ",%d,%.4f,%.4f", r.steps, r.step_mean_ms, r.tick_start_ms);
		f->store_line(String(buf) + String(buf2));
	}
	f.unref();
	double m120 = 0.0;
	for (size_t i = 0; i < wall.size() && i < 120; i++) {
		m120 = std::max(m120, wall[i]);
	}
	Dictionary v = Engine::get_singleton()->get_version_info();
	String sum = String("engine=Godot %s backend=%s scenario=%s mode=%s run=%s frames=%d\n") % Array::make(v.get("string", "?"), eng_tag, file_label, file_mode, run_id, frame_idx);
	sum += "profile=" + profile + " sleeping=" + String(sleeping_off ? "off" : "default") + " vsync=" + String(vsync_on ? "on" : "off") + "\n";
	sum += stats("wall_ms", wall);
	sum += stats("physics_ms (own stopwatch: def 2 = first tick's tick-start window (sync + flush_queries, from the QuestTickMark stamp) + end of _physics_process to start of _process; def 1 without the mark; 0 on frames without a tick)", phys);
	sum += stats("engine_monitor_TIME_PHYSICS_PROCESS_ms (engine updates it ~1 Hz; not used in the CSV)", mon_phys);
	sum += stats("physics_ms, tick frames only", phys_ticks);
	sum += stats("physics_frame_ms (= physics_ms column: sum of the steps in one frame, tick frames only)", phys_ticks);
	sum += stats("physics_step_ms (stopwatch of every single step, same boundaries as physics_ms; the first step of a frame includes the tick-start window)", step_list);
	sum += stats("tick_start_ms (first tick's tick-start window inside physics_ms, tick frames only; all 0 with --tick-mark=0 or when the mark is missing)", tick_start_list);
	sum += stats("steps_per_frame (tick frames only)", steps_pf_list);
	sum += "physics_hz: " + String::num_int64(unpaced ? PHYS_HZ : Engine::get_singleton()->get_physics_ticks_per_second()) + (unpaced ? " (simulated step; engine ticks_per_second x time_scale, see pacing line)" : "") + "\n";
	sum += stats("awake_count_cost_ms (native cost of counting awake bodies, tick frames only)", awake_cost);
	sum += stats("script_physics_process_ms (our _physics_process incl. ray batch and diag busy-wait, tick frames only)", script_cost);
	sum += "total_gc_pause_ms: 0 (native C++ GDExtension, no GC)\n";
	snprintf(buf, sizeof(buf), "first_120_frames_max_wall_ms: %.4f\n", m120);
	sum += buf;
	snprintf(buf, sizeof(buf), "ray_hits_total (measured window): %lld\n", (long long)total_hits);
	sum += buf;
	// ---- SPEC E/F lines ----
	{
		double measured_ms = cum_ms;
		double measured_s = measured_ms / 1000.0;
		snprintf(buf, sizeof(buf), "measured_s: %.4f\n", measured_s);
		sum += buf;
		snprintf(buf, sizeof(buf), "fps: %.3f\n", measured_s > 0.0 ? (double)frame_idx / measured_s : 0.0);
		sum += buf;
		snprintf(buf, sizeof(buf), "steps_per_s: %.3f\n", measured_s > 0.0 ? (double)ticks_rec / measured_s : 0.0);
		sum += buf;
		uint64_t eng_frames = Engine::get_singleton()->get_physics_frames() - phys_frames_start;
		snprintf(buf, sizeof(buf), "ticks_counted: %llu (engine physics_frames delta over the window: %llu)\n", (unsigned long long)ticks_rec, (unsigned long long)eng_frames);
		sum += buf;
		snprintf(buf, sizeof(buf), "frames: %d (render loop unlocked, no frame cap)\n", frame_idx);
		sum += buf;
		snprintf(buf, sizeof(buf), "paced_steps: %llu (200 Hz x measured_s = %.1f; ratio %.4f)\n", (unsigned long long)ticks_rec, measured_s * 200.0, measured_s > 0.0 ? (double)ticks_rec / (measured_s * 200.0) : 0.0);
		sum += buf;
		snprintf(buf, sizeof(buf), "real_time_factor: %.4f (simulated s / wall s at 200 Hz; unpaced runs: see physics_delta_s and steps_per_s)\n", measured_s > 0.0 ? (double)ticks_rec * 0.005 / measured_s : 0.0);
		sum += buf;
		snprintf(buf, sizeof(buf), "dropped_steps_estimate: %.0f (200 Hz x measured_s minus steps run, only meaningful when paced; max_physics_steps_per_frame %d)\n", measured_s * 200.0 > (double)ticks_rec ? measured_s * 200.0 - (double)ticks_rec : 0.0, MAX_STEPS_PER_FRAME);
		sum += buf;
		snprintf(buf, sizeof(buf), "physics_delta_s: min=%.9f max=%.9f (1/200 = 0.005); simulated_s=%.3f\n", delta_min, delta_max, (double)ticks_rec * delta_max);
		sum += buf;
		sum += unpaced ? String("pacing: unpaced (max_fps 0, physics_ticks_per_second 200000, time_scale 1000, max_physics_steps_per_frame 1)\n") : String("pacing: render and physics modes, vsync off, max_fps 0 (unlocked), physics 200 Hz fixed, max_physics_steps_per_frame 64 (steps per frame vary with the frame rate)\n");
		std::vector<double> wall_tick;
		for (size_t i = 0; i < rows.size(); i++) {
			if (row_ticked[i]) {
				wall_tick.push_back(rows[i].wall_ms);
			}
		}
		sum += stats("wall_ms, tick frames only", wall_tick);
		// first 5 s vs last 5 s of the measured window (frames by cumulative wall time)
		std::vector<double> w_first, w_last, p_first, p_last;
		for (size_t i = 0; i < rows.size(); i++) {
			double end_ms = row_cum_ms[i];
			double start_ms = end_ms - rows[i].wall_ms;
			bool in_first = end_ms <= 5000.0;
			bool in_last = start_ms >= measured_ms - 5000.0;
			if (in_first) {
				w_first.push_back(rows[i].wall_ms);
				if (row_ticked[i]) {
					p_first.push_back(rows[i].step_ms);
				}
			}
			if (in_last) {
				w_last.push_back(rows[i].wall_ms);
				if (row_ticked[i]) {
					p_last.push_back(rows[i].step_ms);
				}
			}
		}
		sum += stats("wall_ms first_5s", w_first);
		sum += stats("wall_ms last_5s", w_last);
		sum += stats("physics_ms first_5s (tick frames)", p_first);
		sum += stats("physics_ms last_5s (tick frames)", p_last);
		snprintf(buf, sizeof(buf), "window_frames: first_5s=%d last_5s=%d (windows overlap when measure < 10 s)\n", (int)w_first.size(), (int)w_last.size());
		sum += buf;
		int awake_end = _count_awake();
		snprintf(buf, sizeof(buf), "awake_bodies_at_end: %d of %d dynamic bodies\n", awake_end, is_debris ? (int)live_pieces : (int)body_rids.size());
		sum += buf;
		if (pit_interior > 0.0) {
			PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
			double half = pit_interior / 2.0;
			int outside = 0;
			std::vector<RID> all_rids;
			_collect_rids(all_rids);
			for (const RID &rid : all_rids) {
				Vector3 o = ((Transform3D)ps->body_get_state(rid, PhysicsServer3D::BODY_STATE_TRANSFORM)).origin;
				if (std::fabs(o.x) > half || std::fabs(o.z) > half || o.y < -0.5) {
					outside++;
				}
			}
			snprintf(buf, sizeof(buf), "bodies_outside_pit_at_end: %d (pit interior %.0f m)\n", outside, pit_interior);
			sum += buf;
		}
		if (!impulse_bodies.empty()) {
			// joint/impulse check (PhysX port): end links must stay attached (chain length 5 m, ceiling at y 12) and have moved by the +x impulse
			double min_y = 1e9, max_d = 0.0, sum_dx = 0.0;
			for (const RID &rid : chain_rids) {
				min_y = std::min(min_y, (double)((Transform3D)PhysicsServer3D::get_singleton()->body_get_state(rid, PhysicsServer3D::BODY_STATE_TRANSFORM)).origin.y);
			}
			for (size_t i = 0; i < impulse_bodies.size() && i < impulse_pos0.size(); i++) {
				Vector3 p = impulse_bodies[i]->get_global_position();
				max_d = std::max(max_d, (double)(p - impulse_pos0[i]).length());
				sum_dx += p.x - impulse_pos0[i].x;
			}
			snprintf(buf, sizeof(buf), "chain_check: all_links_min_y=%.3f (start >= 7.5; free fall would go far lower) end_links_max_move=%.3f end_links_mean_dx=%.3f n_end_links=%d\n", min_y, max_d, sum_dx / (double)impulse_bodies.size(), (int)impulse_bodies.size());
			sum += buf;
		}
		if (is_debris) {
			sum += String("debris_variant: ") + (pooled ? "pooled (pre-created pieces parked asleep at y=-1000 inside the simulation, teleported and woken on reuse; no add_child/remove_child or memnew/memdelete)" : "create-and-free (memnew + add_child, remove_child + memdelete)") + "\n";
			snprintf(buf, sizeof(buf), "debris_rate_events_per_s: %g\n", rate);
			sum += buf;
			sum += stats("event_ms (event frames only)", event_ms_list);
			sum += stats("despawn_ms (removal frames only)", despawn_ms_list);
			sum += stats("wall_ms on event frames", wall_event);
			sum += stats("wall_ms on removal frames", wall_despawn);
			sum += stats("wall_ms on non-event frames", wall_nonevent);
			double lmax = 0.0, lsum = 0.0;
			int amin = 1 << 30, amax = 0;
			for (const Row &r : rows) {
				lmax = std::max(lmax, (double)r.live);
				lsum += r.live;
				amin = std::min(amin, r.awake);
				amax = std::max(amax, r.awake);
			}
			snprintf(buf, sizeof(buf), "debris_live: max=%.0f mean=%.1f\n", lmax, rows.empty() ? 0.0 : lsum / (double)rows.size());
			sum += buf;
			snprintf(buf, sizeof(buf), "awake_bodies: min=%d max=%d\n", rows.empty() ? 0 : amin, amax);
			sum += buf;
			snprintf(buf, sizeof(buf), "events_completed (measured window): %lld (total incl. warm-up %lld; on intact block %lld, on already-used block %lld); removals (measured window): %lld\n", (long long)events_rec, (long long)events_total, (long long)events_on_intact, (long long)events_on_used, (long long)removals_rec);
			sum += buf;
			snprintf(buf, sizeof(buf), "pool: initial %lld, misses %lld, free at end %d\n", (long long)pool_size0, (long long)pool_misses, (int)pool_free.size());
			sum += buf;
			snprintf(buf, sizeof(buf), "impulse_probe: |v| right after spawn %.4f, at next tick %.4f (a radial piece should read about 1.5 m/s +-20%%)\n", vel_after_spawn, vel_next_tick);
			sum += buf;
			sum += "debris_render_meshes: shared 0.5 BoxMesh resource, one MeshInstance3D per piece (blocks: shared unit BoxMesh scaled); physics mode has no meshes\n";
		}
		double ws, wsp;
		working_set_mb(ws, wsp);
		snprintf(buf, sizeof(buf), "working_set_mb: %.1f (peak %.1f)\n", ws, wsp);
		sum += buf;
		snprintf(buf, sizeof(buf), "bodies: %d scale_N=%d\n", is_debris ? (int)live_pieces : (int)body_rids.size(), bodies);
		sum += buf;
	}
	UtilityFunctions::print(sum);
	Ref<FileAccess> sf = FileAccess::open(base + ".csv.summary.txt", FileAccess::WRITE);
	if (sf.is_valid()) {
		sf->store_string(sum);
		sf.unref();
	}
	Ref<FileAccess> ef = FileAccess::open(base + ".env.txt", FileAccess::WRITE);
	if (ef.is_valid()) {
		OS *os = OS::get_singleton();
		ef->store_line("engine: Godot " + String(v.get("string", "?")) + " (hash " + String(v.get("hash", "?")) + ")");
		ef->store_line("backend: " + setting("physics/3d/physics_engine"));
		ef->store_line("physics_space_state_class (runtime, get_world_3d()->get_direct_space_state()->get_class()): " + ps_class);
		ef->store_line("engine_tag: " + eng_tag);
		ef->store_line("flavour: " + flavour);
		ef->store_line(String("sleeping: ") + (sleeping_off ? "off" : "default"));
		ef->store_line("camera_mode: " + camera_mode + (render ? "" : " (physics mode: no camera node, values computed from the same rule)"));
		ef->store_line(String("camera_position: ") + String("(%.3f, %.3f, %.3f)") % Array::make(cam_pos.x, cam_pos.y, cam_pos.z));
		ef->store_line(String("camera_look_at: ") + String("(%.3f, %.3f, %.3f)") % Array::make(cam_look_at.x, cam_look_at.y, cam_look_at.z));
		ef->store_line("camera_fov_v_deg: 60");
		ef->store_line("camera_near: 0.1");
		ef->store_line("camera_far: 2000");
		ef->store_line(String("camera_content_radius: ") + String("%.3f") % Array::make(cam_radius));
		ef->store_line(String("camera_content_aabb: ") + String("min (%.3f, %.3f, %.3f) max (%.3f, %.3f, %.3f)") % Array::make(bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z));
		// ---- parity block: the same key names are written by the Stride side (SPEC "Settings parity"); values are read back at run time ----
		ef->store_line("profile: " + profile);
		ef->store_line("parity.profile: " + profile);
		ef->store_line(String("parity.sleeping: ") + (sleeping_off ? "off" : "default"));
		ef->store_line(String("parity.camera_position: ") + String("(%.3f, %.3f, %.3f)") % Array::make(cam_pos.x, cam_pos.y, cam_pos.z));
		ef->store_line(String("parity.camera_look_at: ") + String("(%.3f, %.3f, %.3f)") % Array::make(cam_look_at.x, cam_look_at.y, cam_look_at.z));
		ef->store_line("parity.fov_v_deg: 60");
		ef->store_line("parity.near: 0.1");
		ef->store_line("parity.far: 2000");
		ef->store_line(String("parity.camera_content_radius: ") + String("%.3f") % Array::make(cam_radius));
		{
			Engine *eng = Engine::get_singleton();
			ef->store_line("parity.physics_hz: " + String::num_int64(unpaced ? PHYS_HZ : eng->get_physics_ticks_per_second()) + (unpaced ? " (simulated step 1/200 s; unpaced loop runs ticks_per_second x time_scale, see NOTES)" : ""));
			ef->store_line("physics_hz: " + String::num_int64(PHYS_HZ) + " (fixed simulated step 1/200 s)");
			ef->store_line("parity.max_physics_steps_per_frame: " + String::num_int64(eng->get_max_physics_steps_per_frame()));
			double g = (double)ProjectSettings::get_singleton()->get_setting("physics/3d/default_gravity", 9.8);
			Vector3 gv = ProjectSettings::get_singleton()->get_setting("physics/3d/default_gravity_vector", Vector3(0, -1, 0));
			Vector3 gg = gv.normalized() * g;
			ef->store_line(String("parity.gravity: ") + String("(%.3f, %.3f, %.3f)") % Array::make(gg.x, gg.y, gg.z));
			ef->store_line(String("parity.friction: %.3f") % Array::make(phys_mat->get_friction()));
			ef->store_line(String("parity.restitution: %.3f") % Array::make(phys_mat->get_bounce()));
			ef->store_line("parity.linear_damping: " + setting("physics/3d/default_linear_damp"));
			ef->store_line("parity.angular_damping: " + setting("physics/3d/default_angular_damp"));
			ef->store_line("parity.body_mass: 1 (box, sphere, capsule; debris pieces 0.125); inertia engine-computed from the shape and mass");
			RigidBody3D *probe = memnew(RigidBody3D);
			ef->store_line(String("parity.ccd: ") + (probe->is_using_continuous_collision_detection() ? "on" : "off") + " (RigidBody3D.continuous_cd default read back; never set by the benchmark)");
			memdelete(probe);
			ef->store_line(String("parity.shape_sizes: box 0.5 x 0.5 x 0.5, sphere r 0.25, capsule r 0.15 h 0.5 (box edge ") + String::num(box_shape->get_size().x, 3) + ", sphere r " + String::num(sphere_shape->get_radius(), 3) + ", capsule r " + String::num(capsule_shape->get_radius(), 3) + " h " + String::num(capsule_shape->get_height(), 3) + " read back)");
			if (eng_tag == "jolt") {
				ef->store_line("parity.solver: Jolt velocity_steps " + setting("physics/jolt_physics_3d/simulation/velocity_steps") + ", position_steps " + setting("physics/jolt_physics_3d/simulation/position_steps") + " (project settings read at run time; Bepu substeps are not the same thing, see SPEC)");
				ef->store_line("solver_iterations: " + setting("physics/jolt_physics_3d/simulation/velocity_steps") + " (Jolt velocity_steps; position_steps " + setting("physics/jolt_physics_3d/simulation/position_steps") + ")");
				ef->store_line("solver_substeps: n/a (Jolt has no substeps; Bepu solver_substeps is not comparable)");
			} else if (eng_tag == "godotphysics") {
				ef->store_line("parity.solver: GodotPhysics3D solver_iterations " + setting("physics/3d/solver/solver_iterations"));
				ef->store_line("solver_iterations: " + setting("physics/3d/solver/solver_iterations") + " (GodotPhysics3D)");
				ef->store_line("solver_substeps: n/a (GodotPhysics3D has no substeps)");
			} else {
				ef->store_line("parity.solver: PhysX solver_type " + setting("physics/physx_3d/simulation/solver_type"));
				ef->store_line("solver_iterations: n/a (PhysX solver_type " + setting("physics/physx_3d/simulation/solver_type") + ")");
				ef->store_line("solver_substeps: n/a (PhysX has no substeps)");
			}
			ef->store_line("parity.physics_threads: threading/worker_pool/max_threads = " + setting("threading/worker_pool/max_threads") + " (-1 = engine default), run_on_separate_thread = " + setting("physics/3d/run_on_separate_thread") + ", logical cores " + String::num_int64(os->get_processor_count()));
			DisplayServer *ds = DisplayServer::get_singleton();
			Vector2i ws = ds->window_get_size();
			ef->store_line(String("parity.window_size: ") + (render ? String::num_int64(ws.x) + " x " + String::num_int64(ws.y) : String("n/a (physics mode)")));
			ef->store_line(String("parity.window_mode: ") + (render ? String::num_int64((int)ds->window_get_mode()) + " (DisplayServer.WindowMode, 0 = windowed)" : String("n/a (physics mode)")));
			ef->store_line(String("parity.vsync: ") + (render ? (ds->window_get_vsync_mode() == DisplayServer::VSYNC_ENABLED ? String("on") : String("off")) + " (read back" + String(", max_fps ") + String::num_int64(eng->get_max_fps()) + ")" : String("n/a (physics mode)")));
			ef->store_line(String("vsync: ") + (render ? (ds->window_get_vsync_mode() == DisplayServer::VSYNC_ENABLED ? String("on") : String("off")) : String("off (physics mode)")));
		}
		ef->store_line(render ? render_env_lines.strip_edges() : String("parity.render: n/a (physics mode, no render settings applied)"));
		if (!render) {
			ef->store_line("shadows: n/a (physics mode)");
			ef->store_line("shadow_map_size: n/a (physics mode)");
			ef->store_line("msaa: n/a (physics mode)");
		}
		// every engine-registered physics / rendering-quality / threading setting with the value in effect (defaults included), for audits
		{
			Array props = ProjectSettings::get_singleton()->get_property_list();
			for (int i = 0; i < props.size(); i++) {
				Dictionary pd = props[i];
				String pn = pd["name"];
				if (pn.begins_with("physics/") || pn.begins_with("threading/") || pn.begins_with("rendering/anti_aliasing/") || pn.begins_with("rendering/lights_and_shadows/") || pn.begins_with("rendering/scaling_3d/") || pn.begins_with("display/window/")) {
					ef->store_line("setting " + pn + " = " + String(ProjectSettings::get_singleton()->get_setting(pn)));
				}
			}
		}
		ef->store_line("os: " + os->get_name() + " " + os->get_version());
		ef->store_line("cpu: " + os->get_processor_name());
		ef->store_line("gpu: " + RenderingServer::get_singleton()->get_video_adapter_name());
		ef->store_line(String("build: ") + (os->is_debug_build() ? "debug (INVALID for results)" : "release") + (os->has_feature("editor") ? " editor" : ""));
#ifdef DEBUG_ENABLED
		ef->store_line("bench_extension: C++ GDExtension class QuestBench, godot-cpp template_debug build (INVALID for results)");
#else
		ef->store_line("bench_extension: C++ GDExtension class QuestBench, godot-cpp template_release build");
#endif
#if defined(__clang__)
		ef->store_line(String("bench_extension_compiler: clang ") + String(__clang_version__) + (String(" (clang-cl, MSVC-compatible driver)")));
#elif defined(_MSC_VER)
		ef->store_line("bench_extension_compiler: MSVC cl " + String::num_int64(_MSC_FULL_VER) + " (_MSC_VER " + String::num_int64(_MSC_VER) + ")");
#else
		ef->store_line("bench_extension_compiler: unknown");
#endif
		ef->store_line("rendering_method: " + setting("rendering/renderer/rendering_method"));
		ef->store_line("physics_ticks_per_second: " + String::num_int64(Engine::get_singleton()->get_physics_ticks_per_second()));
		ef->store_line("jolt velocity_steps: " + setting("physics/jolt_physics_3d/simulation/velocity_steps"));
		ef->store_line("jolt position_steps: " + setting("physics/jolt_physics_3d/simulation/position_steps"));
		ef->store_line("jolt allow_sleep: " + setting("physics/jolt_physics_3d/simulation/allow_sleep"));
		ef->store_line("jolt sleep_velocity_threshold: " + setting("physics/jolt_physics_3d/simulation/sleep_velocity_threshold"));
		ef->store_line("jolt sleep_time_threshold: " + setting("physics/jolt_physics_3d/simulation/sleep_time_threshold"));
		ef->store_line("physx solver_type (0=PGS 1=TGS): " + setting("physics/physx_3d/simulation/solver_type"));
		ef->store_line("physx allow_sleep: " + setting("physics/physx_3d/simulation/allow_sleep"));
		ef->store_line("physx stabilization: " + setting("physics/physx_3d/simulation/stabilization"));
		ef->store_line("physx cpu_worker_threads (0=auto): " + setting("physics/physx_3d/simulation/cpu_worker_threads"));
		ef->store_line("physx gpu_dynamics: " + setting("physics/physx_3d/simulation/gpu_dynamics"));
		// The fork registers gpu_dynamics only in a GPU (CUDA) build, so the key existing proves a GPU build; absent = CPU-only module.
		ef->store_line(String("physx gpu_build (key physics/physx_3d/simulation/gpu_dynamics registered): ") + (ProjectSettings::get_singleton()->has_setting("physics/physx_3d/simulation/gpu_dynamics") ? "yes" : "no"));
		// Registered only in a GPU build of the fork (97bc4de, godot_physx_project_settings.cpp:43-45); in a CPU-only build the keys are absent.
		ef->store_line(String("physx jointed_position_iterations / jointed_velocity_iterations (registered only in GPU builds; in a CPU-only build the value is real only if override.cfg sets it, otherwise 1/1): ") + (ProjectSettings::get_singleton()->has_setting("physics/physx_3d/simulation/jointed_position_iterations") ? setting("physics/physx_3d/simulation/jointed_position_iterations") + " / " + setting("physics/physx_3d/simulation/jointed_velocity_iterations") : String("not registered (CPU-only build)")));
		ef->store_line("physx enhanced_determinism: " + setting("physics/physx_3d/simulation/enhanced_determinism"));
		ef->store_line("godot sleep_threshold_linear / angular / time_before_sleep: " + setting("physics/3d/sleep_threshold_linear") + " / " + setting("physics/3d/sleep_threshold_angular") + " / " + setting("physics/3d/time_before_sleep"));
		ef->store_line("godot run_on_separate_thread: " + setting("physics/3d/run_on_separate_thread"));
		ef->store_line("default_linear_damp: " + setting("physics/3d/default_linear_damp"));
		ef->store_line("default_angular_damp: " + setting("physics/3d/default_angular_damp"));
		ef->store_line("jolt max_bodies: " + setting("physics/jolt_physics_3d/limits/max_bodies"));
		ef->store_line("jolt max_body_pairs: " + setting("physics/jolt_physics_3d/limits/max_body_pairs"));
		ef->store_line("jolt max_contact_constraints: " + setting("physics/jolt_physics_3d/limits/max_contact_constraints"));
		ef->store_line("jolt temporary_memory_buffer_size: " + setting("physics/jolt_physics_3d/limits/temporary_memory_buffer_size"));
		ef->store_line(String("jolt_limit_profile_applied_by_extension: ") + g_jolt_profile);
		ef->store_line("max_fps: " + String::num_int64(Engine::get_singleton()->get_max_fps()));
		ef->store_line("max_physics_steps_per_frame: " + String::num_int64(Engine::get_singleton()->get_max_physics_steps_per_frame()));
		ef->store_line("time_scale: " + String::num(Engine::get_singleton()->get_time_scale(), 1));
		ef->store_line(String("unpaced: ") + (unpaced ? "1" : "0") + " scale_bodies: " + String::num_int64(bodies));
		ef->store_line(String("physics_ms_def: ") + (tick_mark_added ? "2 (includes the first tick's tick-start window: QuestTickMark XR interface stamp to _physics_process start)" : "1 (end of _physics_process to start of _process; tick-start window not included)"));
		ef->store_line(String("tick_mark: added=") + (tick_mark_added ? "yes" : "no") + ", missing=" + String::num_int64(lead_missing));
		ef->store_line("diag_busy_ms: " + String::num(diag_busy_ms, 1));
		PackedStringArray ca = os->get_cmdline_args();
		PackedStringArray ua = os->get_cmdline_user_args();
		ef->store_line("cmdline: " + String(" ").join(ca));
		ef->store_line("user_args: " + String(" ").join(ua));
		ef.unref();
	}
	get_tree()->quit();
}
