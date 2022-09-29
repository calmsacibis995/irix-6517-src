#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve the services entries for a given protocol name
#	   following command would repeat the test 100 times
#		./getservbyname.pl -repeat 100 -servname <comma_separated_services_names>
#
#	2. Change the timestamp of the /etc/services file so that it
#	   will cause the cache to be rebuild.
#		./getservbyname.pl -touch -repeat 100 -servname <comma_separated_services_names>
#
#	3. Add entries in the /etc/services file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/services#	   file and repeat the test for 100 times
#	 	./getservbyname.pl -add <comma_separated_services_names> -repeat 100 -servname <comma_separated_services_names>
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./getservbyname.pl -add <comma_separated_services_names> -stopnsd -repeat 100 -servname <comma_separated_services_names>
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./getservbyname.pl -wait <time_val_in_seconds> -repeat 100 -servname <comma_separated_services_names>
#
#	6.Add an arbitrary entry in the /etc/services file and look for it
#		./getservbyname.pl -entry <a_services_entry> -servname <services_name>

$repeatCount = 1;
$touchflg = 0;
$addflg = 0;
$stopnsdflg = 0;
$servnameFlg = 0;
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
  } elsif (/^-servname(.*)/) {
	@list_of_servnames = split(/,/, shift(@ARGV));
	$servnameFlg = 1;
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

if(!$servnameFlg) {
	&print_usage();
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:getservbyname.pl\n";
print "####CMD: ./getservbyname.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================getservbyname() - Begin :", $i,"=============\n";
	if($touchflg ) {
	   #change the timestap of the /etc/services file
	   #this will cause the cache to be rebuild
	   system "/sbin/touch /etc/services";
	}
	if($addflg ) {
	   system "/sbin/cp /etc/services /etc/services.old";
	   $services_num = 80000;

	   #Add the new entries in the /etc/services file
	   #this will cause the cache to be rebuild
	   open(ETC_SERVICES, ">> /etc/services");
	   @l_names_to_add = @names_to_add;
	   while(<@l_names_to_add>) {
		$sname = shift(@l_names_to_add);
		print ETC_SERVICES $sname, "\t\t$services_num/tcp", "\t$sname-alias", "\n";
		$services_num++;
	   }
	   close ETC_SERVICES;
	}
	elsif($entryFlg) {
	   system "/sbin/cp /etc/services /etc/services.old";
	   open(ETC_SERVICES, ">> /etc/services");
	   print ETC_SERVICES $entry_to_add;
	   close ETC_SERVICES;
	}
	
	#Wait for cache to timeout
	if($waitForSecond) {
	    sleep $waitForSecond;
	}

	if($stopnsdflg) {
	    system "/sbin/killall  -TERM nsd";
  	}
	
	##Call the unit test 
	print  "./getservbyname ", join(' ', @list_of_servnames), "\n";
        system "./getservbyname @list_of_servnames";

	print  "./getservbyname_r ", join(' ', @list_of_servnames), "\n";
        system "./getservbyname_r @list_of_servnames";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd oprtion is being used when nsd was running
	if($addflg || $entryFlg) {
	   system "/sbin/cp /etc/services.old /etc/services";
	   unlink("/etc/services.old");
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
	print STDOUT "==================getservbyname() - End :", $i,"===============\n";
}
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


############ subs ##########
sub print_usage()
{
	print "Usage: ./getservbyname.pl \n\t[-add <comma_separated_services_names>] \n\t[-touch] \n\t[-stopnsd]\n\t[-entry <entry_to_add>]\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n\t-servname <comma_separated_services_names>\n";
	exit(-1);
}
