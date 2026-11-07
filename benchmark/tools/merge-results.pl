#!/usr/bin/env perl
# Merge benchmark CSVs into markdown tables (SUMMARY.md). Usage:
#   perl merge-results.pl <results-dir> > SUMMARY.md
#   perl merge-results.pl --eval <csv> [limit2_ms]     one-run pass/fail line (used by run-all.sh for the F and G ladders)
# Reads <engine>-<backend>-<token>-<mode>-<run>.csv (files containing ".failed" are ignored), the matching
# .csv.summary.txt (fps, steps_per_s, working set, awake at end, GC) and .env.txt (Jolt limits). Statistics come from the CSV rows.
# Percentiles: nearest rank on sorted values (same as the engine summaries). Cell values are the median across runs of the
# per-run statistic unless stated. Tokens: pit-2000 raycast-1000 raycast-10000 chains mixed (A-D); unlock-render-mixed
# unlock-physics-mixed unlock-physics-pit-2000 (E); scale-<N> and scale-unlock-<N> (F; Godot writes the unpaced stage as
# scale-<N>-unlock-physics, normalised here); debris-<R> and debris-pooled-<R> (G, extra CSV columns event_ms despawn_ms debris_live).
use strict; use warnings;

sub med { my @s = sort { $a <=> $b } @_; return undef unless @s; my $n = @s; return $n % 2 ? $s[$n/2] : ($s[$n/2-1]+$s[$n/2])/2; }
sub pct { my ($p, @s) = @_; return 0 unless @s; my $i = int($p*@s + 0.999999999) - 1; $i = 0 if $i < 0; return $s[$i]; }
sub mean { return undef unless @_; my $t = 0; $t += $_ for @_; return $t/@_; }
sub f { my ($v, $d) = @_; return '-' unless defined $v; return sprintf("%.${d}f", $v); }
sub srt { return sort { $a <=> $b } @_; }

