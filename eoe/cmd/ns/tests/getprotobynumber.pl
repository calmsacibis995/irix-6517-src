#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve the protocols entries for a given protocol name
#	   following command would repeat the test 100 times
#		./getprotobynumber.pl -repeat 100 -protonumber <comma_separated_protocol_numberss>
#
#	2. Change the timestamp of the /etc/protocols file so that it
#	   will cause the cache to be rebuild.
#		./getprotobynumber.pl -touch -repeat 100 -protonumber <comma_separated_protocol_numberss>
#
#	3. Add entries in the /etc/protocols file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/protocols#	   file and repeat the test for 100 times
#	 	./getprotobynumber.pl -add <comma_separated_protocol_names> -repeat 100 -protonumber <comma_separated_protocol_numberss>
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./getprotobynumber.pl -add <comma_separated_protocol_names> -stopnsd -repeat 100 -protonumber <comma_separated_protocol_numberss>
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./getprotobynumber.pl -wait <time_val_in_seconds> -repeat 100 -protonumber <comma_separated_protocol_numberss>
#
#	6.Add an arbitrary entry in the /etc/protocols file and look for it
#		./getprotobynumber.pl -entry <a_protocol_entry> -protonumber <protocol_name>

$repeatCount = 1;
$touchflg = 0;
$addflg = 0;
$stopnsdflg = 0;
$protocolnumberFlg = 0;
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
  } elsif (/^-protonumber(.*)/) {
	@list_of_protocolnumbers = split(/,/, shift(@ARGV));
	$protocolnumberFlg = 1;
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

if(!$protocolnumberFlg) {
	&print_usage();
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:getprotobynumber.pl\n";
print "####CMD: ./getprotobynumber.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================getprotobynumber() - Begin :", $i,"=============\n";
	if($touchflg ) {
	   #change the timestap of the /etc/protocols file
	   #this will cause the cache to be rebuild
	   system "/sbin/touch /etc/protocols";
	}
	if($addflg ) {
	   system "/sbin/cp /etc/protocols /etc/protocols.old";
	   $prot_num = 100;

	   #Add the new entries in the /etc/protocols file
	   #this will cause the cache to be rebuild
	   open(ETC_PROTOCOLS, ">> /etc/protocols");
	   @l_names_to_add = @names_to_add;
	   while(<@l_names_to_add>) {
		$pname = shift(@l_names_to_add);
		print ETC_PROTOCOLS $pname, "\t\t$prot_num", "\t$pname-alias", "\n";
	   }
	   close ETC_PROTOCOLS;
	}
	elsif($entryFlg) {
	   system "/sbin/cp /etc/protocols /etc/protocols.old";
	   open(ETC_PROTOCOLS, ">> /etc/protocols");
	   print ETC_PROTOCOLS $entry_to_add;
	   close ETC_PROTOCOLS;
	}
	
	#Wait for cache to timeout
	if($waitForSecond) {
	    sleep $waitForSecond;
	}

	if($stopnsdflg) {
	    system "/sbin/killall  -TERM nsd";
  	}
	
	##Call the unit test 
	print  "./getprotobynumber ", join(' ', @list_of_protocolnumbers), "\n";
        system "./getprotobynumber @list_of_protocolnumbers";

	print  "./getprotobynumber_r ", join(' ', @list_of_protocolnumbers), "\n";
        system "./getprotobynumber_r @list_of_protocolnumbers";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd oprtion is being used when nsd was running
	if($addflg || $entryFlg) {
	   system "/sbin/cp /etc/protocols.old /etc/protocols";
	   unlink("/etc/protocols.old");
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
	print STDOUT "==================getprotobynumber() - End :", $i,"===============\n";
}
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


############ subs ##########
sub print_usage()
{
	print "Usage: ./getprotobynumber.pl \n\t[-add <comma_separated_protocol_names>] \n\t[-touch] \n\t[-stopnsd]\n\t[-entry <entry_to_add>]\n\t[-wait <wait_for_seconds>]\n\t[-repeat <n_times>]\n\t-protonumber <comma_separated_protocol_numberss>\n";
	exit(-1);
}
