#!/usr/sbin/perl

#This test script test the following scenerios
#	1. Simply retrieve the group entries for given group id
#	   following command would repeat the test 100 times
#		./getgrgid.pl -repeat 100 -gid <comma_separated_list_of_gids>
#
#	2. Change the timestamp of the /etc/group file so that it
#	   will cause the cache to be rebuild.
#		./getgrgid.pl -touch -repeat 100 -gid <comma_separated_list_of_gids>
#
#	3. Add entries in the /etc/group file.This will cause the cache
#	   to be rebuild.Following test will add entries a,b,c in the /etc/group#	   file and repeat the test for 100 times
#	 	./getgrgid.pl -add <comma_separated_list_of_group_names> -repeat 100 -gid <comma_separated_list_of_gids>
#
#	4. Stop the nsd, add new entries and retrive the information.
#		./getgrgid.pl -add <comma_separated_list_of_group_names> -stopnsd -repeat 100 -gid <comma_separated_list_of_gids>
#
#	5. Wait for timeout and then retrieve the information, this will
#	   cause cache to be rebuild
#		./getgrgid.pl -wait <time_val_in_seconds> -repeat 100 -gid <comma_separated_list_of_gids>
#
#	6. Add an entry in the /etc/group file as defined in -entry flag
#		./getgrnam.pl -entry "bogus::66666\nbogus1::6666444444\n" -gid bogus

$repeatCount = 1;
$touchflg = 0;
$addflg = 0;
$stopnsdflg = 0;
$gidFlg = 0;
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
  } elsif (/^-gid(.*)/) {
	@gids_to_list = split(/,/, shift(@ARGV));
	$gidFlg = 1;
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

if(!$gidFlg) {
	&print_usage();
}


open(SAVEOUT, ">&STDOUT");
open(SAVEERR, ">&STDERR");

open(STDERR, ">&STDOUT") || die "Can't dup stdout";

select(STDOUT); $| = 1;       # make unbuffered
select(STDERR); $| = 1;       # make unbuffered

print "Start Script:getgrgid.pl\n";
print "####CMD: ./getgrgid.pl ", join(' ', @options), "\n";

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
#print "Date:/$mon/$mday/$year  ", "Time:$hour:$min:$sec\n";

for($i=0; $i < $repeatCount ; $i++) {
	print STDOUT "==================getgrgid() - Begin :", $i,"=============\n";
	if($touchflg ) {
	   #change the timestap of the /etc/group file
	   #this will cause the cache to be rebuild
	   system "/sbin/touch /etc/group";
	}
	if($addflg ) {
	   system "/sbin/cp /etc/group /etc/group.old";
	   $g_number = 55555;

	   #Add the new entries in the /etc/group file
	   #this will cause the cache to be rebuild
	   open(ETC_GROUP, ">> /etc/group");
	   @l_names_to_add = @names_to_add;
	   while(<@l_names_to_add>) {
		$gname = shift(@l_names_to_add);
		@entry = ($gname, ':', ':', $g_number);
		$g_number++;
		print ETC_GROUP @entry, "\n";
	   }
	   close ETC_GROUP;
	}
	elsif($entryFlg) {
	   	system "/sbin/cp /etc/group /etc/group.old";
	        open(ETC_GROUP, ">> /etc/group");
		print ETC_GROUP $entry_to_add;
	   	close ETC_GROUP;
	}
	
	#Wait for cache to timeout
	if($waitForSecond) {
	    sleep $waitForSecond;
	}

	if($stopnsdflg) {
	    system "/sbin/killall  -TERM nsd";
  	}
	
	##Call the unit test 
	print  "======\n./getgrgid ", join(' ', @gids_to_list), "\n";
        system "./getgrgid @gids_to_list";
	
	print  "======\n./getgrgid_r ", join(' ', @gids_to_list), "\n";
        system "./getgrgid_r @gids_to_list";

	#Get the cpu and system time to execute the test till now
        ($user,$system,$cuser,$csystem) = times;

	#Revert back to the original state, here assumption is that
	#stopnsd oprtion is being used when nsd was running
	if($addflg || $entryFlg) {
	   system "/sbin/cp /etc/group.old /etc/group";
	   unlink("/etc/group.old");
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
	print STDOUT "==================getgrgid() - End :", $i,"===============\n";
}
print STDOUT "End Script\n";
close(STDOUT);
close(STDERR);
open(STDOUT, ">&SAVEOUT");
open(STDERR, ">&SAVEERR");
exit(0);


####### Subs ###############
sub print_usage()
{
	print "Usage: ./getgrgid.pl \n\t[-add <comma_separated_list_of_group_names>] \n\t[-touch] \n\t[-stopnsd]\n\t[-wait <wait_for_seconds>]\n\t[-entry <entry_to_add>]\n\t[-repeat <n_times>]\n\t-gid <comma_separate_list_f_group_gids>\n";
	exit(-1);
}
