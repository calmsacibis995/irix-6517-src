#!./perl

# $RCSfile: s.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:22 $

print "1..62\n";

$x = 'foo';
$_ = "x";
s/x/\$x/;
print "#1\t:$_: eq :\$x:\n";
if ($_ eq '$x') {print "ok 1\n";} else {print "not ok 1\n";}

$_ = "x";
s/x/$x/;
print "#2\t:$_: eq :foo:\n";
if ($_ eq 'foo') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = "x";
s/x/\$x $x/;
print "#3\t:$_: eq :\$x foo:\n";
if ($_ eq '$x foo') {print "ok 3\n";} else {print "not ok 3\n";}

$b = 'cd';
($a = 'abcdef') =~ s<(b${b}e)>'\n$1';
print "#4\t:$1: eq :bcde:\n";
print "#4\t:$a: eq :a\\n\$1f:\n";
if ($1 eq 'bcde' && $a eq 'a\n$1f') {print "ok 4\n";} else {print "not ok 4\n";}

$a = 'abacada';
if (($a =~ s/a/x/g) == 4 && $a eq 'xbxcxdx')
    {print "ok 5\n";} else {print "not ok 5\n";}

if (($a =~ s/a/y/g) == 0 && $a eq 'xbxcxdx')
    {print "ok 6\n";} else {print "not ok 6 $a\n";}

if (($a =~ s/b/y/g) == 1 && $a eq 'xyxcxdx')
    {print "ok 7\n";} else {print "not ok 7 $a\n";}

