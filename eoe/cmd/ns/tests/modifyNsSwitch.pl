#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Modify the table entry in /etc/nsswitch.conf file 
#		./modifyNsSwitch.pl -table passwd -order "files(compat) [notfound=return] nis" -repeat 10
#       2. Wait for timeout and then retrieve the information, this will
#          cause cache to be rebuild
#               ./modifyNsSwitch.pl -wait <time_val_in_seconds> -repeat 100
#
#	3.Remove a table entry from the /etc/nsswitch.conf
#		./modifyNsSwitch.pl -remove -table <table_name>
#

$repeatCount = 1;
$tableNameFlg = 0;
$waitForSecond = 0;
$libOrderFlg = 0;
$removeFlg = 0;
$prevUser = 0.0;
$prevSystem = 0.0;
$precCuser = 0.0;
$prevCsystem = 0.0;

@options = @ARGV;

while(@ARGV) {
  $_ = shift;
  if(/^-repeat(.*)/) {
	$repeatCount = shift(@ARGV);
  } elsif(/^-table(.*)/) {
	$tableName = shift(@ARGV);
	$tableNameFlg = 1;
  } elsif (/^-order(.*)/) {
	$libOrder = shift(@ARGV);
	$libOrderFlg = 1;
  } elsif(/^-wait(.*)/) {
	$waitForSecond = shift(@ARGV);
  } elsif(/^-remove(.*)/) {
	$removeFlg = 1;
  } else {
	print "Unknown options, exiting\n";
	&print_usage();
  }
  
}

if(!($tableNameFlg  && $libOrderFlg) && !($tableNameFlg && $removeFlg)) {
	&print_usage();
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered


#Wait for cache to timeout
if($waitForSecond) {
    sleep $waitForSecond;
}

system "/sbin/cp /etc/nsswitch.conf /etc/nsswitch.conf.old";
open(INFILE, "< /etc/nsswitch.conf");
open(OUTFILE, "> /etc/nsswitch.conf.new");
while(<INFILE>) {
	if(/^$tableName:(.*)/) {
		if($removeFlg) {
			print OUTFILE "#$_"; 
		}
		else {
			print OUTFILE "$tableName:\t\t\t", $libOrder, "\n"; 
		}
	}
	elsif(/^\#$tableName:(.*)/) {
		print OUTFILE "$tableName:\t\t\t", $libOrder, "\n"; 
  	}
	else {
		print OUTFILE $_;
	}
}
close (INFILE);
close (OUTFILE);
system "/sbin/cp /etc/nsswitch.conf.new /etc/nsswitch.conf";
system "/sbin/killall -HUP nsd";
unlink("/etc/nsswitch.conf.new");
unlink("/etc/nsswitch.conf.old");

close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


######### Subs ############
sub print_usage()
{
	print "Usage: ./modifyNsSwitch.pl \t\n\t-remove\n\t-table <table_name> \n\t-order <lib_order_string>\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n";
	exit(-1);
}
