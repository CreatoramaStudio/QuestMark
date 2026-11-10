#!/usr/bin/perl
# Writes the README benchmark charts as standalone SVG files (docs/charts/*.svg) and prints the README chart sections (markdown image links).
# Bars run best to worst (bars at 100% are green); each label shows the value and the percentage of the best (best = 100%). Own background, so light and dark GitHub themes both read well.
# The values are copied by hand from the README numbers table: update them there and here together.
# The ezEngine ms-per-step values (first value of the pile, mixed, 1000, 4000 and pile-game charts) are window-corrected, (measured_s + overhead_s) / steps, copied from the README numbers table
# (re-measured 2026-11-09 in .tmp/comp-chart-1, 3 runs, median; before that recomputed from the 2026-11-08 r2 summaries).
# Usage: perl benchmark/tools/gen-charts.pl > .tmp/charts.md   (then paste over the chart sections of README.md between the markers)
use strict; use warnings; use FindBin;
my $dir = "$FindBin::Bin/../../docs/charts"; mkdir $dir unless -d $dir;
my @n = ("ezEngine direct Jolt","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt","PhysX RID","Jolt RID");
my %comp;   # components value per chart file, filled below (undef = no bar)
sub z { my @v = @_; map { ($n[$_], $v[$_]) } 0..$#v }
sub esc { my $s = shift; $s =~ s/&/&amp;/g; $s =~ s/</&lt;/g; $s =~ s/>/&gt;/g; $s }
# chart(file, title, unit, lower_better, [name, value]...) writes the svg
sub chart {
  my ($file,$title,$unit,$low,@p) = @_; my @d; push @d,[splice(@p,0,2)] while @p;
  push @d,["ezEngine",$comp{$file}] if defined $comp{$file};
  @d = sort { $low ? $a->[1] <=> $b->[1] : $b->[1] <=> $a->[1] } @d;
  my ($best,$max) = ($d[0][1], (sort { $b <=> $a } map { $_->[1] } @d)[0]);
  my ($W,$pad,$rowh,$barh,$lab,$txt) = (820,32,40,24,190,150);
  my $x0 = $pad + $lab + 14; my $L = $W - $pad - $txt - $x0;
  my $top = $pad + 46; my $H = $top + @d * $rowh + $pad - 4;
  my $s = qq(<svg xmlns="http://www.w3.org/2000/svg" width="$W" height="$H" viewBox="0 0 $W $H" font-family="Segoe UI, Helvetica, Arial, sans-serif">\n);
  # no background: the page shows through; text and axis colours follow the light/dark scheme of the page
  $s .= qq(<style>.t{fill:#1f2328}.m{fill:#57606a}.ax{stroke:#8c959f}.bar{fill:#2f81f7}.best{fill:#1a7f37}\@media (prefers-color-scheme:dark){.t{fill:#e6edf3}.m{fill:#9198a1}.ax{stroke:#6e7681}.best{fill:#3fb950}}</style>\n);
  $s .= qq(<text x="$pad" y="@{[$pad+12]}" font-size="17" font-weight="600" class="t">@{[esc($title)]}</text>\n);
  $s .= qq(<line x1="$x0" y1="@{[$top-6]}" x2="$x0" y2="@{[$top+@d*$rowh-4]}" class="ax" stroke-width="1.5"/>\n);
  my $i = 0;
  for my $d (@d) {
    my ($name,$v) = @$d; my $y = $top + $i*$rowh; $i++;
    my $pct = $low ? $best/$v*100 : $v/$best*100; my $len = $v/$max*$L;
    my $cls = int($pct+0.5) >= 100 ? "best" : "bar"; my $vs = sprintf("%.5g",$v);
    $s .= qq(<text x="@{[$x0-10]}" y="@{[$y+$barh/2+5]}" font-size="14" text-anchor="end" class="t">@{[esc($name)]}</text>\n);
    $s .= qq(<rect x="$x0" y="$y" width="@{[sprintf('%.1f',$len)]}" height="$barh" rx="3" class="$cls"/>\n);
    $s .= qq(<text x="@{[sprintf('%.1f',$x0+$len+8)]}" y="@{[$y+$barh/2+5]}" font-size="14" class="t">$vs$unit<tspan class="m"> · @{[int($pct+0.5)]}%</tspan></text>\n);
  }
  $s .= "</svg>\n";
  open my $f, ">:raw", "$dir/$file" or die; print $f $s; close $f;
}
my @c = ("ezEngine","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt");
sub zc { my @v = @_; map { ($c[$_], $v[$_]) } 0..$#v }
# "ezEngine" series = the default way to use the engine (its Jolt physics components, --physics-path=components); the positional first value of z() is the
# alternative "ezEngine direct Jolt" (bodies through the Jolt BodyInterface, --physics-path=raw, the exe default before 2026-11-09 and all ezEngine runs before then): one optional value per chart file, added as an extra bar
# by chart(); undef = no bar. Existing values above stay positional and untouched. Fill from a NEW run folder (plan: .tmp/comp-chart-run.sh, OUT_DIR
# .tmp/comp-chart-1, 3 runs per cell, median); do NOT use .tmp/comp-all5 (different accounting). Cell per chart (summary line in brackets):
#   pile-ms.svg              <- unlock-physics-pit-2000, stress   [1000 / steps_per_s_window] (E, unpaced)
#   pile-game-ms.svg         <- unlock-physics-pit-2000, game     [1000 / steps_per_s_window] (E, unpaced)
#   mixed-ms.svg             <- unlock-physics-mixed, stress      [1000 / steps_per_s_window] (E, unpaced)
#   bodies1000-ms.svg        <- scale-unlock-1000 (scale --bodies=1000 --unpaced=1), stress [1000 / steps_per_s_window]  (F unpaced pass)
#   bodies4000-ms.svg        <- scale-unlock-4000, stress         [1000 / steps_per_s_window] (F unpaced pass)
#   mixed-fps.svg            <- unlock-render-mixed, stress       [fps]                       (E)
#   mixed-fps-game.svg       <- unlock-render-mixed, game         [fps]                       (E)
#   mixed-p99-stress.svg     <- mixed, render mode, stress        [p99 wall_ms]               (D, paced)
#   mixed-p99-game.svg       <- mixed, render mode, game          [p99 wall_ms]               (D, paced)
#   raycast-10000-ms.svg     <- raycast-10000, render mode, game  [median wall_ms per frame]  (B)
#   capacity-render-on.svg / capacity-render-on-game.svg    <- F ladder scale, render mode, stress / game  (last N passing 20 ms p99)
#   capacity-render-off.svg / capacity-render-off-game.svg  <- F ladder scale, physics mode, stress / game
#   debris-render.svg / debris-physics.svg                  <- G ladder debris (game only), render / physics mode (highest rate passing 20 ms p99)
# Not charted from components: the renderer-ab-* charts (other series).
%comp = map { $_ => undef } qw(pile-ms.svg pile-game-ms.svg mixed-ms.svg bodies1000-ms.svg bodies4000-ms.svg mixed-fps.svg mixed-fps-game.svg
  mixed-p99-stress.svg mixed-p99-game.svg raycast-10000-ms.svg capacity-render-on.svg capacity-render-on-game.svg capacity-render-off.svg
  capacity-render-off-game.svg debris-render.svg debris-physics.svg);
# Filled 2026-11-09 from .tmp/comp-chart-1 (3 runs, median; ms values = 1000 / steps_per_s_window, the same like-for-like rule as the raw ezEngine bars).
@comp{qw(pile-ms.svg pile-game-ms.svg mixed-ms.svg bodies1000-ms.svg bodies4000-ms.svg)} = (3.10, 3.20, 3.22, 1.366, 6.45);
@comp{qw(mixed-fps.svg mixed-fps-game.svg mixed-p99-stress.svg mixed-p99-game.svg raycast-10000-ms.svg)} = (222.4, 216.2, 8.6, 8.9, 353);
# Ladders filled 2026-11-10 from .tmp/comp-ladders-1 (run-all.sh ezengine --stages=FG, components, RUNS=3).
# Capacity and debris values of ALL series = "last pass 20 ms" of the merge-results.pl ladder table (highest tested stage whose median passes, midpoints
# included), not the run-all LADDER log line, which drops a passing limit2 midpoint (engine-choice.md section 13 item 29). Sets: nodes F = results-200hz
# (PhysX, Stride) and results-200hz-r2 (Godot Jolt, ezEngine direct Jolt); nodes G = results-200hz (PhysX, Stride) and r2 (Godot Jolt, ezEngine direct Jolt);
# RID = results-rid-cap, results-rid-g, results-rid-g-physx.
@comp{qw(capacity-render-on.svg capacity-render-on-game.svg capacity-render-off.svg capacity-render-off-game.svg debris-render.svg debris-physics.svg)} = (2500, 2500, 3000, 2000, 3.5, 3.5);
chart("pile-ms.svg","Pile of 2000 awake bodies: ms per step"," ms",1,z(2.82,1.02,1.04,2.24,3.99,2.18,3.14));
chart("mixed-ms.svg","Mixed scene (3000 bodies, 100 chains, 1000 rays per step): ms per step"," ms",1,z(2.47,1.25,1.33,3.85,5.03,3.30,3.50));
chart("bodies4000-ms.svg","4000 awake bodies: ms per step"," ms",1,z(5.49,1.69,1.71,5.30,9.37,4.73,6.26));
chart("mixed-fps.svg","Mixed scene, render loop unlocked: frames per second"," fps",0,z(299.1,53.5,50.6,67.6,2.8,262,217));
chart("capacity-render-on.svg","Most bodies with p99 frame at or below 20 ms, rendering on"," bodies",0,z(3000,2000,2000,2500,1500,3000,2500));
chart("capacity-render-off.svg","Most bodies with p99 frame at or below 20 ms, rendering off"," bodies",0,z(3500,8000,10000,2500,2000,3500,3000));
# order: ezEngine direct Jolt, Stride JIT, Stride AOT, Godot PhysX, Godot Jolt, PhysX RID, Jolt RID (same as @n)
chart("bodies1000-ms.svg","1000 awake bodies: ms per step"," ms",1,z(1.158,0.59,0.62,1.17,1.85,1.145,1.51));
chart("pile-game-ms.svg","Pile of 2000 bodies, default sleeping (game profile): ms per step"," ms",1,z(2.78,0.09,0.09,0.23,3.97,0.21,3.09));
chart("mixed-p99-stress.svg","Mixed scene with rendering, stress profile: p99 frame time"," ms",1,z(5.1,25.0,25.0,89.5,408,5.6,5.6));
chart("mixed-p99-game.svg","Mixed scene with rendering, game profile: p99 frame time"," ms",1,z(5.2,39.0,24.0,19.3,447,5.5,9.2));
chart("mixed-fps-game.svg","Mixed scene, render loop unlocked, game profile: frames per second"," fps",0,z(297,54,53,291,2.7,412,206));
chart("capacity-render-on-game.svg","Most bodies with p99 frame at or below 20 ms, rendering on, game profile"," bodies",0,z(2000,2000,2000,2500,1500,4000,2500));
chart("capacity-render-off-game.svg","Most bodies with p99 frame at or below 20 ms, rendering off, game profile"," bodies",0,z(3000,8000,8000,2500,2000,3500,3000));
chart("raycast-10000-ms.svg","10000 rays per frame: wall ms per frame"," ms",1,z(345,394,498,653,503,644,500));
chart("debris-render.svg","Debris (G): highest rate passing 20 ms p99, rendering on"," events/s",0,z(3,3,3,3,2.5,7,3));
chart("debris-physics.svg","Debris (G): highest rate passing 20 ms p99, rendering off"," events/s",0,z(3,8,10,5,2.5,8,3));
# Renderer A/B (2026-11-08, stages A and D, 3 runs per cell, median fps; values from .tmp/vk-ab and .tmp/vk-ab-godot, section 3.1a and 12.4 of docs/engine-choice.md)
my @ab = ("ezEngine direct Jolt DX11","ezEngine direct Jolt Vulkan","Godot Jolt Vulkan","Godot Jolt D3D12");
sub zab { my @v = @_; map { ($ab[$_], $v[$_]) } 0..$#v }
chart("renderer-ab-pit-physics-stress.svg","Renderer A/B, pile of 2000, physics mode, stress: frames per second"," fps",0,zab(1223.6,702.1,144.9,144.9));
chart("renderer-ab-pit-physics-game.svg","Renderer A/B, pile of 2000, physics mode, game: frames per second"," fps",0,zab(1178.8,728.9,144.7,144.9));
chart("renderer-ab-mixed-render-stress.svg","Renderer A/B, mixed scene with rendering, stress: frames per second"," fps",0,zab(288.3,259.3,2.8,2.8));
chart("renderer-ab-mixed-render-game.svg","Renderer A/B, mixed scene with rendering, game: frames per second"," fps",0,zab(277.8,231.3,2.8,2.8));

sub img { my ($f,$alt) = @_; "![$alt](docs/charts/$f)\n\n" }
print "### Physics cost per step (lower is better)\n\nEach value is the wall time per physics step with rendering off and the loop unpaced: 1000 / steps per second. Bars run best to worst (bars at 100% are green); the percentage is the best value divided by this value (best = 100%, 50% means twice as slow).\n\n";
print img("pile-ms.svg","Pile of 2000 awake bodies: ms per step"), img("mixed-ms.svg","Mixed scene: ms per step"), img("bodies4000-ms.svg","4000 awake bodies: ms per step");
print img("bodies1000-ms.svg","1000 awake bodies: ms per step"), img("pile-game-ms.svg","Pile of 2000 bodies, default sleeping (game profile): ms per step"), img("raycast-10000-ms.svg","10000 rays per frame: wall ms per frame");
print "### Frame time with rendering (lower is better)\n\n99th percentile frame time of the mixed scene with rendering on; the frame budget is 20 ms.\n\n";
print img("mixed-p99-stress.svg","Mixed scene with rendering, stress profile: p99 frame ms"), img("mixed-p99-game.svg","Mixed scene with rendering, game profile: p99 frame ms");
print "### Whole frame with rendering (higher is better)\n\nBest to worst; the percentage is this value divided by the best value. Charts without game in the title are the stress profile.\n\n";
print img("mixed-fps.svg","Mixed scene, render loop unlocked, stress: frames per second"), img("mixed-fps-game.svg","Mixed scene, render loop unlocked, game: frames per second"), img("capacity-render-on.svg","Most bodies at 20 ms p99, rendering on, stress"), img("capacity-render-on-game.svg","Most bodies at 20 ms p99, rendering on, game");
print "### Physics-only capacity (higher is better)\n\n";
print img("capacity-render-off.svg","Most bodies at 20 ms p99, rendering off, stress"), img("capacity-render-off-game.svg","Most bodies at 20 ms p99, rendering off, game");
print "### Debris (higher is better)\n\nScenario G always runs with default sleeping. The value is the highest debris rate (events per second, 64 pieces per event) that keeps the p99 frame time at or below 20 ms on the tested ladder.\n\n";
print img("debris-render.svg","Debris: highest rate at 20 ms p99, rendering on"), img("debris-physics.svg","Debris: highest rate at 20 ms p99, rendering off");
print "### Renderer A/B (higher is better)\n\nStages A and D only, median of 3 runs, one desktop, native Windows (no Steam Deck). ezEngine direct Jolt (the A/B ran on the direct Jolt path): the same exe with `-renderer DX11` and `-renderer Vulkan`; Godot: stock Jolt, nodes, `GODOT_RD_DRIVER=vulkan` and `d3d12`. ezEngine Vulkan is slower in every cell; Godot shows no difference beyond the run spread (details: docs/engine-choice.md sections 3.1a and 12.4). Median fps per cell (percentage against the same engine's other renderer):

| Cell | ezEngine direct Jolt DX11 | ezEngine direct Jolt Vulkan | Godot Jolt Vulkan | Godot Jolt D3D12 |
|---|---|---|---|---|
| pile of 2000, physics mode, stress | 1223.6 | 702.1 (-43%) | 144.9 | 144.9 (0%) |
| pile of 2000, physics mode, game | 1178.8 | 728.9 (-38%) | 144.7 | 144.9 (+0.1%) |
| mixed scene, rendering, stress | 288.3 | 259.3 (-10%) | 2.8 | 2.8 (0%) |
| mixed scene, rendering, game | 277.8 | 231.3 (-17%) | 2.8 | 2.8 (0%) |

The Godot physics-mode fps looks pacing-bound (identical on both drivers). These bars compare each engine with its own other renderer; the Godot and ezEngine rows are not one ranking (the main charts above use other cells).\n\n";
print img("renderer-ab-pit-physics-stress.svg","Renderer A/B, pile of 2000, physics mode, stress: fps"), img("renderer-ab-pit-physics-game.svg","Renderer A/B, pile of 2000, physics mode, game: fps");
print img("renderer-ab-mixed-render-stress.svg","Renderer A/B, mixed scene with rendering, stress: fps"), img("renderer-ab-mixed-render-game.svg","Renderer A/B, mixed scene with rendering, game: fps");
