#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve all the hosts entries
#	   following command would repeat the test 100 times
#		./gethostent.pl -repeat 100
#
#	2. Change the timestamp of the /etc/hosts file so that it
#	   will cause the cache to be rebuild.
#		./gethostent.pl -touch -repeat 100 
#
#	3. Add entries in the /etc/hosts file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/hosts#	   file and repeat the test for 100 times
#	 	./gethostent.pl -add a,b,c -repeat 100
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./gethostent.pl -add a,b,c -stopnsd -repeat 100
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./gethostent.pl -wait <time_val_in_seconds> -repeat 100
#

$repeatCount = 1;
$touchflg = 0;
$addflg = 0;
$stopnsdflg = 0;
$waitForSecond = 0;
$prevUser = 0.0;
$prevSystem = 0.0;
$precCuser = 0.0;
$prevCsystem = 0.0;

@options = @ARGV;

while(@ARGV) {
  $_ = shift;
  if(/^-repeat(.*)/) {
	$repeatCount = shift(@ARGV);
  } elsif(/^-touch(.*)/) {
	$touchflg = 1;
  } elsif(/^-add(.*)/) {
	@names_to_add = split(/,/, shift(@ARGV));
	$addflg = 1;
  } elsif (/^-stopnsd(.*)/) {
	$stopnsdflg = 1;
  } elsif(/^-wait(.*)/) {
	$waitForSecond = shift(@ARGV);
  } else {
	print "Unknown options, exiting\n";
	print "Usage: ./gethostent.pl \n\t[-add a,b,c] \n\t[-touch] \n\t[-stopnsd]\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n";
	exit(-1);
  }
  
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:gethostent.pl\n";
print "####CMD: ./gethostent.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================Allhosts() - Begin :", $i,"=============\n";
	if($touchflg ) {
	   #change the timestap of the /etc/hosts file
	   #this will cause the cache to be rebuild
	   system "/sbin/touch /etc/hosts";
	}
	if($addflg ) {
	   system "/sbin/cp /etc/hosts /etc/hosts.old";
	   $ip_addr = "1.1.1.1";

	   #Add the new entries in the /etc/hosts file
	   #this will cause the cache to be rebuild
	   open(ETC_HOSTS, ">> /etc/hosts");
	   @l_names_to_add = @names_to_add;
	   while(<@l_names_to_add>) {
		$hname = shift(@l_names_to_add);
		print ETC_HOSTS $ip_addr, "\t\t$hname", "\n";
	   }
	   close ETC_HOSTS;
	}
	
	#Wait for cache to timeout
	if($waitForSecond) {
	    sleep $waitForSecond;
	}

	if($stopnsdflg) {
	    system "/sbin/killall  -TERM nsd";
  	}
	
	##Call the unit test 
        system "./gethostent";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd option is being used when nsd was running
	if($addflg) {
	   system "/sbin/cp /etc/hosts.old /etc/hosts";
	   unlink("/etc/hosts.old");
	}
	if($stopnsdflg) {
		system "/usr/etc/nsd `cat /etc/config/nsd.options`";
	}
	$luser = $user - $prevUser;
	$lsystem = $system - $prevSystem;
	$lcuser = $cuser - $prevCuser;
	$lcsystem = $csystem - $prevCsystem;	
#        print STDOUT "Time - user = $luser, system = $lsystem, cuser = $lcuser, csystem = $lcsystem\n";
	$prevUser = $user;
	$prevSystem = $system;
	$prevCuser = $cuser;
	$prevCsystem = $csystem;
	print STDOUT "==================Allhosts() - End :", $i,"===============\n";
}

print STDOUT "End Script\n";
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);
