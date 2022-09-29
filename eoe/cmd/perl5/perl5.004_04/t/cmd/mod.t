#!./perl

# $RCSfile: mod.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:11 $

print "1..11\n";

print "ok 1\n" if 1;
print "not ok 1\n" unless 1;

print "ok 2\n" unless 0;
print "not ok 2\n" if 0;

1 && (print "not ok 3\n") if 0;
1 && (print "ok 3\n") if 1;
0 || (print "not ok 4\n") if 0;
0 || (print "ok 4\n") if 1;

$x = 0;
do {$x[$x] = $x;} while ($x++) < 10;
if (join(' ',@x) eq '0 1 2 3 4 5 6 7 8 9 10') {
	print "ok 5\n";
} else {
	print "not ok 5 @x\n";
}

$x = 15;
$x = 10 while $x < 10;
if ($x == 15) {print "ok 6\n";} else {print "not ok 6\n";}

open(foo,'./TEST') || open(foo,'TEST') || open(foo,'t/TEST');
$x = 0;
$x++ while <foo>;
print $x > 50 && $x < 1000 ? "ok 7\n" : "not ok 7\n";

$x = -0.5;
print "not " if scalar($x) < 0 and $x >= 0;
print "ok 8\n";

print "not " unless (-(-$x) < 0) == ($x < 0);
print "ok 9\n";

print "ok 10\n" if $x < 0;
print "not ok 10\n" unless $x < 0;

print "ok 11\n" unless $x > 0;
print "not ok 11\n" if $x > 0;

