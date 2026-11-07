extends Node3D
# Quest engine benchmark, Godot side. Spec: benchmark/SPEC.md. See NOTES.md for what was run and verified.
# Usage (args after "--"):  --scenario=pit-2000|raycast-1000|raycast-10000|chains|mixed
#                           --mode=physics|render --run=1 --out=<dir> [--warmup=5 --measure=20]
#                           [--diag-busy-ms=<ms>]  experiment only: busy-wait inside _physics_process

const SEED := 12345
const ENGINE_TAG := "godot"
const BACKEND_TAG := "jolt"
const RAY_LEN := 30.0
const SPACING := 0.6

var scenario := "pit-2000"
var mode := "physics"
var run_id := "1"
var out_dir := "results"
var warmup_s := 5.0
var measure_s := 20.0
var diag_busy_ms := 0.0

var rng := RandomNumberGenerator.new()
var phys_mat := PhysicsMaterial.new()
var box_shape := BoxShape3D.new()
var sphere_shape := SphereShape3D.new()
var capsule_shape := CapsuleShape3D.new()
var box_mesh: Mesh
var sphere_mesh: Mesh
var capsule_mesh: Mesh
var render := false

var ray_count := 0
var ray_from := PackedVector3Array()
var ray_to := PackedVector3Array()
var ray_params: PhysicsRayQueryParameters3D
var total_hits := 0
var query_ms_last := 0.0

var impulse_bodies: Array[RigidBody3D] = []
var impulse_done := false

# physics step stopwatch: end of our last _physics_process in a frame to the start of _process
var phys_end_us := 0
var tick_in_frame := 0
var step_acc_us := 0
var body_rids: Array[RID] = []
var awake_last := 0
var mon_phys := PackedFloat64Array()
var awake_cost := PackedFloat64Array()
var script_ms_last := 0.0
var script_cost := PackedFloat64Array()
var phys_ticks := PackedFloat64Array()

# recording
var last_us := 0
var phase_start_us := 0
var recording := false
var frame_idx := 0
var wall: PackedFloat64Array = PackedFloat64Array()
var phys: PackedFloat64Array = PackedFloat64Array()
var rows := PackedStringArray()

func _ready() -> void:
	_parse_args()
	render = (mode == "render")
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	if render:
		Engine.max_fps = 0
	else:
		# Physics mode: no meshes, minimized window, loop capped at the 60 Hz tick rate.
		Engine.max_fps = 60
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MINIMIZED)
	rng.seed = SEED
	phys_mat.friction = 0.5
	phys_mat.bounce = 0.1
	box_shape.size = Vector3(0.5, 0.5, 0.5)
	sphere_shape.radius = 0.25
	capsule_shape.radius = 0.15
	capsule_shape.height = 0.5  # Godot: total height including caps
	if render:
		var bm := BoxMesh.new()
		bm.size = Vector3(0.5, 0.5, 0.5)
		box_mesh = bm
		var sm := SphereMesh.new()
		sm.radius = 0.25
		sm.height = 0.5
		sphere_mesh = sm
		var cm := CapsuleMesh.new()
		cm.radius = 0.15
		cm.height = 0.5
		capsule_mesh = cm
		_add_camera_and_light()
	match scenario:
		"pit-2000":
			_build_pit(22.0)
			_build_pit_bodies(false)
		"raycast-1000":
			ray_count = 1000
			_build_raycast_statics()
			_build_rays()
		"raycast-10000":
			ray_count = 10000
			_build_raycast_statics()
			_build_rays()
		"chains":
			_build_chains()
		"mixed":
			ray_count = 1000
			_build_pit(32.0)
			_build_chains()
			_build_pit_bodies(true)
			_build_rays()
		_:
			push_error("unknown scenario: " + scenario)
			get_tree().quit(2)
			return
	last_us = Time.get_ticks_usec()
	phase_start_us = last_us

# ---------- args ----------
func _parse_args() -> void:
	for a in OS.get_cmdline_user_args():
		if not a.begins_with("--") or not a.contains("="):
			continue
		var kv := a.substr(2).split("=", true, 1)
		match kv[0]:
			"scenario": scenario = kv[1]
			"mode": mode = kv[1]
			"run": run_id = kv[1]
			"out": out_dir = kv[1]
			"warmup": warmup_s = float(kv[1])
			"measure": measure_s = float(kv[1])
			"diag-busy-ms": diag_busy_ms = float(kv[1])