# ---- one run -> metrics hash ----
sub read_run {
  my ($path) = @_;
  open(my $fh, '<', $path) or die "$path: $!\n";
  my $hdr = <$fh>; chomp $hdr; $hdr =~ s/\r//; my @h = split /,/, $hdr;
  my %ix; @ix{@h} = (0..$#h);
  my (@w, @p, @q, @aw, @ev, @de, @dl, @wev, @wnev, @wde);
  my $has_ev = exists $ix{event_ms};
  my $si = exists $ix{steps_per_frame} ? $ix{steps_per_frame} : undef; my $nsteps = 0; my @spf;
  while (<$fh>) { chomp; s/\r//; my @c = split /,/; next unless @c >= 9;
    push @w, $c[1]; push @p, $c[2]; push @q, $c[3]; push @aw, $c[8];
    if (defined $si) { my $s = $c[$si] // 0; $nsteps += $s; push @spf, $s if $s > 0; }
    if ($has_ev) {
      my $e = $c[$ix{event_ms}] // 0; my $d = exists $ix{despawn_ms} ? ($c[$ix{despawn_ms}] // 0) : 0;
      my $l = exists $ix{debris_live} ? ($c[$ix{debris_live}] // 0) : 0;
      push @dl, $l;
      if ($e > 0) { push @ev, $e; push @wev, $c[1]; } else { push @wnev, $c[1]; }
      if ($d > 0) { push @de, $d; push @wde, $c[1]; }
    } }
  close $fh;
  my %m; $m{rows} = scalar @w; $m{nsteps} = defined $si ? $nsteps : undef; $m{spf_max} = defined $si && @spf ? (srt(@spf))[-1] : undef;
  my @ws = srt(@w); my @pt = grep { $_ > 0 } @p; my @pts = srt(@pt);
  $m{sumwall} = 0; $m{sumwall} += $_ for @w; $m{sumwall} /= 1000;
  $m{wmed} = med(@w); $m{p95} = pct(0.95,@ws); $m{p99} = pct(0.99,@ws); $m{max} = $ws[-1];
  $m{pmed} = med(@p); $m{qmed} = med(@q);
  if (@pts) { $m{pt_med} = med(@pts); $m{pt_p95} = pct(0.95,@pts); $m{pt_p99} = pct(0.99,@pts); $m{pt_max} = $pts[-1]; }
  my @wtick; for my $i (0..$#w) { push @wtick, $w[$i] if $p[$i] > 0; }
  $m{wtick_p99} = pct(0.99, srt(@wtick)) if @wtick;
  my @as = srt(@aw); $m{awmin} = $as[0]; $m{awmax} = $as[-1];
  my $first = 0; for my $i (0 .. ($#w < 119 ? $#w : 119)) { $first = $w[$i] if $w[$i] > $first; } $m{first} = $first;
  # first 5 s / last 5 s of the measured window (frames by cumulative wall time), physics_ms over tick frames
  my $tot = $m{sumwall}*1000; my $cum = 0; my (@pf, @pl, @wf, @wl);
  for my $i (0..$#w) { my $s = $cum; $cum += $w[$i];
    if ($cum <= 5000) { push @wf, $w[$i]; push @pf, $p[$i] if $p[$i] > 0; }
    if ($s >= $tot - 5000) { push @wl, $w[$i]; push @pl, $p[$i] if $p[$i] > 0; } }
  $m{p_first5} = med(@pf) if @pf; $m{p_last5} = med(@pl) if @pl;
  $m{w_first5} = pct(0.99,srt(@wf)) if @wf; $m{w_last5} = pct(0.99,srt(@wl)) if @wl;
  if ($has_ev) {
    $m{has_ev} = 1; $m{events} = scalar @ev; $m{removals} = scalar @de;
    if (@ev) { my @e = srt(@ev); $m{ev_med} = med(@e); $m{ev_p95} = pct(0.95,@e); $m{ev_p99} = pct(0.99,@e); $m{ev_max} = $e[-1];
               $m{wev_max} = (srt(@wev))[-1]; $m{wev_p99} = pct(0.99, srt(@wev)); }
    if (@de) { my @d = srt(@de); $m{de_med} = med(@d); $m{de_p95} = pct(0.95,@d); $m{de_p99} = pct(0.99,@d); $m{de_max} = $d[-1];
               $m{wde_max} = (srt(@wde))[-1]; }
    $m{wnev_p99} = pct(0.99, srt(@wnev)) if @wnev;
    if (@dl) { $m{dl_max} = (srt(@dl))[-1]; $m{dl_mean} = mean(@dl); }
  }
  # summary and env files
  my $sf; if (open($sf, '<', "$path.summary.txt")) { while (<$sf>) {
      $m{gc} = $1 if /(?:gc_pause_total_ms|total_gc_pause_ms):?=?\s*([\d.]+)/;
      $m{fps} = $1 if /^fps[:=]\s*([\d.]+)/;
      $m{steps} = $1 if /^steps_per_s[:=]\s*([\d.]+)/;
      $m{ws} = $1 if /^working_set_mb[:=]\s*([\d.]+)/;
      $m{awake_end} = $1 if /^awake_bodies_at_end[:=]\s*(\d+)/ || /^awake_bodies_last[:=]\s*(\d+)/;
    } close $sf; }
  (my $envf = $path) =~ s/\.csv$/.env.txt/;
  my $ef; if (open($ef, '<', $envf)) { my %l; my $profile_line = ""; while (<$ef>) {
      $m{sleeping} = $1 if /^sleeping:\s*(\w+)/; $m{hz} = $1 if /^physics_hz:\s*(\d+)/;
      $profile_line = $1 if /^jolt_limit_profile_applied_by_extension:\s*(.+?)\s*$/;
      $l{pairs} = $1 if /^jolt max_body_pairs:\s*(\d+)/; $l{bodies} = $1 if /^jolt max_bodies:\s*(\d+)/; $l{cc} = $1 if /^jolt max_contact_constraints:\s*(\d+)/;
      $l{tmp} = $1 if /^jolt temporary_memory_buffer_size:\s*(\d+)/; }
    close $ef;
    $m{tmp} = $l{tmp};
    $m{lim} = $1 if $profile_line =~ /^(\w+)/;
    if (!defined $m{lim} && defined $l{pairs}) { $m{lim} = $l{pairs} == 65536 ? 'old' : $l{pairs} == 1048576 ? 'raised' : "pairs $l{pairs}"; } }
  $m{rtf} = rtf(\%m); return \%m;
}

# ---- pass rule (SPEC F and G) ----
sub frames_ok { my ($m) = @_;   # PROPOSAL (SPEC \"Frame rate\"): real-time factor, simulated s / wall s at 200 Hz, within 1 percent of 1.0 or above
  return 0 unless defined $m->{nsteps} && $m->{sumwall} > 0;
  return $m->{nsteps} >= 0.99 * 200 * $m->{sumwall} ? 1 : 0; }
sub rtf { my ($m) = @_; return (defined $m->{nsteps} && $m->{sumwall} > 0) ? $m->{nsteps} * 0.005 / $m->{sumwall} : undef; }
sub run_pass { my ($m, $lim20, $lim2, $evlim) = @_;   # -> (pass at 20, pass at limit2)
  my $fo = frames_ok($m); my $evok = 1;
  if ($m->{has_ev}) { $evok = defined $m->{ev_p99} && $m->{ev_p99} <= $evlim ? 1 : 0; }
  return ($fo && $m->{p99} <= $lim20 && $evok ? 1 : 0, $fo && $m->{p99} <= $lim2 && $evok ? 1 : 0); }

if (@ARGV && $ARGV[0] eq '--eval') {
  my $csv = $ARGV[1] or die "usage: merge-results.pl --eval <csv> [limit2_ms]\n"; my $l2 = $ARGV[2] // 33.3;
  my $m = read_run($csv); my ($a, $b) = run_pass($m, 20, $l2, 8);
  printf "rows=%d steps=%s rtf=%s p99=%.3f event_p99=%s rtf_ok=%d pass20=%d pass2=%d\n", $m->{rows}, defined $m->{nsteps} ? $m->{nsteps} : 'na', defined rtf($m) ? sprintf("%.4f", rtf($m)) : 'na', $m->{p99},
    defined $m->{ev_p99} ? sprintf("%.3f", $m->{ev_p99}) : 'na', frames_ok($m), $a, $b;
  exit 0;
}

# ---- collect ----
my $dir = shift or die "usage: merge-results.pl <results-dir>\n";
opendir(my $dh, $dir) or die "cannot open $dir: $!\n";
my $FN = qr/^(godot|stride)-([A-Za-z0-9_]+?)(?:-(?!stress-)([A-Za-z0-9_]+?))?(-stress)?-((?:pit|raycast|chains|mixed|unlock|scale|debris)[A-Za-z0-9_.-]*)-(physics|render)-(\d+)\.csv$/;
my @files = sort grep { $_ =~ $FN && !/\.failed/ } readdir $dh;
closedir $dh;
my %cell;
for my $file (@files) {
  my ($eng, $be, $fl, $st, $sc, $mode, $run) = $file =~ $FN or next;
  $be .= "-$fl" if defined $fl; my $prof = $st ? 'stress' : 'game';
  $sc =~ s/^scale-(\d+)-unlock$/scale-unlock-$1/;     # Godot name of the unpaced stage
  my $m = read_run("$dir/$file");
  my $c = $cell{"$eng/$be|$sc|$mode|$prof"} ||= { runs => [] };
  push @{$c->{runs}}, $m;
}
sub cm { my ($c, $k) = @_; return med(grep { defined } map { $_->{$k} } @{$c->{runs}}); }
sub cr { my ($c, $k) = @_; my @v = srt(grep { defined } map { $_->{$k} } @{$c->{runs}}); return @v ? ($v[0], $v[-1]) : (undef, undef); }
sub slpcell { my ($c) = @_; my %s = map { ($_->{sleeping} // 'n/a') => 1 } @{$c->{runs}}; return join('+', sort keys %s); }
sub limcell { my ($c) = @_; my %s = map { (($_->{lim} // "n/a") . (defined $_->{tmp} ? " tmp$_->{tmp}MiB" : "")) => 1 } @{$c->{runs}}; return join('+', sort keys %s); }
sub cellpass { my ($c, $l2, $evl) = @_;   # median p99 within limit and frames ok in every run
  my $p99 = cm($c,'p99'); my $fo = 1; $fo &&= frames_ok($_) for @{$c->{runs}};
  my $evok = 1; if ($c->{runs}[0]{has_ev}) { my $e = cm($c,'ev_p99'); $evok = defined $e && $e <= $evl; }
  return ($fo && $p99 <= 20 && $evok ? 1 : 0, $fo && $p99 <= $l2 && $evok ? 1 : 0); }
sub key_parts { my ($k) = @_; return split /\|/, $k; }
sub keys_where { my ($re) = @_; return grep { (key_parts($_))[1] =~ $re } keys %cell; }

# ---- header ----
print "# Benchmark summary\n\n";
print "Physics rate: fixed 200 Hz (dt 0.005 s), render loop unlocked (vsync off, no frame cap). Results recorded at 60 Hz physics (older CSVs without the steps_per_frame column) are NOT comparable with 200 Hz results and are not given a pass verdict here. Profile: game = engine default sleeping, stress = sleeping off for A, D (mixed), F; B, C and G are the same in both profiles (see SPEC Profiles).\n\n";
print "Known deviations between the engines (SPEC \"Settings parity\"; read the SPEC table for values and reasons): no restitution in Bepu; solver iterations and substeps are not comparable between Jolt and Bepu (both recorded); damping formulas differ at equal nominal 0.1; Jolt limit profiles old/raised exist only for Godot; mixed ceiling is static in Godot and kinematic in Stride; RNG sequences differ; physics_ms stopwatches bound different work; shadow filtering and cascade implementations differ by engine. Use tools/parity_check.pl on two env.txt files for the per-run key comparison.\n\n";

# ---- A to D ----
my %so = ('pit-2000'=>1,'raycast-1000'=>2,'raycast-10000'=>3,'chains'=>4,'mixed'=>5);
my @abcd = sort { my @a = key_parts($a); my @b = key_parts($b);
  $a[0] cmp $b[0] or ($a[3] cmp $b[3]) or ($so{$a[1]}||9) <=> ($so{$b[1]}||9) or $a[2] cmp $b[2] } grep { (key_parts($_))[1] !~ /^(unlock-|scale-|debris)/ } keys %cell;
print "## Scenarios A to D (physics fixed 200 Hz, render loop unlocked)\n\n" if @abcd;
if (@abcd) {
print "| engine/backend | profile | sleeping | scenario | mode | runs | frames (min-max) | RTF med | wall med | wall p95 | wall p99 | wall max | physics med | physics tick-only med | query med | awake min-max | GC pause total | first-120 max wall | p99 spread (min-max) | Jolt limits |\n";
print "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
for my $k (@abcd) {
  my ($e, $s, $m, $p) = key_parts($k); my $c = $cell{$k};
  my ($r0,$r1) = cr($c,'rows'); my ($p0,$p1) = cr($c,'p99'); my ($a0) = cr($c,'awmin'); my (undef,$a1) = cr($c,'awmax');
  my $gc = cm($c,'gc');
  printf "| %s | %s | %s | %s | %s | %d | %d-%d | %s | %s | %s | %s | %s | %s | %s | %s | %d-%d | %s | %s | %s-%s | %s |\n",
    $e, $p, slpcell($c), $s, $m, scalar @{$c->{runs}}, $r0, $r1, f(cm($c,'rtf'),4), f(cm($c,'wmed'),2), f(cm($c,'p95'),2), f(cm($c,'p99'),2), f(cm($c,'max'),2),
    f(cm($c,'pmed'),3), f(cm($c,'pt_med'),3), f(cm($c,'qmed'),3), $a0, $a1, (defined $gc ? f($gc,2) : 'n/a'), f(cm($c,'first'),2), f($p0,2), f($p1,2), limcell($c);
}
print "\nUnits: ms. Cells are the median across runs of the per-run statistic (rows, awake and p99 spread: min and max across runs). physics tick-only = frames with physics_ms > 0. GC pause total = summary value (Stride; Godot native n/a), median across runs. Jolt limits: old = max_body_pairs 65536, raised = 1048576 as recorded in env.txt raised also sets the Jolt temporary memory buffer to 1024 MiB (env.txt `jolt temporary_memory_buffer_size`, MiB; the 32 MiB default overflowed in F and G, so results of F and G from before this change are affected). n/a: env.txt has no limit lines, older runs.\n\n";
}

# ---- E ----
my @e = sort { my @a = key_parts($a); my @b = key_parts($b); $a[0] cmp $b[0] or ($a[3] cmp $b[3]) or $a[1] cmp $b[1] or $a[2] cmp $b[2] } keys_where(qr/^unlock-/);
if (@e) {
print "## Scenario E (unlock: uncapped frame rate and unpaced physics)\n\n";
print "| engine/backend | profile | sleeping | scenario | mode | runs | fps | steps/s | wall med | wall p95 | wall p99 | wall max | wall p99 tick frames | physics med | physics tick p95 | physics tick p99 | physics tick max | awake min-max | Jolt limits |\n";
print "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
for my $k (@e) { my ($en, $s, $m, $p) = key_parts($k); my $c = $cell{$k}; my ($a0) = cr($c,'awmin'); my (undef,$a1) = cr($c,'awmax');
  printf "| %s | %s | %s | %s | %s | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %d-%d | %s |\n", $en, $p, slpcell($c), $s, $m, scalar @{$c->{runs}},
    f(cm($c,'fps'),1), f(cm($c,'steps'),1), f(cm($c,'wmed'),3), f(cm($c,'p95'),3), f(cm($c,'p99'),3), f(cm($c,'max'),3), f(cm($c,'wtick_p99'),3),
    f(cm($c,'pt_med'),3), f(cm($c,'pt_p95'),3), f(cm($c,'pt_p99'),3), f(cm($c,'pt_max'),3), $a0, $a1, limcell($c); }
print "\nUnits: ms. fps = frames / measured s and steps/s = physics steps / measured s, both from the run summaries (median across runs; '-' when the summary lacks the line). Physics columns are over tick frames. In unlock-physics runs one CSV row is one step (Stride) or one frame with one tick (Godot), so wall columns are step durations.\n\n";
}

# ---- F ----
my @f = sort { my @a = key_parts($a); my @b = key_parts($b); my ($na) = $a[1] =~ /(\d+)$/; my ($nb) = $b[1] =~ /(\d+)$/;
  $a[0] cmp $b[0] or ($a[3] cmp $b[3]) or $a[2] cmp $b[2] or ($a[1] =~ /unlock/ ? 1 : 0) <=> ($b[1] =~ /unlock/ ? 1 : 0) or $na <=> $nb } keys_where(qr/^scale-/);
if (@f) {
print "## Scenario F (scale: simultaneous dynamic bodies, physics 200 Hz, render loop unlocked unless 'unlock')\n\n";
print "| engine/backend | profile | sleeping | variant | mode | N | runs | steps (run / 200 Hz x s) | wall med | wall p95 | wall p99 | wall max | physics med | physics tick p99 | physics first 5 s | physics last 5 s | awake at end | working set MB | steps/s | pass 20 ms | pass 33.3 ms | Jolt limits |\n";
print "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
my %lad;   # engine|mode -> { N -> [pass20, pass33] }
for my $k (@f) { my ($en, $s, $m, $p) = key_parts($k); my $c = $cell{$k}; my ($n) = $s =~ /(\d+)$/; my $unl = $s =~ /unlock/ ? 'unlock' : 'paced';
  my ($rn) = cr($c,'rows'); my ($pa, $pb) = $unl eq 'paced' ? cellpass($c, 33.3, 8) : ('n/a','n/a');
  $lad{"$en|$m|$p"}{$n} = [$pa, $pb] if $unl eq 'paced';
  printf "| %s | %s | %s | %s | %s | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", $en, $p, slpcell($c), $unl, $m, $n, scalar @{$c->{runs}},
    $unl eq 'paced' ? sprintf('%d / %.0f', cm($c,'nsteps') // 0, 200*cm($c,'sumwall')) : sprintf('%d rows', cm($c,'rows')), f(cm($c,'wmed'),2), f(cm($c,'p95'),2), f(cm($c,'p99'),2), f(cm($c,'max'),2), f(cm($c,'pmed'),3), f(cm($c,'pt_p99'),3),
    f(cm($c,'p_first5'),3), f(cm($c,'p_last5'),3), (defined cm($c,'awake_end') ? int(cm($c,'awake_end')) : '-'), f(cm($c,'ws'),0), f(cm($c,'steps'),1), $pa, $pb, limcell($c); }
print "\nPass rule (PROPOSAL, SPEC \"Frame rate\"; paced rows only): median p99 wall <= limit AND real-time factor (steps x 0.005 s / measured wall s) >= 0.99 in every run (200 Hz physics keeps up with the wall clock; the loop itself is unlocked). 'physics tick p99' = p99 of physics_ms over tick frames. Unlock rows have no pass rule, read steps/s. Cells are medians across runs.\n\n";
print "### F ladder result\n\n| engine/backend | profile | mode | tested N | last pass 20 ms | first fail 20 ms | last pass 33.3 ms | first fail 33.3 ms |\n|---|---|---|---|---|---|---|---|\n";
for my $lk (sort keys %lad) { my ($en, $m, $p) = split /\|/, $lk; my @ns = sort { $a <=> $b } keys %{$lad{$lk}}; my ($lp, $ff, $lp2, $ff2) = ('-','-','-','-');
  for my $n (@ns) { my ($a, $b) = @{$lad{$lk}{$n}}; if ($a) { $lp = $n; } elsif ($ff eq '-') { $ff = $n; }
    if ($b) { $lp2 = $n; } elsif ($ff2 eq '-') { $ff2 = $n; } }
  print "| $en | $p | $m | ", join(' ', @ns), " | $lp | $ff | $lp2 | $ff2 |\n"; }
print "\nLast pass = the highest tested N that passes (a pass above the first fail, from noise, is shown but the first fail stays). Only tested stages count: '-' for first fail means no tested stage failed (capacity may be higher), '-' for last pass means no tested stage passed.\n\n";
}

# ---- G ----
my @g = sort { my @a = key_parts($a); my @b = key_parts($b); my ($ra) = $a[1] =~ /([\d.]+)$/; my ($rb) = $b[1] =~ /([\d.]+)$/;
  $a[0] cmp $b[0] or ($a[3] cmp $b[3]) or $a[2] cmp $b[2] or ($a[1] =~ /pooled/ ? 1 : 0) <=> ($b[1] =~ /pooled/ ? 1 : 0) or $ra <=> $rb } keys_where(qr/^debris/);
if (@g) {
print "## Scenario G (debris: runtime-created bodies, R events per second, 64 pieces per event, lifetime 8 s)\n\n";
print "| engine/backend | profile | sleeping | variant | mode | R | runs | events | event med | event p95 | event p99 | event max | despawn med | despawn p99 | despawn max | wall p99 event frames | wall p99 other frames | wall max event | wall max removal | wall p99 | steps (run / 200 Hz x s) | debris live max | debris live mean | R x 512 | awake min-max | working set MB | pass 20 ms | pass 36 ms | Jolt limits |\n";
print "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
my %lad;
for my $k (@g) { my ($en, $s, $m, $p) = key_parts($k); my $c = $cell{$k}; my ($r) = $s =~ /([\d.]+)$/; my $var = $s =~ /pooled/ ? 'pooled' : 'created';
  my ($pa, $pb) = cellpass($c, 36, 8); $lad{"$en|$m|$p"}{$r} = [$pa, $pb] if $var eq 'created';
  my ($a0) = cr($c,'awmin'); my (undef,$a1) = cr($c,'awmax');
  printf "| %s | %s | %s | %s | %s | %s | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %d / %.0f | %s | %s | %.0f | %d-%d | %s | %s | %s | %s |\n", $en, $p, slpcell($c), $var, $m, $r, scalar @{$c->{runs}},
    f(cm($c,'events'),0), f(cm($c,'ev_med'),3), f(cm($c,'ev_p95'),3), f(cm($c,'ev_p99'),3), f(cm($c,'ev_max'),3), f(cm($c,'de_med'),3), f(cm($c,'de_p99'),3), f(cm($c,'de_max'),3),
    f(cm($c,'wev_p99'),2), f(cm($c,'wnev_p99'),2), f(cm($c,'wev_max'),2), f(cm($c,'wde_max'),2), f(cm($c,'p99'),2), cm($c,'nsteps') // 0, 200*cm($c,'sumwall'),
    f(cm($c,'dl_max'),0), f(cm($c,'dl_mean'),0), $r*512, $a0 // 0, $a1 // 0, f(cm($c,'ws'),0), $pa, $pb, limcell($c); }
print "\nPass rule (SPEC G, paced): median p99 wall <= 20 ms (36 ms for the 30 fps variant) AND real-time factor >= 0.99 in every run (PROPOSAL, SPEC \"Frame rate\") AND median p99 event_ms <= 8 ms. event/despawn statistics are over frames with a value > 0. 'wall p99 event frames' against 'other frames' is the per-event hitch. Pooled rows are the optional pre-created-pieces variant.\n\n";
print "### G ladder result\n\n| engine/backend | profile | mode | tested R | max R pass 20 ms | first fail 20 ms | max R pass 36 ms | first fail 36 ms |\n|---|---|---|---|---|---|---|---|\n";
for my $lk (sort keys %lad) { my ($en, $m, $p) = split /\|/, $lk; my @rs = sort { $a <=> $b } keys %{$lad{$lk}}; my ($lp, $ff, $lp2, $ff2) = ('-','-','-','-');
  for my $r (@rs) { my ($a, $b) = @{$lad{$lk}{$r}}; if ($a) { $lp = $r; } elsif ($ff eq '-') { $ff = $r; }
    if ($b) { $lp2 = $r; } elsif ($ff2 eq '-') { $ff2 = $r; } }
  print "| $en | $p | $m | ", join(' ', @rs), " | $lp | $ff | $lp2 | $ff2 |\n"; }
print "\n";
}
