# Prints the README chart sections (mermaid horizontal bars, best to worst, label = value and percent of best) from the numbers below.
# Usage: perl benchmark/tools/gen-readme-charts.pl > charts.md, then paste over the chart sections of README.md.
# The values are copied by hand from the README numbers table: update them there and here together.
use strict; use warnings;
binmode(STDOUT, ":utf8");
# chart(title, unit, lower_better, ymax, [name, value]...) -> mermaid block, sorted best first, label "name value unit (pct%)"
sub chart {
  my ($title,$unit,$low,$ymax,@p)=@_; my $yl = $title =~ m{ms per step} ? "ms per step" : $title =~ m{frames} ? "fps" : "bodies"; my @d; push @d,[splice(@p,0,2)] while @p;
  @d = sort { $low ? $a->[1] <=> $b->[1] : $b->[1] <=> $a->[1] } @d;
  my $best=$d[0][1]; my (@l,@v);
  for my $d (@d){ my $pct = $low ? $best/$d->[1]*100 : $d->[1]/$best*100;
    my $v=$d->[1]; my $s = sprintf("%.5g",$v);
    push @l, sprintf('"%s: %s%s (%d%%)"',$d->[0],$s,$unit,int($pct+0.5)); push @v,$v; }
  # spacer categories with value 0 above the first and below the last bar: space between the bars and the plot border
  unshift @l, "\" \""; unshift @v, 0; push @l, "\"  \""; push @v, 0;
  # frontmatter config: wider plot, taller per bar, padding around titles, labels and axes (nothing touches the border)
  my $h = 100 + 64 * (@d + 2);
  my $cfg = "---\nconfig:\n  xyChart:\n    width: 1000\n    height: $h\n    titlePadding: 28\n    titleFontSize: 18\n    plotReservedSpacePercent: 60\n    xAxis:\n      labelFontSize: 14\n      labelPadding: 18\n      titlePadding: 18\n    yAxis:\n      labelFontSize: 14\n      labelPadding: 18\n      titlePadding: 18\n---\n";
  return "```mermaid\n${cfg}xychart-beta horizontal\n    title \"$title\"\n    x-axis [".join(", ",@l)."]\n    y-axis \"$yl\" 0 --> $ymax\n    bar [".join(", ",@v)."]\n```\n";
}
my @n=("ezEngine","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt","PhysX RID","Jolt RID");
sub z { my @v=@_; map { ($n[$_],$v[$_]) } 0..$#v }
print "### Physics cost per step (lower is better)\n\nEach value is the wall time per physics step with rendering off and the loop unpaced: 1000 / steps per second. Bars run best to worst; the percentage is the best value divided by this value (best = 100%, 50% means twice as slow).\n\n";
print chart("Pile of 2000 awake bodies: ms per step"," ms",1,5,z(2.52,1.02,1.04,2.24,3.99,2.18,3.14)),"\n";
print chart("Mixed scene (3000 bodies, 100 chains, 1000 rays per step): ms per step"," ms",1,6,z(2.23,1.25,1.33,3.85,5.03,3.30,3.50)),"\n";
print chart("4000 awake bodies: ms per step"," ms",1,10,z(5.05,1.69,1.71,5.30,9.37,4.73,6.26)),"\n";
print "### Whole frame with rendering (higher is better)\n\nBest to worst; the percentage is this value divided by the best value.\n\n";
print chart("Mixed scene, render loop unlocked: frames per second"," fps",0,320,z(299.1,53.5,50.6,67.6,2.8,262,217)),"\n";
my @c=("ezEngine","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt");
print chart("Most bodies with p99 frame at or below 20 ms, rendering on","","",3500,map { ($c[$_],(3000,2000,2000,2500,1500)[$_]) } 0..4),"\n";
print "### Physics-only capacity (higher is better)\n\n";
print chart("Most bodies with p99 frame at or below 20 ms, rendering off","",0,10500,map { ($c[$_],(3500,8000,10000,2500,2000)[$_]) } 0..4),"\n";
