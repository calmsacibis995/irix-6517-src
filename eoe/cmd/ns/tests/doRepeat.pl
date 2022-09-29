#!/usr/sbin/perl

$repeatCount = 1;
$waitForSecond = 0;
$cmdFlg = 0;
$prevUser = 0.0;
$prevSystem = 0.0;
$precCuser = 0.0;
$prevCsystem = 0.0;

@options = @ARGV;

while(@ARGV) {
  $_ = shift;
  if(/^-repeat(.*)/) {
        $repeatCount = shift(@ARGV);
  } elsif(/^-wait(.*)/) {
        $waitForSecond = shift(@ARGV);
  } elsif(/^-cmd(.*)/) {
        $cmd = shift(@ARGV);
	$cmdFlg = 1;
  } else {
	&print_usage();
  }

}

if(!$cmdFlg) {
	&print_usage();
}

open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");


open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "####CMD: ./doRepeat.pl ", join(' ', @options),"\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "======== CMD : [$cmd] ======================\n";
	system "$cmd";
};
	print STDOUT "======== CMD : [$cmd] END ======================\n";

close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


#### Subroutines ####
sub print_usage()
{
	print "Usage: ./doRepeat.pl \n\t[-repeat <no_of_times>\n\t[-wait <wait_for_seconds>\n\t-cmd <cmd_to_execute>\n";
	exit(0);
}
