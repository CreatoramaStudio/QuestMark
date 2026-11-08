#!/usr/bin/perl
# Writes the README benchmark charts as standalone SVG files (docs/charts/*.svg) and prints the README chart sections (markdown image links).
# Bars run best to worst; each label shows the value and the percentage of the best (best = 100%). Own background, so light and dark GitHub themes both read well.
# The values are copied by hand from the README numbers table: update them there and here together.
# Usage: perl benchmark/tools/gen-charts.pl > .tmp/charts.md   (then paste over the chart sections of README.md between the markers)
use strict; use warnings; use FindBin;
my $dir = "$FindBin::Bin/../../docs/charts"; mkdir $dir unless -d $dir;
my @n = ("ezEngine","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt","PhysX RID","Jolt RID");
sub z { my @v = @_; map { ($n[$_], $v[$_]) } 0..$#v }
sub esc { my $s = shift; $s =~ s/&/&amp;/g; $s =~ s/</&lt;/g; $s =~ s/>/&gt;/g; $s }
# chart(file, title, unit, lower_better, [name, value]...) writes the svg
sub chart {
  my ($file,$title,$unit,$low,@p) = @_; my @d; push @d,[splice(@p,0,2)] while @p;
  @d = sort { $low ? $a->[1] <=> $b->[1] : $b->[1] <=> $a->[1] } @d;
  my ($best,$max) = ($d[0][1], (sort { $b <=> $a } map { $_->[1] } @d)[0]);
  my ($W,$pad,$rowh,$barh,$lab,$txt) = (780,32,40,24,150,150);
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
    my $cls = $i == 1 ? "best" : "bar"; my $vs = sprintf("%.5g",$v);
    $s .= qq(<text x="@{[$x0-10]}" y="@{[$y+$barh/2+5]}" font-size="14" text-anchor="end" class="t">@{[esc($name)]}</text>\n);
    $s .= qq(<rect x="$x0" y="$y" width="@{[sprintf('%.1f',$len)]}" height="$barh" rx="3" class="$cls"/>\n);
    $s .= qq(<text x="@{[sprintf('%.1f',$x0+$len+8)]}" y="@{[$y+$barh/2+5]}" font-size="14" class="t">$vs$unit<tspan class="m"> · @{[int($pct+0.5)]}%</tspan></text>\n);
  }
  $s .= "</svg>\n";
  open my $f, ">:raw", "$dir/$file" or die; print $f $s; close $f;
}
my @c = ("ezEngine","Stride JIT","Stride AOT","Godot PhysX","Godot Jolt");
sub zc { my @v = @_; map { ($c[$_], $v[$_]) } 0..$#v }
chart("pile-ms.svg","Pile of 2000 awake bodies: ms per step"," ms",1,z(2.52,1.02,1.04,2.24,3.99,2.18,3.14));
chart("mixed-ms.svg","Mixed scene (3000 bodies, 100 chains, 1000 rays per step): ms per step"," ms",1,z(2.23,1.25,1.33,3.85,5.03,3.30,3.50));
chart("bodies4000-ms.svg","4000 awake bodies: ms per step"," ms",1,z(5.05,1.69,1.71,5.30,9.37,4.73,6.26));
chart("mixed-fps.svg","Mixed scene, render loop unlocked: frames per second"," fps",0,z(299.1,53.5,50.6,67.6,2.8,262,217));
chart("capacity-render-on.svg","Most bodies with p99 frame at or below 20 ms, rendering on"," bodies",0,z(3000,2000,2000,2500,1500,3000,2000));
chart("capacity-render-off.svg","Most bodies with p99 frame at or below 20 ms, rendering off"," bodies",0,z(3500,8000,10000,2500,2000,3000,3000));
sub img { my ($f,$alt) = @_; "![$alt](docs/charts/$f)\n\n" }
print "### Physics cost per step (lower is better)\n\nEach value is the wall time per physics step with rendering off and the loop unpaced: 1000 / steps per second. Bars run best to worst (the best bar is green); the percentage is the best value divided by this value (best = 100%, 50% means twice as slow).\n\n";
print img("pile-ms.svg","Pile of 2000 awake bodies: ms per step"), img("mixed-ms.svg","Mixed scene: ms per step"), img("bodies4000-ms.svg","4000 awake bodies: ms per step");
print "### Whole frame with rendering (higher is better)\n\nBest to worst; the percentage is this value divided by the best value.\n\n";
print img("mixed-fps.svg","Mixed scene, render loop unlocked: frames per second"), img("capacity-render-on.svg","Most bodies at 20 ms p99, rendering on");
print "### Physics-only capacity (higher is better)\n\n";
print img("capacity-render-off.svg","Most bodies at 20 ms p99, rendering off");
