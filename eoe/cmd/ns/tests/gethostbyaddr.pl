#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve the hosts entries for a given host name
#	   following command would repeat the test 100 times
#		./gethostbyaddr.pl -repeat 100 -hostaddr <comma_separated_host_addrs>
#
#	2. Change the timestamp of the /etc/hosts file so that it
#	   will cause the cache to be rebuild.
#		./gethostbyaddr.pl -touch -repeat 100 -hostaddr <comma_separated_host_addrs>
#
#	3. Add entries in the /etc/hosts file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/hosts#	   file and repeat the test for 100 times
#	 	./gethostbyaddr.pl -add <comma_separated_host_addrs> -repeat 100 -hostaddr <comma_separated_host_addrs>
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./gethostbyaddr.pl -add <comma_separated_host_addrs> -stopnsd -repeat 100 -hostaddr <comma_separated_host_addrs>
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./gethostbyaddr.pl -wait <time_val_in_seconds> -repeat 100 -hostaddr <comma_separated_host_addrs>
#
#	6.Add an arbitrary entry in the /etc/hosts file and look for it
#		./gethostbyaddr.pl -entry <a_host_entry> -hostaddr <host_name>

$repeatCount = 1;
$touchflg = 0;
$addflg = 0;
$stopnsdflg = 0;
$hostaddrFlg = 0;
$entryFlg = 0;
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
  } elsif (/^-hostaddr(.*)/) {
	@list_of_hostnames = split(/,/, shift(@ARGV));
	$hostaddrFlg = 1;
  } elsif (/^-entry(.*)/) {
	$entry_to_add = shift(@ARGV);
	$entryFlg = 1;
  } elsif(/^-wait(.*)/) {
	$waitForSecond = shift(@ARGV);
  } else {
	print "Unknown options, exiting\n";
	&print_usage();
  }
  
}

if(!$hostaddrFlg) {
	&print_usage();
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:gethostbyaddr.pl\n";
print "####CMD: ./gethostbyaddr.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================gethostbyaddr() - Begin :", $i,"=============\n";
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
	if($entryFlg) {
	   open(ETC_HOSTS, ">> /etc/hosts");
	   print ETC_HOSTS $entry_to_add;
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
	print  "./gethostbyaddr ", join(' ', @list_of_hostnames), "\n";
        system "./gethostbyaddr @list_of_hostnames";

	print  "./gethostbyaddr_r ", join(' ', @list_of_hostnames), "\n";
        system "./gethostbyaddr_r @list_of_hostnames";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd oprtion is being used when nsd was running
	if($addflg || $entryFlg) {
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
	print STDOUT "==================gethostbyaddr() - End :", $i,"===============\n";
}
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);



####### Subs ###########
sub print_usage()
{
	print "Usage: ./gethostbyaddr.pl \n\t[-add <comma_separated_host_addrs>] \n\t[-touch] \n\t[-stopnsd]\n\t[-entry <entry_to_add>]\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n\t-hostaddr <comma_separated_host_addrs>\n";
	exit(-1);
}