# ---------- builders ----------
func _add_camera_and_light() -> void:
	var light := DirectionalLight3D.new()
	add_child(light)
	light.look_at_from_position(Vector3(10, 20, 10), Vector3.ZERO)
	var cam := Camera3D.new()
	add_child(cam)
	var big := (scenario == "mixed")
	cam.look_at_from_position(Vector3(0, 14 if big else 10, 40 if big else 26), Vector3(0, 4, 0))

func _static_box(size: Vector3, pos: Vector3) -> StaticBody3D:
	var b := StaticBody3D.new()
	b.physics_material_override = phys_mat
	var cs := CollisionShape3D.new()
	var s := BoxShape3D.new()
	s.size = size
	cs.shape = s
	b.add_child(cs)
	b.position = pos
	add_child(b)
	return b

func _build_pit(interior: float) -> void:
	_static_box(Vector3(interior + 2.0, 1.0, interior + 2.0), Vector3(0, -0.5, 0))
	var h := interior / 2.0 + 0.5
	_static_box(Vector3(1, 6, interior + 2.0), Vector3(h, 3, 0))
	_static_box(Vector3(1, 6, interior + 2.0), Vector3(-h, 3, 0))
	_static_box(Vector3(interior, 6, 1), Vector3(0, 3, h))
	_static_box(Vector3(interior, 6, 1), Vector3(0, 3, -h))

func _make_dynamic(kind: int, pos: Vector3) -> RigidBody3D:
	var b := RigidBody3D.new()
	b.mass = 1.0
	b.physics_material_override = phys_mat
	var cs := CollisionShape3D.new()
	var mesh: Mesh = null
	match kind:
		0:
			cs.shape = box_shape
			mesh = box_mesh
		1:
			cs.shape = sphere_shape
			mesh = sphere_mesh
		_:
			cs.shape = capsule_shape
			mesh = capsule_mesh
	b.add_child(cs)
	if render:
		var mi := MeshInstance3D.new()
		mi.mesh = mesh
		b.add_child(mi)
	b.position = pos
	body_rids.append(b.get_rid())
	return b

func _jit() -> float:
	return rng.randf_range(-0.05, 0.05)

func _build_pit_bodies(ring: bool) -> void:
	var n := 0
	if not ring:
		# 12 x 12 columns (6.6 m span, inside the 22 m pit), layers from y = 1 until 2000 bodies; same rule as Stride BuildPit(22, 12, 0).
		var layer0 := 0
		while n < 2000:
			for i in range(12):
				for j in range(12):
					if n >= 2000:
						break
					var p := Vector3((i - 5.5) * SPACING + _jit(), 1.0 + layer0 * SPACING + _jit(), (j - 5.5) * SPACING + _jit())
					add_child(_make_dynamic(n % 2, p))
					n += 1
			layer0 += 1
		return
	# mixed: columns on a +-9 m grid, skipping |x|<6 and |z|<6 (chain area), layers until 2000 bodies.
	var cols: Array[Vector2] = []
	var steps := int(9.0 / SPACING)
	for i in range(-steps, steps + 1):
		for j in range(-steps, steps + 1):
			var x := i * SPACING
			var z := j * SPACING
			if absf(x) < 6.0 and absf(z) < 6.0:
				continue
			cols.append(Vector2(x, z))
	var layer := 0
	while n < 2000:
		for c in cols:
			if n >= 2000:
				break
			var p := Vector3(c.x + _jit(), 1.0 + layer * SPACING + _jit(), c.y + _jit())
			add_child(_make_dynamic(n % 2, p))
			n += 1
		layer += 1

func _build_raycast_statics() -> void:
	for i in range(2000):
		var pos := Vector3(rng.randf_range(-20, 20), rng.randf_range(0, 20), rng.randf_range(-20, 20))
		var axis := Vector3(rng.randf_range(-1, 1), rng.randf_range(-1, 1), rng.randf_range(-1, 1))
		if axis.length_squared() < 0.0001:
			axis = Vector3.UP
		var bas := Basis(axis.normalized(), rng.randf_range(0.0, TAU))
		var b := StaticBody3D.new()
		b.physics_material_override = phys_mat
		var cs := CollisionShape3D.new()
		match i % 3:
			0:
				cs.shape = box_shape
			1:
				cs.shape = capsule_shape
			_:
				cs.shape = sphere_shape
		b.add_child(cs)
		b.transform = Transform3D(bas, pos)
		add_child(b)