$_ = 'ABACADA';
if (/a/i && s///gi && $_ eq 'BCD') {print "ok 8\n";} else {print "not ok 8 $_\n";}

$_ = '\\' x 4;
if (length($_) == 4) {print "ok 9\n";} else {print "not ok 9\n";}
s/\\/\\\\/g;
if ($_ eq '\\' x 8) {print "ok 10\n";} else {print "not ok 10 $_\n";}

$_ = '\/' x 4;
if (length($_) == 8) {print "ok 11\n";} else {print "not ok 11\n";}
s/\//\/\//g;
if ($_ eq '\\//' x 4) {print "ok 12\n";} else {print "not ok 12\n";}
if (length($_) == 12) {print "ok 13\n";} else {print "not ok 13\n";}

$_ = 'aaaXXXXbbb';
s/^a//;
print $_ eq 'aaXXXXbbb' ? "ok 14\n" : "not ok 14\n";

$_ = 'aaaXXXXbbb';
s/a//;
print $_ eq 'aaXXXXbbb' ? "ok 15\n" : "not ok 15\n";

$_ = 'aaaXXXXbbb';
s/^a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 16\n" : "not ok 16\n";

$_ = 'aaaXXXXbbb';
s/a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 17\n" : "not ok 17\n";

$_ = 'aaaXXXXbbb';
s/aa//;
print $_ eq 'aXXXXbbb' ? "ok 18\n" : "not ok 18\n";

$_ = 'aaaXXXXbbb';
s/aa/b/;
print $_ eq 'baXXXXbbb' ? "ok 19\n" : "not ok 19\n";

$_ = 'aaaXXXXbbb';
s/b$//;
print $_ eq 'aaaXXXXbb' ? "ok 20\n" : "not ok 20\n";

$_ = 'aaaXXXXbbb';
s/b//;
print $_ eq 'aaaXXXXbb' ? "ok 21\n" : "not ok 21\n";

$_ = 'aaaXXXXbbb';
s/bb//;
print $_ eq 'aaaXXXXb' ? "ok 22\n" : "not ok 22\n";

$_ = 'aaaXXXXbbb';
s/aX/y/;
print $_ eq 'aayXXXbbb' ? "ok 23\n" : "not ok 23\n";

$_ = 'aaaXXXXbbb';
s/Xb/z/;
print $_ eq 'aaaXXXzbb' ? "ok 24\n" : "not ok 24\n";

$_ = 'aaaXXXXbbb';
s/aaX.*Xbb//;
print $_ eq 'ab' ? "ok 25\n" : "not ok 25\n";

$_ = 'aaaXXXXbbb';
s/bb/x/;
print $_ eq 'aaaXXXXxb' ? "ok 26\n" : "not ok 26\n";

# now for some unoptimized versions of the same.

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a//;
print $_ eq 'aaXXXXbbb' ? "ok 27\n" : "not ok 27\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/a//;
print $_ eq 'aaXXXXbbb' ? "ok 28\n" : "not ok 28\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 29\n" : "not ok 29\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 30\n" : "not ok 30\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa//;
print $_ eq 'aXXXXbbb' ? "ok 31\n" : "not ok 31\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa/b/;
print $_ eq 'baXXXXbbb' ? "ok 32\n" : "not ok 32\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/b$//;
print $_ eq 'aaaXXXXbb' ? "ok 33\n" : "not ok 33\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/b//;
print $_ eq 'aaaXXXXbb' ? "ok 34\n" : "not ok 34\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb//;
print $_ eq 'aaaXXXXb' ? "ok 35\n" : "not ok 35\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aX/y/;
print $_ eq 'aayXXXbbb' ? "ok 36\n" : "not ok 36\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/Xb/z/;
print $_ eq 'aaaXXXzbb' ? "ok 37\n" : "not ok 37\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aaX.*Xbb//;
print $_ eq 'ab' ? "ok 38\n" : "not ok 38\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb/x/;
print $_ eq 'aaaXXXXxb' ? "ok 39\n" : "not ok 39\n";

$_ = 'abc123xyz';
s/\d+/$&*2/e;              # yields 'abc246xyz'
print $_ eq 'abc246xyz' ? "ok 40\n" : "not ok 40\n";
s/\d+/sprintf("%5d",$&)/e; # yields 'abc  246xyz'
print $_ eq 'abc  246xyz' ? "ok 41\n" : "not ok 41\n";
s/\w/$& x 2/eg;            # yields 'aabbcc  224466xxyyzz'
print $_ eq 'aabbcc  224466xxyyzz' ? "ok 42\n" : "not ok 42\n";

$_ = "aaaaa";
print y/a/b/ == 5 ? "ok 43\n" : "not ok 43\n";
print y/a/b/ == 0 ? "ok 44\n" : "not ok 44\n";
print y/b// == 5 ? "ok 45\n" : "not ok 45\n";
print y/b/c/s == 5 ? "ok 46\n" : "not ok 46\n";
print y/c// == 1 ? "ok 47\n" : "not ok 47\n";
print y/c//d == 1 ? "ok 48\n" : "not ok 48\n";
print $_ eq "" ? "ok 49\n" : "not ok 49\n";

$_ = "Now is the %#*! time for all good men...";
print (($x=(y/a-zA-Z //cd)) == 7 ? "ok 50\n" : "not ok 50\n");
print y/ / /s == 8 ? "ok 51\n" : "not ok 51\n";

$_ = 'abcdefghijklmnopqrstuvwxyz0123456789';
tr/a-z/A-Z/;

print $_ eq 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789' ? "ok 52\n" : "not ok 52\n";

# same as tr/A-Z/a-z/;
y[\101-\132][\141-\172];

print $_ eq 'abcdefghijklmnopqrstuvwxyz0123456789' ? "ok 53\n" : "not ok 53\n";

$_ = '+,-';
tr/+--/a-c/;
print $_ eq 'abc' ? "ok 54\n" : "not ok 54\n";

$_ = '+,-';
tr/+\--/a\/c/;
print $_ eq 'a,/' ? "ok 55\n" : "not ok 55\n";

$_ = '+,-';
tr/-+,/ab\-/;
print $_ eq 'b-a' ? "ok 56\n" : "not ok 56\n";


# test recursive substitutions
# code based on the recursive expansion of makefile variables

my %MK = (
    AAAAA => '$(B)', B=>'$(C)', C => 'D',			# long->short
    E     => '$(F)', F=>'p $(G) q', G => 'HHHHH',	# short->long
    DIR => '$(UNDEFINEDNAME)/xxx',
);
sub var { 
    my($var,$level) = @_;
    return "\$($var)" unless exists $MK{$var};
    return exp_vars($MK{$var}, $level+1); # can recurse
}
sub exp_vars { 
    my($str,$level) = @_;
    $str =~ s/\$\((\w+)\)/var($1, $level+1)/ge; # can recurse
    #warn "exp_vars $level = '$str'\n";
    $str;
}

print exp_vars('$(AAAAA)',0)           eq 'D'
	? "ok 57\n" : "not ok 57\n";
print exp_vars('$(E)',0)               eq 'p HHHHH q'
	? "ok 58\n" : "not ok 58\n";
print exp_vars('$(DIR)',0)             eq '$(UNDEFINEDNAME)/xxx'
	? "ok 59\n" : "not ok 59\n";
print exp_vars('foo $(DIR)/yyy bar',0) eq 'foo $(UNDEFINEDNAME)/xxx/yyy bar'
	? "ok 60\n" : "not ok 60\n";

# a match nested in the RHS of a substitution:

$_ = "abcd";
s/../$x = $&, m#.#/eg;
print $x eq "cd" ? "ok 61\n" : "not ok 61\n";

# check parsing of split subst with comment
eval 's{foo} # this is a comment, not a delimiter
       {bar};';
print @? ? "not ok 62\n" : "ok 62\n";
