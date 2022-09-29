#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve all the networks entries
#	   following command would repeat the test 100 times
#		./getnetent.pl -repeat 100
#
#	2. Change the timestamp of the /etc/networks file so that it
#	   will cause the cache to be rebuild.
#		./getnetent.pl -touch -repeat 100 
#
#	3. Add entries in the /etc/networks file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/networks#	   file and repeat the test for 100 times
#	 	./getnetent.pl -add a,b,c -repeat 100
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./getnetent.pl -add <comma_separated_network_name> -stopnsd -repeat 100
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./getnetent.pl -wait <time_val_in_seconds> -repeat 100
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
	print "Usage: ./getnetent.pl \n\t[-add <comma_separated_network_name>] \n\t[-touch] \n\t[-stopnsd]\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n";
	exit(-1);
  }
  
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:getnetent.pl\n";
print "####CMD: ./getnetent.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================Allnetworks() - Begin :", $i,"=============\n";
	if($touchflg ) {
	   #change the timestap of the /etc/networks file
	   #this will cause the cache to be rebuild
	   system "/sbin/touch /etc/networks";
	}
	if($addflg ) {
	   system "/sbin/cp /etc/networks /etc/networks.old";
	   $net_addr = "1.1.1";

	   #Add the new entries in the /etc/networks file
	   #this will cause the cache to be rebuild
	   open(ETC_NETWORKS, ">> /etc/networks");
	   @l_names_to_add = @names_to_add;
	   while(<@l_names_to_add>) {
		$netname = shift(@l_names_to_add);
		print ETC_NETWORKS $netname,"\t\t$net_addr", "\t$netname-alias","\n";
	   }
	   close ETC_NETWORKS;
	}
	
	#Wait for cache to timeout
	if($waitForSecond) {
	    sleep $waitForSecond;
	}

	if($stopnsdflg) {
	    system "/sbin/killall  -TERM nsd";
  	}
	
	##Call the unit test 
        system "./getnetent";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd oprtion is being used when nsd was running
	if($addflg) {
	   system "/sbin/cp /etc/networks.old /etc/networks";
	   unlink("/etc/networks.old");
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
	print STDOUT "==================Allnetworks() - End :", $i,"===============\n";
}
print STDOUT "End Script\n";
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);
