#!/usr/sbin/perl

#Script to get the performance numbers for getgrnam(), getgrgid(),
#       getpwnam(), getpwuid(), getrpcbyname(), getrpcbynumber(),
#       getprotobyname(), getprotobynumber()
#       1. To run on ficus to scale  getgrnam, ruinng it 10000 times
#               ./b6_2Mark.pl -ficus -o 1 -t 10000 -f getgrnam -n <grp_name>
#       2. To run on ficus to scale getgrgid, ruinng it 10000 times
#               ./b6_2Mark.pl -ficus -o 2 -t 10000 -f getgrgid -n <grp_id>
#       3. To run on kudzu to scale getgrnam, , ruinng it 10000 times
#               ./b6_2Mark.pl -kudzu -o 1 -t 10000 -f getgrnam -n <grp_name>
#       4. To run on kudzu to scale getgrgid, , ruinng it 10000 times
#               ./b6_2Mark.pl -kudzu -o 2 -f getgrnam -n <grp_id>


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
$funcName="";
$hostFlg=0;
$servFlg=0;

while(@ARGV) {
  $_ = shift;
  if(/^-kudzu(.*)/) {
	$kudzuFlg = 1;
  } elsif(/^-ficus(.*)/) {
	$ficusFlg = 1;
  } elsif(/^-host(.*)/) {
	$hostFlg = 1;
  } elsif(/^-service(.*)/) {
	$servFlg = 1;
  } elsif(/^-t(.*)/) {
	$timeToRun = shift(@ARGV);
  } elsif(/^-f(.*)/) {
	$funcName = shift(@ARGV);
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
     print "Usage: ./b6_2Mark.pl \n\t-kudzu|-ficus \n\t-n <comma_separated_lookup_names> \n";
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
		system "/usr/sbin/nsadmin restart -a nis_security=local -a timeout=$timeOut -t $nsdCacheTimeOut ";
	}
	sleep(4);
	$i = 0;
	$accessTime = 0;
	$adjSamplingCnt = 0;
	for($j=0; $j < $samplingCnt ; $j++) {
		if($hostFlg) {
		    system "./bHostLookup1 -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		elsif($servFlg) {
			system "./bServLookup1 -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		else {
		    system "./bGenLookup -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		$i++;
		if($i > $argCnt -1)  { $i = 0; }
		$tmpaccessTime = 0;
		$tmpaccessTime = `cat /tmp/bb | grep \"Access Time\" | cut -d: -f2`;
		if($tmpaccessTime > 0) {
			$accessTime += $tmpaccessTime;
			$adjSamplingCnt++;
		}
	}

	$avgAccessTime = $accessTime / $adjSamplingCnt;
	if($stopNSD) {
		print "-NSD is not running\n";
		$stopNSD = 0;
		system "/usr/sbin/nsadmin restart -a nis_security=local -a timeout=300 ";
	}
	else {
		print "-NSD running with \n\tpagesize=$pagesize\n\tcachesize=$cachesize\n\ttimeout=$timeOut\n\tnsdCacheTimeout=$nsdCacheTimeOut\n";
	}
	print "-Access Time($funcName):$avgAccessTime sec\n";
	print "-----------------------------------------------\n\n";

}

sub ficus_run_benchmark
{
	$i = 0;
	$accessTime = 0;
	$adjSamplingCnt = 0;
	for($j=0; $j < $samplingCnt ; $j++) {
		if($hostFlg) {
			system "./bHostLookup1 -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		elsif($servFlg) {
			system "./bServLookup1 -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		else {
			system "./bGenLookup -f $funcName -t $timeToRun -o $fn_no -n $ent{$i} > /tmp/bb";
		}
		$i++;
		if($i > $argCnt -1)  { $i = 0; }
		$tmpaccessTime = 0;
		$tmpaccessTime = `cat /tmp/bb | grep \"Access Time\" | cut -d: -f2`;
		if($tmpaccessTime > 0) {
			$accessTime += $tmpaccessTime;
			$adjSamplingCnt++;
		}
	}

	$avgAccessTime = $accessTime / $adjSamplingCnt;
	print "-Access Time($funcName):$avgAccessTime sec\n";
	print "-----------------------------------------------\n\n";
}
