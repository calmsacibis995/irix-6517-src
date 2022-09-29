# Tests for VMS::Stdio v2.01
use VMS::Stdio;
import VMS::Stdio qw(&flush &getname &rewind &sync);

print "1..14\n";
print +(defined(&getname) ? '' : 'not '), "ok 1\n";

$name = "test$$";
$name++ while -e "$name.tmp";
$fh = VMS::Stdio::vmsopen("+>$name",'ctx=rec','shr=put','fop=dlt','dna=.tmp');
print +($fh ? '' : 'not '), "ok 2\n";

print +(flush($fh) ? '' : 'not '),"ok 3\n";
print +(sync($fh) ? '' : 'not '),"ok 4\n";

$time = (stat("$name.tmp"))[9];
print +($time ? '' : 'not '), "ok 5\n";

$fh->autoflush;  # Can we autoload autoflush from IO::File?  Do or die.
print "ok 6\n";

print 'not ' unless print $fh scalar(localtime($time)),"\n";
print "ok 7\n";

print +(rewind($fh) ? '' : 'not '),"ok 8\n";

chop($line = <$fh>);
print +($line eq localtime($time) ? '' : 'not '), "ok 9\n";

($gotname) = (getname($fh) =~/\](.*);/);
print +($gotname eq "\U$name.tmp" ? '' : 'not '), "ok 10\n";

$sfh = VMS::Stdio::vmssysopen($name, O_RDONLY, 0,
                              'ctx=rec', 'shr=put', 'dna=.tmp');
print +($sfh ? '' : 'not ($!) '), "ok 11\n";

close($fh);
sysread($sfh,$line,24);
print +($line eq localtime($time) ? '' : 'not '), "ok 12\n";

undef $sfh;
print +(stat("$name.tmp") ? 'not ' : ''),"ok 13\n";

print +(&VMS::Stdio::tmpnam ? '' : 'not '),"ok 14\n";