func _build_rays() -> void:
	ray_from.resize(ray_count)
	ray_to.resize(ray_count)
	for i in range(ray_count):
		var o := Vector3(rng.randf_range(-20, 20), rng.randf_range(0, 20), rng.randf_range(-20, 20))
		var d := Vector3(rng.randf_range(-1, 1), rng.randf_range(-1, 1), rng.randf_range(-1, 1))
		if d.length_squared() < 0.0001:
			d = Vector3.DOWN
		ray_from[i] = o
		ray_to[i] = o + d.normalized() * RAY_LEN
	ray_params = PhysicsRayQueryParameters3D.create(Vector3.ZERO, Vector3.ONE)

func _build_chains() -> void:
	# Static ceiling bar: slab 11 x 0.2 x 11, bottom face at y = 12.
	var ceiling := _static_box(Vector3(11, 0.2, 11), Vector3(0, 12.1, 0))
	for cx in range(10):
		for cz in range(10):
			var x := (cx - 4.5) * 1.0
			var z := (cz - 4.5) * 1.0
			var prev: PhysicsBody3D = ceiling
			for l in range(10):
				var link := _make_dynamic(2, Vector3(x, 12.0 - 0.25 - 0.5 * l, z))
				add_child(link)
				var j := PinJoint3D.new()
				j.position = Vector3(x, 12.0 - 0.5 * l, z)
				add_child(j)
				j.node_a = prev.get_path()
				j.node_b = link.get_path()
				prev = link
				if l == 9:
					impulse_bodies.append(link)

# ---------- loop ----------
func _physics_process(_delta: float) -> void:
	var t_start := Time.get_ticks_usec()
	if tick_in_frame > 0:
		# second tick in the same frame: the gap since the previous tick's script end is that tick's step
		step_acc_us += t_start - phys_end_us
	tick_in_frame += 1
	if not impulse_done and not impulse_bodies.is_empty():
		impulse_done = true
		for b in impulse_bodies:
			b.apply_central_impulse(Vector3(2, 0, 0))
	if ray_count > 0:
		var space := get_world_3d().direct_space_state
		var hits := 0
		var t0 := Time.get_ticks_usec()
		for i in range(ray_count):
			ray_params.from = ray_from[i]
			ray_params.to = ray_to[i]
			if not space.intersect_ray(ray_params).is_empty():
				hits += 1
		query_ms_last = (Time.get_ticks_usec() - t0) / 1000.0
		if recording:
			total_hits += hits
	if diag_busy_ms > 0.0:
		var until := Time.get_ticks_usec() + int(diag_busy_ms * 1000.0)
		while Time.get_ticks_usec() < until:
			pass
	phys_end_us = Time.get_ticks_usec()
	script_ms_last = (phys_end_us - t_start) / 1000.0

func _count_awake() -> int:
	var n := 0
	for rid in body_rids:
		if not PhysicsServer3D.body_get_state(rid, PhysicsServer3D.BODY_STATE_SLEEPING):
			n += 1
	return n

func _process(_delta: float) -> void:
	var now := Time.get_ticks_usec()
	var wall_ms := (now - last_us) / 1000.0
	last_us = now
	var ticked := tick_in_frame > 0
	var step_ms := 0.0
	if ticked:
		step_ms = (step_acc_us + (now - phys_end_us)) / 1000.0
	tick_in_frame = 0
	step_acc_us = 0
	if not recording:
		if (now - phase_start_us) / 1.0e6 >= warmup_s:
			recording = true
			phase_start_us = now
		return
	if ticked:
		var t1 := Time.get_ticks_usec()
		awake_last = _count_awake()
		awake_cost.append((Time.get_ticks_usec() - t1) / 1000.0)
	var mon: float = Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS) * 1000.0
	wall.append(wall_ms)
	phys.append(step_ms)
	mon_phys.append(mon)
	if ticked:
		script_cost.append(script_ms_last)
		phys_ticks.append(step_ms)
	rows.append("%d,%.4f,%.4f,%.4f,0,0,0,0,%d" % [frame_idx, wall_ms, step_ms, query_ms_last, awake_last])
	frame_idx += 1
	if (now - phase_start_us) / 1.0e6 >= measure_s:
		_finish()

# ---------- output ----------
func _pct(sorted: PackedFloat64Array, p: float) -> float:
	if sorted.is_empty():
		return 0.0
	var idx := clampi(int(ceil(p * sorted.size())) - 1, 0, sorted.size() - 1)
	return sorted[idx]

func _stats(label: String, a: PackedFloat64Array) -> String:
	var s := a.duplicate()
	s.sort()
	return "%s: median=%.4f p95=%.4f p99=%.4f max=%.4f\n" % [label, _pct(s, 0.5), _pct(s, 0.95), _pct(s, 0.99), _pct(s, 1.0)]

