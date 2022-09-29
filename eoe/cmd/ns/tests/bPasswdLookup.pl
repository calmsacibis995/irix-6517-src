#!/usr/sbin/perl

#       1. To run on ficus to scale  getpwnam
#               ./bPasswdLookup.pl -ficus -o 1 -n <user_name>
#       2. To run on ficus to scale getpwuid
#               ./bPasswdLookup.pl -ficus -o 2 -n <user_id>
#       3. To run on kudzu to scale getpwnam
#               ./bPasswdLookup.pl -kudzu -o 1 -n <user_name>
#       4. To run on kudzu to scale getpwuid
#               ./bPasswdLookup.pl -kudzu -o 2 -n <user_id>

$samplingCnt=8;

#Default pagesize and cachesize

$avgAccessTime=0;
$stopNSD=0;
$i = 0;
$kudzuFlg = 0;
$ficusFlg = 0;
$fn_no = 1;
$pg_name="";
$argCnt=0;
$timeToRun=10;

while(@ARGV) {
  $_ = shift;
  if(/^-kudzu(.*)/) {
	$kudzuFlg = 1;
  } elsif(/^-ficus(.*)/) {
	$ficusFlg = 1;
  } elsif(/^-t(.*)/) {
        $timeToRun = shift(@ARGV);
  } elsif(/^-n(.*)/) {
	@names_to_add = split(/,/, shift(@ARGV));
	while(@names_to_add) {
	   $ent{$argCnt} = shift @names_to_add;
	   $argCnt++;
	}
  } elsif(/^-o(.*)/) {
	$fn_no = shift(@ARGV);
  }else {
	print "Unknown options, exiting\n";
	&print_usage();
  }
  
}

if($kudzuFlg) {
	&main_kudzu();
}
elsif($ficusFlg) {
	&main_ficus();
}
else {
        &print_usage();
}


exit(0);

sub print_usage
{
     print "Usage: ./bPasswdLookup.pl \n\t-kudzu|-ficus \n\t-n <comma_separated_lookup_names> \n";
        exit(-1);

}

sub main_ficus
{
	&ficus_run_benchmark();
}


sub main_kudzu {
	###### Default pagesize and cachesize
	$pagesize=12;
	$cachesize=16;
	$timeOut=300;
	$nsdCacheTimeOut=30;
	&run_benchmark();

	###### Default cachesize , pagesize=32
	$pagesize=32;
	$cachesize=16;
	$timeOut=300;
	$nsdCacheTimeOut=30;
	&run_benchmark();

	###### Default cachesize , pagesize=8
	$pagesize=8;
	$cachesize=16;
	$timeOut=300;
	$nsdCacheTimeOut=30;
	&run_benchmark();

	###### cachesize=64 , pagesize=12
	$pagesize=12;
	$cachesize=64;
	$timeOut=300;
	$nsdCacheTimeOut=30;
	&run_benchmark();

	###### no  cache , pagesize=8
	$pagesize=12;
	$cachesize=0;
	$timeOut=0;
	$nsdCacheTimeOut=0;
	&run_benchmark();


	##### NSD not running #####
	$stopNSD=1;
	&run_benchmark();

}


sub run_benchmark 
{
	if($stopNSD) {
		system "/sbin/killall -TERM nsd";
	}
	else {
		system "/usr/sbin/nsadmin restart -a nis_security=local -a timeout=$timeOut -t $nsdCacheTimeOut";
	}
	sleep(2);
	$i = 0;
	$accessTime = 0;
	for($j=0; $j < $samplingCnt ; $j++) {
		system "./bPasswdLookup -r 1 -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		$i++;
		if($i > $argCnt -1)  { $i = 0; }
		$accessTime += `cat /tmp/bb | grep \"Access Time\" | cut -d: -f2`;
		$pg_name = `cat /tmp/bb | grep \"Access Time\" | cut -d: -f1`;
	}

	$avgAccessTime = $accessTime / $samplingCnt;
	if($stopNSD) {
		print "-NSD is not running\n";
		$stopNSD = 0;
		system "/usr/sbin/nsadmin restart -a nis_security=local -a timeout=300";
	}
	else {
		print "-NSD running with \n\tpagesize=$pagesize\n\tcachesize=$cachesize\n\ttimeout=$timeOut\n\tnsdCacheTimeout=$nsdCacheTimeOut\n";
	}
	chop($pg_name);
	print "-$pg_name:$avgAccessTime microsec\n";
	print "-----------------------------------------------\n\n";

}

sub ficus_run_benchmark
{
	$i = 0;
	$accessTime = 0;
	for($j=0; $j < $samplingCnt ; $j++) {
		system "./bPasswdLookup -r 1 -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		$i++;
		if($i > $argCnt -1)  { $i = 0; }
		$accessTime += `cat /tmp/bb | grep \"Access Time\" | cut -d: -f2`;
		$pg_name = `cat /tmp/bb | grep \"Access Time\" | cut -d: -f1`;
	}
	chop($pg_name);

	$avgAccessTime = $accessTime / $samplingCnt;
	print "-$pg_name:$avgAccessTime microsec\n";
	print "-----------------------------------------------\n\n";
}
