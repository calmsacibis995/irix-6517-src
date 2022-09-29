#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Retrive the table information based on the given domain
#	   name, library, table name and specifice entry name.If no
#	   entry name is give default ".all" is taken 
#		./genCmd.pl -domain .local -table group.byname -library .nis -name emt -repeat 10
#       2. Wait for timeout and then retrieve the information, this will
#          cause cache to be rebuild
#               ./grnam.pl -wait <time_val_in_seconds> -repeat 100
#
#

$domainNameflg = 0;
$repeatCount = 1;
$libraryNameFlg = 0;
$nameFlg = 0;
$name = ".all";
$tableName = ".files";
$tableNameFlg = 0;
$waitForSecond = 0;
$libOrderFlg = 0;
$prevUser = 0.0;
$prevSystem = 0.0;
$precCuser = 0.0;
$prevCsystem = 0.0;

@options = @ARGV;

while(@ARGV) {
  $_ = shift;
  if(/^-domain(.*)/) {
	$domainName = shift(@ARGV);
  	$domainNameflg = 1;
  } elsif(/^-repeat(.*)/) {
	$repeatCount = shift(@ARGV);
  } elsif(/^-library(.*)/) {
	$libraryName = shift(@ARGV);
	$libraryNameFlg = 1;
  } elsif(/^-table(.*)/) {
	$tableName = shift(@ARGV);
	$tableNameFlg = 1;
  } elsif (/^-name(.*)/) {
	$name = shift(@ARGV);
	$nameFlg = 1;
  } elsif (/^-order(.*)/) {
	@lib_order = split(/,/, shift(@ARGV));
	$libOrderFlg = 1;
  } elsif(/^-wait(.*)/) {
	$waitForSecond = shift(@ARGV);
  } else {
	print "Unknown options, exiting\n";
	print "Usage: ./genCmd.pl \n\t[-domain <domain_name>] \n\t[-table <table_name>] \n\t[-library <library_name>]\n\t[-order <comma_separate_lib_list>\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n";
	exit(-1);
  }
  
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:\n";
print "####CMD: ./genCmd.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================genCmd.pl - Begin :", $i,"=============\n";
	#Wait for cache to timeout
	if($waitForSecond) {
	    system "sleep $waitForSecond";
	}


	##Call the unit test 
	if($libOrderFlg) {
	    @localLibOrder = @lib_order;
	    while(<@localLibOrder>) {
                $libraryName = shift(@localLibOrder);
		print "/sbin/cat /ns/$domainName/$tableName/$libraryName/$name \n";
		system "/sbin/cat /ns/$domainName/$tableName/$libraryName/$name \n";
		&compute_exec_time();
            }

	} else {
	   	print "/sbin/cat /ns/$domainName/$tableName/$libraryName/$name \n"; 
	   	system "/sbin/cat /ns/$domainName/$tableName/$libraryName/$name \n"; 
		&compute_exec_time();
	}

	print STDOUT "==================genCmd - End :", $i,"===============\n";
}
print STDOUT "End Script\n";
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


sub compute_exec_time()
{
	#Get the cpu and system time to execute the test till now
	($user,$system,$cuser,$csystem) = times;
	$luser = $user - $prevUser;
	$lsystem = $system - $prevSystem;
	$lcuser = $cuser - $prevCuser;
	$lcsystem = $csystem - $prevCsystem;	
#	print STDOUT "Time - user = $luser, system = $lsystem, cuser = $lcuser, csystem = $lcsystem\n";
	$prevUser = $user;
	$prevSystem = $system;
	$prevCuser = $cuser;
	$prevCsystem = $csystem;
}
