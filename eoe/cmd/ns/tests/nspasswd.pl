#!/usr/sbin/perl

$ranTest = 0;
$repeatCnt = 100;
$waitForTime=35;

if($#ARGV < 0) {
   &print_usage();
}

open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered



#passwd
print "==passwd: files(compat) nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"files(compat) nis\"";
system "./getpwnam @ARGV";
print "==passwd:filesBg(compat)(timeout=2) nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"filesBg(compat)(timeout=2) nis\"";
system "./getpwnam @ARGV";
print "==passwd:filesBg(compat) nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"filesBg(compat) nis\"";
system "./getpwnam @ARGV";
print "==passwd:files(compat) nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"files(compat) nis\"";
system "./getpwnam @ARGV";

close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);

sub print_usage()
{
	print "Usage: ./nspasswd.pl <user_name>";
}