func _setting(setting_name: String) -> String:
	return str(ProjectSettings.get_setting(setting_name, "<unset: engine default>"))

func _finish() -> void:
	set_process(false)
	set_physics_process(false)
	var abs_dir := out_dir
	if out_dir.begins_with("user://") or out_dir.begins_with("res://"):
		abs_dir = ProjectSettings.globalize_path(out_dir)
	DirAccess.make_dir_recursive_absolute(abs_dir)
	var base := abs_dir.path_join("%s-%s-%s-%s-%s" % [ENGINE_TAG, BACKEND_TAG, scenario, mode, run_id])
	var f := FileAccess.open(base + ".csv", FileAccess.WRITE)
	if f == null:
		push_error("cannot open csv: %s (err %d)" % [base + ".csv", FileAccess.get_open_error()])
		get_tree().quit(3)
		return
	f.store_line("frame,wall_ms,physics_ms,query_ms,gc0,gc1,gc2,gc_pause_ms,awake_bodies")
	for r in rows:
		f.store_line(r)
	f.close()
	var m120 := 0.0
	for v in wall.slice(0, 120):
		m120 = maxf(m120, v)
	var sum := "engine=Godot %s backend=Jolt scenario=%s mode=%s run=%s frames=%d\n" % [Engine.get_version_info().get("string", "?"), scenario, mode, run_id, frame_idx]
	sum += _stats("wall_ms", wall)
	sum += _stats("physics_ms (own stopwatch: end of _physics_process to start of _process; 0 on frames without a tick)", phys)
	sum += _stats("engine_monitor_TIME_PHYSICS_PROCESS_ms (engine updates it ~1 Hz; not used in the CSV)", mon_phys)
	sum += _stats("physics_ms, tick frames only", phys_ticks)
	sum += _stats("awake_count_cost_ms (script cost of counting awake bodies, tick frames only)", awake_cost)
	sum += _stats("script_physics_process_ms (our _physics_process incl. ray batch and diag busy-wait, tick frames only)", script_cost)
	sum += "total_gc_pause_ms: 0 (GDScript)\n"
	sum += "first_120_frames_max_wall_ms: %.4f\n" % m120
	sum += "ray_hits_total (measured window): %d\n" % total_hits
	print(sum)
	var sf := FileAccess.open(base + ".csv.summary.txt", FileAccess.WRITE)
	if sf:
		sf.store_string(sum)
		sf.close()
	var ef := FileAccess.open(base + ".env.txt", FileAccess.WRITE)
	if ef:
		var v := Engine.get_version_info()
		ef.store_line("engine: Godot " + str(v.get("string", "?")) + " (hash " + str(v.get("hash", "?")) + ")")
		ef.store_line("backend: " + _setting("physics/3d/physics_engine"))
		ef.store_line("os: %s %s" % [OS.get_name(), OS.get_version()])
		ef.store_line("cpu: " + OS.get_processor_name())
		ef.store_line("gpu: " + RenderingServer.get_video_adapter_name())
		ef.store_line("build: " + ("debug (INVALID for results)" if OS.is_debug_build() else "release") + (" editor" if OS.has_feature("editor") else ""))
		ef.store_line("rendering_method: " + _setting("rendering/renderer/rendering_method"))
		ef.store_line("physics_ticks_per_second: " + str(Engine.physics_ticks_per_second))
		ef.store_line("jolt velocity_steps: " + _setting("physics/jolt_physics_3d/simulation/velocity_steps"))
		ef.store_line("jolt position_steps: " + _setting("physics/jolt_physics_3d/simulation/position_steps"))
		ef.store_line("jolt allow_sleep: " + _setting("physics/jolt_physics_3d/simulation/allow_sleep"))
		ef.store_line("jolt sleep_velocity_threshold: " + _setting("physics/jolt_physics_3d/simulation/sleep_velocity_threshold"))
		ef.store_line("jolt sleep_time_threshold: " + _setting("physics/jolt_physics_3d/simulation/sleep_time_threshold"))
		ef.store_line("default_linear_damp: " + _setting("physics/3d/default_linear_damp"))
		ef.store_line("default_angular_damp: " + _setting("physics/3d/default_angular_damp"))
		ef.store_line("diag_busy_ms: " + str(diag_busy_ms))
		ef.store_line("cmdline: " + " ".join(OS.get_cmdline_args()))
		ef.store_line("user_args: " + " ".join(OS.get_cmdline_user_args()))
		ef.close()
	get_tree().quit()
