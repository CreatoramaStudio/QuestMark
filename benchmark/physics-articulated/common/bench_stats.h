#pragma once
// Small self-contained timing helper for the physics-articulated tests.
//  - fixed time step: the workload always receives the same dt = 1 / step_hz;
//  - a warm-up phase is run first and excluded from the statistics;
//  - statistics per run: mean, median (p50), p99, p99.9 and max of the per-step time, in ms;
//  - the workload must say which backend actually ran; a run without it is rejected.
// Steps run back to back without sleeping: the numbers are the cost of one step, not frame pacing.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace artic::bench {

struct Config {
  std::string name;
  std::string device_class = "unlabelled";  // e.g. desktop
  double step_hz = 120.0;                   // fixed step rate
  int warmup_steps = 120;                   // excluded from the statistics
  int measured_steps = 1200;
  int runs = 3;
};

struct Workload {
  std::function<void()> setup;            // optional, runs before every run
  std::function<void(double dt)> step;    // required, one simulation step
  std::function<std::string()> backend;   // required, what really ran (library, version, settings)
};

struct Stats {
  double mean_ms = 0, p50_ms = 0, p99_ms = 0, p999_ms = 0, max_ms = 0;
};

struct RunResult {
  std::string backend;
  Stats stats;
};

// Nearest-rank percentile (p in [0,1]) of an unsorted sample; 0 for an empty sample.
inline double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  auto rank = static_cast<size_t>(std::ceil(p * static_cast<double>(v.size())));
  if (rank < 1) rank = 1;
  return v[rank - 1];
}

inline Stats compute_stats(const std::vector<double>& step_ms) {
  Stats s;
  if (step_ms.empty()) return s;
  double sum = 0;
  for (double x : step_ms) sum += x;
  s.mean_ms = sum / static_cast<double>(step_ms.size());
  s.p50_ms = percentile(step_ms, 0.50);
  s.p99_ms = percentile(step_ms, 0.99);
  s.p999_ms = percentile(step_ms, 0.999);
  s.max_ms = *std::max_element(step_ms.begin(), step_ms.end());
  return s;
}

// Runs the workload cfg.runs times; returns false (and prints why) if the workload is incomplete or reports no backend.
inline bool run(const Config& cfg, const Workload& w, std::vector<RunResult>& out) {
  using clock = std::chrono::steady_clock;
  if (!w.step || !w.backend) {
    std::fprintf(stderr, "bench '%s': workload needs step and backend\n", cfg.name.c_str());
    return false;
  }
  const double dt = 1.0 / cfg.step_hz;
  for (int r = 0; r < cfg.runs; ++r) {
    if (w.setup) w.setup();
    std::vector<double> step_ms;
    step_ms.reserve(static_cast<size_t>(cfg.measured_steps));
    for (int i = 0; i < cfg.warmup_steps + cfg.measured_steps; ++i) {
      const auto t0 = clock::now();
      w.step(dt);
      const auto t1 = clock::now();
      if (i >= cfg.warmup_steps) step_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    RunResult rr;
    rr.backend = w.backend();
    if (rr.backend.empty()) {
      std::fprintf(stderr, "bench '%s': run %d reported no backend, result rejected\n", cfg.name.c_str(), r + 1);
      return false;
    }
    rr.stats = compute_stats(step_ms);
    out.push_back(rr);
  }
  return true;
}

inline std::string json_escape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) o += c;
  }
  return o;
}

}  // namespace artic::bench
