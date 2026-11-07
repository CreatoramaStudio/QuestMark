#include "register_types.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include <cstdio>

#include "quest_bench.h"

using namespace godot;

const char *g_jolt_profile = "not applied";

namespace {
// Jolt reads its limits (max_bodies, max_body_pairs, max_contact_constraints) from the project settings when the physics space is
// created. project.godot holds the OLD limits (10240 / 65536 / 20480). This runs at SERVERS initialization (after the engine read the
// project, before the physics server is created; SCENE level was tested and is too late), looks at the command line and overrides the settings:
//   old     10240 /   65536 /   20480   scenarios A to E (default)
//   raised  70000 / 1048576 / 1048576   scenarios F (scale) and G (debris, default; with the old pair and constraint caps R = 8 printed "contact constraint buffer exceeded capacity and contacts were ignored", and R = 32 keeps about 16400 pieces alive, above the old 10240 body cap)
//   bodies  70000 /   65536 /   20480   only the body cap raised (explicit, for A/B)
// --jolt-limits=<profile> chooses explicitly; --jolt-max-bodies / --jolt-max-pairs / --jolt-max-constraints=<n> override single values.
void apply_jolt_limits() {
	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	String scenario, profile;
	int64_t mb = -1, mp = -1, mc = -1;
	for (int i = 0; i < args.size(); i++) {
		String a = args[i];
		if (!a.begins_with("--") || !a.contains("=")) {
			continue;
		}
		PackedStringArray kv = a.substr(2).split("=", true, 1);
		if (kv[0] == "scenario") {
			scenario = kv[1];
		} else if (kv[0] == "jolt-limits") {
			profile = kv[1];
		} else if (kv[0] == "jolt-max-bodies") {
			mb = kv[1].to_int();
		} else if (kv[0] == "jolt-max-pairs") {
			mp = kv[1].to_int();
		} else if (kv[0] == "jolt-max-constraints") {
			mc = kv[1].to_int();
		}
	}
	if (profile.is_empty()) {
		if (scenario == "scale") {
			profile = "raised";
		} else if (scenario == "debris" || scenario == "debris-pooled") {
			profile = "raised";
		} else {
			profile = "old";
		}
	}
	int64_t b = 10240, p = 65536, c = 20480;
	if (profile == "raised") {
		b = 70000;
		p = 1048576;
		c = 1048576;
	} else if (profile == "bodies") {
		b = 70000;
	}
	if (mb >= 0) {
		b = mb;
	}
	if (mp >= 0) {
		p = mp;
	}
	if (mc >= 0) {
		c = mc;
	}
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ps->set_setting("physics/jolt_physics_3d/limits/max_bodies", b);
	ps->set_setting("physics/jolt_physics_3d/limits/max_body_pairs", p);
	ps->set_setting("physics/jolt_physics_3d/limits/max_contact_constraints", c);
	static char buf[160];
	snprintf(buf, sizeof(buf), "%s (max_bodies %lld, max_body_pairs %lld, max_contact_constraints %lld)", profile.utf8().get_data(), (long long)b, (long long)p, (long long)c);
	g_jolt_profile = buf;
}
} // namespace

void initialize_questbench(ModuleInitializationLevel level) {
	if (level == MODULE_INITIALIZATION_LEVEL_CORE) {
		// before the physics servers are created: Jolt reads its limits then (tested: SCENE level is too late)
		apply_jolt_limits();
		return;
	}
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(QuestBench);
}

void uninitialize_questbench(ModuleInitializationLevel level) {
	(void)level;
}

extern "C" {
GDExtensionBool GDE_EXPORT questbench_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_questbench);
	init_obj.register_terminator(uninitialize_questbench);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_CORE);
	return init_obj.init();
}
}
