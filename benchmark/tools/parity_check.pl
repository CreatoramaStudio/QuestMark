#!/usr/bin/env perl
# Compare two env.txt files key by key and print the mismatches (SPEC "Settings parity").
# Usage: perl parity_check.pl <a.env.txt> <b.env.txt> [--all]
#   All engines (Godot, Stride, ezEngine) write the same plain `key: value` names for the compared keys. Godot additionally writes `parity.*` details
#   (gravity, friction, damping, shapes, ...); keys that exist in only one file are listed as "only in A/B" and never count as
#   a mismatch. Values are compared on the first token (the text after the number is a free-form annotation); numbers with a
#   relative tolerance of 1e-3 (absolute 0.002), vectors "(x, y, z)" component-wise; values starting with "n/a" on either side
#   are reported as n/a, not as a mismatch.
# Exit code: 0 = no mismatch, 1 = at least one mismatch, 2 = usage / file error.
use strict; use warnings;
my @files = grep { !/^--/ } @ARGV; my $all = grep { $_ eq '--all' } @ARGV;
die "usage: parity_check.pl <a.env.txt> <b.env.txt> [--all]\n" unless @files == 2;

# compared keys (the SPEC target values are the single source of truth; these must be equal in every run of the same profile and mode)
my @keys = qw(profile sleeping shadows shadow_map_size msaa vsync max_fps physics_hz solver_iterations solver_substeps
              camera_mode camera_position camera_look_at camera_fov_v_deg camera_near camera_far camera_content_radius camera_content_aabb);
# keys that are legitimately different per engine and only recorded (never a mismatch): listed with both values
my %known = (solver_iterations => 'Jolt velocity_steps vs Bepu solver iterations are not the same quantity',
             solver_substeps   => 'Bepu substeps have no Jolt equivalent',
             max_fps           => 'Stride physics mode keeps a 60 Hz frame loop (no rendering); Godot physics mode is unlocked');

sub load { my ($p) = @_; local $_; open(my $fh, '<', $p) or die "$p: $!\n"; my %h;
  while (<$fh>) { s/\r?\n$//; next unless /^([A-Za-z0-9_.]+):\s*(.*)$/; $h{$1} = $2 unless exists $h{$1}; } close $fh; return \%h; }
my ($A, $B) = map { load($_) } @files;
sub nums { my ($v) = @_; return [ $v =~ /(-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)/g ]; }
sub first_tok { my ($v) = @_; $v =~ s/^\s+//; $v =~ s/\s.*$//; return $v; }
sub same { my ($x, $y) = @_;
  return 'na' if $x =~ /^n\/a/ || $y =~ /^n\/a/;
  if ($x =~ /^\(/ || $x =~ /^min\s*\(/) { my ($a, $b) = (nums($x), nums($y)); return 0 unless @$a == @$b;
    for my $i (0..$#$a) { return 0 if abs($a->[$i]-$b->[$i]) > 0.002 + 1e-3*abs($b->[$i]); } return 1; }
  my ($fx, $fy) = (first_tok($x), first_tok($y));
  if ($fx =~ /^-?\d+(\.\d+)?$/ && $fy =~ /^-?\d+(\.\d+)?$/) { return abs($fx-$fy) <= 0.002 + 1e-3*abs($fy) ? 1 : 0; }
  return lc($fx) eq lc($fy) ? 1 : 0; }

my ($mis, $ok, $na) = (0, 0, 0);
printf "A = %s\nB = %s\n\n", @files;
for my $k (@keys) {
  my ($x, $y) = ($A->{$k}, $B->{$k});
  if (!defined $x || !defined $y) { printf "MISSING  %-26s only in %s\n", $k, defined $x ? 'A' : defined $y ? 'B' : 'neither'; $mis++ if defined $x || defined $y; next; }
  my $s = same($x, $y);
  if ($s eq 'na') { $na++; print "n/a      $k: A=[$x] B=[$y]\n" if $all; next; }
  if ($s) { $ok++; print "equal    $k: $x\n" if $all; next; }
  if ($known{$k}) { printf "KNOWN    %-26s A=[%s] B=[%s]  (%s)\n", $k, $x, $y, $known{$k}; next; }
  $mis++; printf "MISMATCH %-26s A=[%s] B=[%s]\n", $k, $x, $y;
}
if ($all) { my %seen = map { $_ => 1 } @keys;
  for my $k (sort keys %$A) { next if $seen{$k} || exists $B->{$k}; printf "only A   %s: %s\n", $k, substr($A->{$k}, 0, 80) if $k =~ /^parity\./; }
  for my $k (sort keys %$B) { next if $seen{$k} || exists $A->{$k}; printf "only B   %s: %s\n", $k, substr($B->{$k}, 0, 80); } }
# Jolt-only keys have a space in env.txt ("jolt <name>: ..."), so load() skips them. The temporary allocator size is shown as information
# only, never a mismatch (Stride/Bepu has no such buffer; raised profile = 1024 MiB).
sub raw_jolt { my ($p, $name) = @_; open(my $fh, '<', $p) or return 'n/a'; while (<$fh>) { s/\r?\n$//; return $1 if /^jolt \Q$name\E[:]\s*(.*)$/; } return 'n/a'; }
printf "INFO     %-26s A=[%s] B=[%s]  (Jolt-only, MiB; Stride/Bepu has no such buffer, ezEngine uses a growing allocator)\n", 'jolt temp_buffer_size', raw_jolt($files[0], 'temporary_memory_buffer_size'), raw_jolt($files[1], 'temporary_memory_buffer_size');
# sleep rules differ per engine by design (SPEC "Settings parity", row sleep): shown as information only, never a mismatch.
printf "INFO     %-26s A=[%s] B=[%s]  (engines differ: Jolt 0.03 m/s point velocity, PhysX energy threshold, Bepu unchecked; not forced equal)\n", 'sleep (jolt threshold)', raw_jolt($files[0], 'sleep_velocity_threshold'), raw_jolt($files[1], 'sleep_velocity_threshold');
printf "\nparity_check:%d equal, %d mismatch, %d n/a (physics mode), see KNOWN lines for recorded engine differences\n", $ok, $mis, $na;
exit($mis ? 1 : 0);
