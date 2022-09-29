#!/usr/sbin/perl

$ranTest = 0;
$repeatCnt = 1;
$waitForTime=35;



if($#ARGV < 0) {
   &print_usage();
}

$REF_RESULT_DIR=`pwd`;
chop($REF_RESULT_DIR);
$REF_RESULT_DIR.="/ref/";

$CURR_RESULT_DIR=`pwd`;
chop($CURR_RESULT_DIR);
$CURR_RESULT_DIR.="/result/";

if ($> != 0) {
   die "You need to be root to run as some of the test cases manipulates the system configuration files along with restarting/stopping nsd.\n";
}

print "TESTS REF DIR :$REF_RESULT_DIR\n";
print "TESTS RESULT DIR :$CURR_RESULT_DIR\n";

-e $REF_RESULT_DIR || die "Can not continue , ref dirctory does not exist"; 

#Create the result directory if does not exists
if( ! -e $CURR_RESULT_DIR ) {
	mkdir($CURR_RESULT_DIR,0777);
}



while(@ARGV) {
	$_ = shift;
	if($_ eq "-init") {
		$CURR_RESULT_DIR = $REF_RESULT_DIR;
		$ranTest = 1;
	}
	if($_ eq "-nslist") {
		&ns_list_test();
		$ranTest = 1;
	}
	if($_ eq "-getgrent") {
		&getgrent_test();
		$ranTest = 1;
	} 
    if($_ eq "-getgrnam") {
		&getgrnam_test();
		$ranTest = 1;
	} 
	if($_ eq "-getgrgid") {
		&getgrgid_test();
		$ranTest = 1;
	} 
	if($_ eq "-gethostent") {
		&gethostent_test();
		$ranTest = 1;
	} 
	if($_ eq "-gethostbyname") {
		&gethostbyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-gethostbyaddr") {
		&gethostbyaddr_test();
		$ranTest = 1;
	}
	if($_ eq "-modifynsswitch") {
		&modifynsswitch_test();
		$ranTest = 1;
	}
	if($_ eq "-getnetent") {
		&getnetent_test();
		$ranTest = 1;
	} 
	if($_ eq "-getnetbyname") {
		&getnetbyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-getnetbyaddr") {
		&getnetbyaddr_test();
		$ranTest = 1;
	} 
	if($_ eq "-getprotoent") {
		&getprotoent_test();
		$ranTest = 1;
	} 
	if($_ eq "-getprotobyname") {
		&getprotobyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-getprotobynumber") {
		&getprotobynumber_test();
		$ranTest = 1;
	} 
	if($_ eq "-getpwent") {
		&getpwent_test();
		$ranTest = 1;
	} 
	if($_ eq "-getpwbyname") {
		&getpwbyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-getpwbyuid") {
		&getpwbyuid_test();
		$ranTest = 1;
	} 
	if($_ eq "-getrpcent") {
		&getrpcent_test();
		$ranTest = 1;
	} 
	if($_ eq "-getrpcbyname") {
		&getrpcbyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-getrpcbynumber") {
		&getrpcbynumber_test();
		$ranTest = 1;
	} 
	if($_ eq "-getservent") {
		&getservent_test();
		$ranTest = 1;
	} 
	if($_ eq "-getservbyname") {
		&getservbyname_test();
		$ranTest = 1;
	} 
	if($_ eq "-getservbyport") {
		&getservbyport_test();
		$ranTest = 1;
	} 
	if($_ eq "-multithread") {
		&multithread_test();
		$ranTest = 1;
	} 
	if($_ eq "-all") {
		&ns_list_test();
		&getgrent_test();
		&getgrnam_test();
		&getgrgid_test();
		&gethostent_test();
		&gethostbyname_test();
		&gethostbyaddr_test();
		&getnetent_test();
		&getnetbyname_test();
		&getnetbyaddr_test();
		&getprotoent_test();
		&getprotobyname_test();
		&getprotobynumber_test();
		&getpwent_test();
		&getpwbyname_test();
		&getpwbyuid_test();
		&getrpcent_test();
		&getrpcbyname_test();
		&getrpcbynumber_test();
		&getservent_test();
		&getservbyname_test();
		&getservbyport_test();
		&multithread_test();
		&modifynsswitch_test();
		$ranTest = 1;
	}
    if(!$ranTest) { 
    	&print_usage();
    }
}


sub print_usage()
{
   print "Usage :./regTestSuite.pl \n\t-nslist\n\t-getgrent\n\t-getgrnam\n\t-getgrgid\n\t-gethostent\n\t-gethostbyname\n\t-gethostbyaddr\n\t-getnetent\n\t-getnetbyname\n\t-getnetbyaddr\n\t-modifynsswitch\n\t-getptotoent\n\t-getprotobyname\n\t-getprotobynumber\n\t-getpwent\n\t-getpwbyname\n\t-getpwbyuid\n\t-getrpcent\n\t-getrpcbyname\n\t-getrpcbynumber\n\t-getservent\n\t-getservbyname\n\t-getservbyport\n\t-multithread\n\t-all\n";
   exit(0);
		   
}

sub do_diff()
{
	local($file1, $file2, $test_name)= @_;
    	#Do a diff on the result and ref files and conclude the status
    	$ret_code = system "/usr/bin/diff $file1 $file2";
    	if($ret_code != 0) {
		print "/usr/bin/diff $file1 $file2";
        	print "FAILURE - $test_name() failed\n";
    	}

	#check for any NSD crash
	$hostname=`hostname`;
        $dir=`pwd`;

	if ( -e "/var/ns/core") {
		#NSD crashed , send mail to bob and John
		system "echo \"NSD core dumped\n$hostname:/var/ns\" /tmp/nss";
		system "Mail -s \"nsd crashed\" mende\@engr.sgi.com < /tmp/nss";
		#stop all the tests
		print SAVOUT "NSD crashed, core in /var/ns.Sent mail to mende\@engr.sgi.com\n";	
		exit(-1);
	}

	#check for any local core file
	if(-e "./core") {
		#Some query routine crashed
		system "echo \"getXbyY() or getXent() dumped core\n$hostname:$dir\" > /tmp/nss";
		system "Mail -s \"getXbyY() or getXent() crashed\" mende\@engr.sgi.com < /tmp/nss";
		#mv the core file
		system "/sbin/mv core $test_name.core"; 
		print SAVOUT  "$test_name dumped core, core have been moved to $test_name.core.A mail has been sent to mende\@engr.sgi.com\n";
	}


}

####### getgrent() #######################
sub getgrent_test()
{
    $res_file = $CURR_RESULT_DIR."allgroups.res";
    $ref_file = $REF_RESULT_DIR."allgroups.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./allgroups.pl ";
    system "./allgroups.pl -touch -repeat $repeatCnt " ;
    system "./allgroups.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt ";
    system "./allgroups.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./allgroups.pl -wait $waitForTime  ";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");

    &do_diff($ref_file, $res_file, "getgrent_test");
}

####### getgrnam() and getgrnam_r() ########
sub getgrnam_test()
{
    $res_file = $CURR_RESULT_DIR."getgrnam.res";
    $ref_file = $REF_RESULT_DIR."getgrnam.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getgrnam.pl -repeat $repeatCnt -name emt,uucp,sys,bogus,bin,invalid " ;
    system "./getgrnam.pl -touch -repeat $repeatCnt -name emt,uucp,sys,bogus,bin,invalid " ;
    system "./getgrnam.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -name bogus,bin,sys -repeat $repeatCnt ";
    system "./getgrnam.pl -add a,b,c -stopnsd  -name sys,bogus,uucp,user -repeat $repeatCnt ";
    system "./getgrnam.pl -wait $waitForTime -name emt,uucp,sys,bogus,bin,invalid -repeat $repeatCnt ";
    system "./getgrnam.pl -entry \"bogus::666666666666\" -name bogus -repeat $repeatCnt ";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getgrnam a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\" ";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getgrnam_test");
}

####### getgrgid() and getgrgid_r() ########
sub getgrgid_test()
{
    $res_file = $CURR_RESULT_DIR."getgrgid.res";
    $ref_file = $REF_RESULT_DIR."getgrgid.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getgrgid.pl -gid 10,100,10,20000000,10 -repeat $repeatCnt " ;
    system "./getgrgid.pl -touch -gid 10,100,10,20000000,10 -repeat $repeatCnt " ;
    system "./getgrgid.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -name 10000000,20,200  -repeat $repeatCnt ";
    system "./getgrgid.pl -add a,b,c -stopnsd  -gid 10,10000000,0,20  -repeat $repeatCnt ";
    system "./getgrgid.pl -wait $waitForTime -gid 10,10,2000000,0,0,10000000 -repeat $repeatCnt ";
    system "./getgrgid.pl -wait $waitForTime -gid xxx,-10,%%%%,0,0,100 -repeat $repeatCnt ";
    system "./getgrgid.pl -gid xxx,-10,%%%%,0,0,100 -repeat $repeatCnt ";
    system "./getgrgid.pl -entry \"bogus::666666666666\" -gid 666666666666 -repeat $repeatCnt ";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getgrgid a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\" ";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getgrgid_test");
}


######Modify entry in /etc/nsswitch.conf file ##############
sub modifynsswitch_test()
{
    $res_file = $CURR_RESULT_DIR."modifynsswitch.res";
    $ref_file = $REF_RESULT_DIR."modifynsswitch.res";

	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
   	system "./getgrmember.pl emt ";
	#change the hosts table entry in /etc/nsswitch
	#case 1
	print "hosts:          \n ";
	system "./modifyNsSwitch.pl -table hosts -order \" \" ";
	system "./gethostent.pl  " ;
	system "./gethostbyname.pl -hostname ankit,relay,yahoo " ;
	system "./gethostbyaddr.pl -hostaddr 150.166.75.52 " ;
	#case 2
	print "hosts:          nis1 dns1 files1\n ";
	system "./modifyNsSwitch.pl -table hosts -order \"nis1 dns1 files1\" ";
	system "./gethostent.pl  " ;
	system "./gethostbyname.pl -hostname ankit,relay,yahoo " ;
	system "./gethostbyaddr.pl -hostaddr 150.166.75.52 " ;

	#Revert back
	print "hosts:          nis dns files\n ";
	system "./modifyNsSwitch.pl -table hosts -order \"nis dns files\" ";

	
	#change the group table entry in /etc/nsswitch
	#case 1
	print "group:          \n ";
	system "./modifyNsSwitch.pl -table group -order \" \" ";
	system "./getgrent  " ;
	system "./getgrnam.pl -name sys,bogus,emt " ;
	system "./getgrgid.pl -gid 150,0,10 ";
	#case 2
	print "group:          files1 nis1\n ";
	system "./modifyNsSwitch.pl -table group -order \"files1 nis1\" ";
	system "./getgrent  " ;
	system "./getgrnam.pl -name sys,bogus,emt " ;
	system "./getgrgid.pl -gid 150,0,10 ";

	#Revert back
	print "group:          files  nis\n ";
	system "./modifyNsSwitch.pl -table group -order \"files [notfound=return] nis\" ";

	#passwd
	system "./nspasswd.pl  surendra emt sys ";

	#files attributes - using host map
	system "./nsfilesattr.pl  emt1 ankit sys >> $res_file";

	#nis attributes - using host map
	system "./nsnisattr.pl java.sun.com emt1 ankit neteng >> $res_file";	

	#dns attributes - using host map
	system "./nsdnsattr.pl java.sun.com www.yahoo.com emt1 ankit >> $res_file";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");

	 &do_diff($ref_file, $res_file, "modifynsswitch_test");
}


#######gethostent() ######################
sub gethostent_test()
{
    $res_file = $CURR_RESULT_DIR."gethostent.res";
    $ref_file = $REF_RESULT_DIR."gethostent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");

#	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allhosts\" ";
    system "./gethostent.pl -repeat $repeatCnt ";
    system "./gethostent.pl -touch -repeat $repeatCnt " ;
    system "./gethostent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt ";
    system "./gethostent.pl -add a,b,c -stopnsd -repeat $repeatCnt ";
    system "./gethostent.pl -wait $waitForTime  -repeat $repeatCnt ";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "gethostent_test");
}

#######gethostbyname() ######################
sub gethostbyname_test()
{
    $res_file = $CURR_RESULT_DIR."gethostbyname.res";
    $ref_file = $REF_RESULT_DIR."gethostbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./gethostbyname.pl -hostname ankit,relay,yahoo -repeat $repeatCnt " ;
    system "./gethostbyname.pl -touch -hostname ankit,relay,yahoo -repeat $repeatCnt " ;
    system "./gethostbyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -hostname a,bogus,relay,ankit  -repeat $repeatCnt ";
    system "./gethostbyname.pl -add a,b,c -stopnsd  -hostname relay,ankit,bogus -repeat $repeatCnt ";
    system "./gethostbyname.pl -wait $waitForTime -hostname relay,ankit,bogus1  -repeat $repeatCnt ";
    system "./gethostbyname.pl -entry \"1.1.1.1   bogus.vgi.com bogus1 boggy\" -hostname bogus1,boggy -repeat $repeatCnt ";
	#change the hosts table entry in /etc/nsswitch
	print "hosts:          files [notfound=return] nis\n";
	system "./modifyNsSwitch.pl -table hosts -order \"files [notfound=return] nis\"";
	system "./gethostbyname.pl -hostname ankit,relay,yahoo" ;
	#Revert back
	print "hosts:          nis dns files\n";
	system "./modifyNsSwitch.pl -table hosts -order \"nis dns files\"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./gethostbyname a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "gethostbyname_test");
}

#######gethostbyaddr() ######################
sub gethostbyaddr_test()
{
    $res_file = $CURR_RESULT_DIR."gethostbyaddr.res";
    $ref_file = $REF_RESULT_DIR."gethostbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./gethostbyaddr.pl -hostaddr 150.166.75.52 -repeat $repeatCnt" ;
    system "./gethostbyaddr.pl -touch -hostaddr 150.166.75.52,bogus -repeat $repeatCnt" ;
    #system "./gethostbyaddr.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -hostaddr a,150.166.75.52,1.1.1.1 -repeat $repeatCnt";
    #system "./gethostbyaddr.pl -add a,b,c -stopnsd  -hostaddr 150.166.75.52,bg,150.166.75.52 -repeat $repeatCnt";
    system "./gethostbyaddr.pl -wait $waitForTime -hostname relay,ankit,bogus1  -repeat $repeatCnt";
    #system "./gethostbyaddr.pl -entry \"1.1.1.1   bogus.vgi.com bogus1 boggy\" -hostname bogus1,boggy -repeat $repeatCnt";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./gethostbyaddr a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "gethostbyaddr_test");
}

sub getnetent_test()
{
	$res_file = $CURR_RESULT_DIR."getnetent.res";
    $ref_file = $REF_RESULT_DIR."getnetent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allnets\"";
    system "./getnetent.pl -repeat $repeatCnt";
    system "./getnetent.pl -touch -repeat $repeatCnt" ;
    system "./getnetent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt";
    system "./getnetent.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./getnetent.pl -wait $waitForTime  -repeat $repeatCnt";
    system "././modifyNsSwitch.pl -table networks -order \"nis files \"";
    system "./getnetent.pl";
    system "./modifyNsSwitch.pl -table networks -order \"files nis \"";
    system "./getnetent.pl";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getnetent_test");
}

sub getnetbyname_test()
{
	$res_file = $CURR_RESULT_DIR."getnetbyname.res";
    $ref_file = $REF_RESULT_DIR."getnetbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getnetbyname.pl -networkname loopback,arpa -repeat $repeatCnt" ;
    system "./getnetbyname.pl -touch -networkname loopback,arpa,bogus -repeat $repeatCnt" ;
    system "./getnetbyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -networkname a,bogus,arpa -repeat $repeatCnt";
    system "./getnetbyname.pl -add a,b,c -stopnsd  -networkname a,bogus,arpa -repeat $repeatCnt";
    system "./getnetbyname.pl -wait $waitForTime -networkname %%%%,XXXX,11111,loopback,bogus1  -repeat $repeatCnt";
    system "./getnetbyname.pl -entry \"%%%%%%.1.1.1   bogus1 boggy\" -networkname bogus1,boggy -repeat $repeatCnt";
	#change the networks table entry in /etc/nsswitch
	print "networks:          nis files \n";
	system "./modifyNsSwitch.pl -table networks -order \"nis files\"";
	system "./getnetbyname.pl -networkname loopback,arpa,bogus" ;
	#Revert back
	print "networks:          files nis \n";
	system "./modifyNsSwitch.pl -table networks -order \"files nis\"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getnetbyname a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table networks -order \"nis\"";
    system "./getnetbyname.pl -networkname sgicust";
    print "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getnetbyname.pl -networkname sgicust,arpa";
    print "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getnetbyname.pl -networkname sgicust,arpa";
    system "./modifyNsSwitch.pl -table networks -order \"files nis\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getnetbyname_test");
}

sub getnetbyaddr_test()
{
	$res_file = $CURR_RESULT_DIR."getnetbyaddr.res";
    $ref_file = $REF_RESULT_DIR."getnetbyaddr.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getnetbyaddr.pl -networkaddr 11.51.0.0,10.0.0.0 -repeat $repeatCnt" ;
    system "./getnetbyaddr.pl -touch -networkaddr 11.51.0.0,10.0.0.0,1.1.1.1,xxxx -repeat $repeatCnt" ;
    system "./getnetbyaddr.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -networkaddr 11.51.0.0,10.0.0.0,1.1.1.1,a,arpa -repeat $repeatCnt";
    system "./getnetbyaddr.pl -add a,b,c -stopnsd  -networkaddr 11.51.0.0,10.0.0.0,1.1.1.1,bogus,arpa -repeat $repeatCnt";
    system "./getnetbyaddr.pl -wait $waitForTime -networkaddr %.%.%.%,XXXX,11111,loopback,11.51.0.0  -repeat $repeatCnt";
    system "./getnetbyaddr.pl -entry \"bogus %%%%%%.1.1.1  bogus1 boggy\" -networkaddr %%%%%%.1.1.1,boggy -repeat $repeatCnt";
	#change the networks table entry in /etc/nsswitch
	print "networks:          files nis\n";
	system "./modifyNsSwitch.pl -table networks -order \"nis files\"";
	system "./getnetbyaddr.pl -networkaddr 11.51.0.0,10.0.0.0,bogus" ;
	#Revert back
	print "networks:          files nis \n";
	system "./modifyNsSwitch.pl -table networks -order \"files nis\"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getnetbyaddr a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getnetbyaddr_test");
}

sub getprotoent_test()
{
	$res_file = $CURR_RESULT_DIR."getprotoent.res";
    $ref_file = $REF_RESULT_DIR."getprotoent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getprotoent.pl -repeat $repeatCnt";
    system "./getprotoent.pl -touch -repeat $repeatCnt" ;
    system "./getprotoent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt";
    system "./getprotoent.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./getprotoent.pl -wait $waitForTime  -repeat $repeatCnt";
    system "././modifyNsSwitch.pl -table networks -order \"nis files \"";
    system "./getprotoent.pl";
    system "./modifyNsSwitch.pl -table networks -order \"files nis \"";
    system "./getprotoent.pl";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getprotoent_test");
}

sub getprotobyname_test()
{
	$res_file = $CURR_RESULT_DIR."getprotobyname.res";
    $ref_file = $REF_RESULT_DIR."getprotobyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getprotobyname.pl -protoname ip,udp,icmp,xx,%%%%,ip -repeat $repeatCnt" ;
    system "./getprotobyname.pl -touch -protoname ip,udp,icmp,xx,%%%%,ip -repeat $repeatCnt" ;
    system "./getprotobyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -protoname ip,udp,icmp,xx,%%%%,ip -repeat $repeatCnt";
    system "./getprotobyname.pl -add a,b,c -stopnsd  -protoname ip,udp,icmp,xx,%%%%,ip -repeat $repeatCnt";
    system "./getprotobyname.pl -wait $waitForTime -protoname %%%%,XXXX,11111,ip,udp,icmp,bogus1  -repeat $repeatCnt";
    system "./getprotobyname.pl -entry \"%%%%%%.1.1.1   7777 boggy\" -protoname %%%%%%.1.1.1,boggy -repeat $repeatCnt";
	#change the protocols table entry in /etc/nsswitch
	print "protocols:          files nis\n";
	system "./modifyNsSwitch.pl -table protocols -order \"files nis\"";
	system "./getprotobyname.pl -protoname ip,udp,icmp,xx,%%%%,ip" ;
	#Revert back
	print "protocols:          nis[success=return] files \n";
	system "./modifyNsSwitch.pl -table protocols -order \"nis[success=return] files\"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getprotobyname a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table protocols -order \"nis\"";
    system "./getprotobyname.pl -protoname ip,udp";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getprotobyname.pl -protoname ip,udp";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getprotobyname.pl -protoname ip,udp";
    system "./modifyNsSwitch.pl -table protocols -order \"nis[success=return] files\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getprotobyname_test");
}

sub getprotobynumber_test()
{
	$res_file = $CURR_RESULT_DIR."getprotobynumber.res";
    $ref_file = $REF_RESULT_DIR."getprotobynumber.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getprotobynumber.pl -protonumber 0,1,6,xx,%%%%,58 -repeat $repeatCnt" ;
    system "./getprotobynumber.pl -touch -protonumber 1,0,1,10,4,12,58,77 -repeat $repeatCnt" ;
    system "./getprotobynumber.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -protonumber 1,0,1,10,4,12,58,77 -repeat $repeatCnt";
    system "./getprotobynumber.pl -add a,b,c -stopnsd  -protonumber 1,0,1,10,4,12,58,77 -repeat $repeatCnt";
    system "./getprotobynumber.pl -wait $waitForTime -protonumber 1,0,1,10,4,12,58,77 -repeat $repeatCnt";
    system "./getprotobynumber.pl -entry \"%%%%%%.1.1.1   7777 boggy\" -protonumber 1,0,1,10,4,12,58,77 -repeat $repeatCnt";
	#change the protocols table entry in /etc/nsswitch
	print "protocols:          files nis\n";
	system "./modifyNsSwitch.pl -table protocols -order \"files nis\"";
	system "./getprotobynumber.pl -protonumber 1,0,1,10,4,12,58,77" ;
	#Revert back
	print "protocols:          nis[success=return] files \n";
	system "./modifyNsSwitch.pl -table protocols -order \"nis[success=return] files\"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getprotobynumber 1 0 1 10 4 12 58 77 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table protocols -order \"nis\"";
    system "./getprotobynumber.pl -protonumber 1,0,1,10,4,12,58,77";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getprotobynumber.pl -protonumber 1,0,1,10,4,12,58,77";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getprotobynumber.pl -protonumber 1,0,1,10,4,12,58,77";
    system "./modifyNsSwitch.pl -table protocols -order \"nis[success=return] files\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getprotobynumber_test");
}

sub getpwent_test()
{
	$res_file = $CURR_RESULT_DIR."getpwent.res";
    $ref_file = $REF_RESULT_DIR."getpwent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getpwent.pl -repeat $repeatCnt";
    system "./getpwent.pl -touch -repeat $repeatCnt" ;
    system "./getpwent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt";
    system "./getpwent.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./getpwent.pl -wait $waitForTime  -repeat $repeatCnt";
    system "././modifyNsSwitch.pl -table passwd -order \"nis files \"";
    system "./getpwent.pl";
    system "./modifyNsSwitch.pl -table networks -order \"files  nis \"";
    system "./getpwent.pl";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getpwent_test");
}

sub getpwbyname_test()
{
	$res_file = $CURR_RESULT_DIR."getpwbyname.res";
    $ref_file = $REF_RESULT_DIR."getpwbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getpwbyname.pl -username root,sysadm,nobody,xx,%%%%,bg -repeat $repeatCnt" ;
    system "./getpwbyname.pl -touch -username root,sysadm,nobody,xx,%%%%,bg -repeat $repeatCnt" ;
    system "./getpwbyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -username root,sysadm,nobody,xx,%%%%,bg -repeat $repeatCnt";
    system "./getpwbyname.pl -add a,b,c -stopnsd  -username root,sysadm,nobody,xx,%%%%,bg -repeat $repeatCnt";
    system "./getpwbyname.pl -wait $waitForTime -username root,sysadm,nobody,xx,%%%%,bg,bogus1  -repeat $repeatCnt";
    system "./getpwbyname.pl -entry \"%%%%%%.1.1.1::55555:10:Junk entry:/usr/junk:/bin/tcsh\" -username %%%%%%.1.1.1,boggy -repeat $repeatCnt";
	#change the passwd table entry in /etc/nsswitch
	print "passwd:          files nis\n";
	system "./modifyNsSwitch.pl -table passwd -order \"files nis\"";
	system "./getpwbyname.pl -username root,sysadm,nobody,xx,%%%%,bg" ;
	#Revert back
	print "passwd:          nis[success=return] files \n";
	system "./modifyNsSwitch.pl -table passwd -order \"files(compat) [notfound=return] nis \"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getpwnam a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table passwd -order \"nis\"";
    system "./getppwyname.pl -username emt,root";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getpwbyname.pl -username emt,root";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getpwbyname.pl -username emt,root";
    system "./modifyNsSwitch.pl -table passwd -order \"files(compat) [notfound=return] nis\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getpwbyname_test");
}

sub getpwbyuid_test()
{
	$res_file = $CURR_RESULT_DIR."getpwbyuid.res";
    $ref_file = $REF_RESULT_DIR."getpwbyuid.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getpwbyuid.pl -userid 0,9,100,12000,bg,$$$$,%%%% -repeat $repeatCnt" ;
    system "./getpwbyuid.pl -touch -userid 0,9,100,12000,bg,$$$$,%%%% -repeat $repeatCnt" ;
    system "./getpwbyuid.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -userid 0,9,100,12000,bg,$$$$,%%%%,60000 -repeat $repeatCnt";
    system "./getpwbyuid.pl -add a,b,c -stopnsd  -userid 0,9,100,12000,bg,$$$$,%%%%,60000 -repeat $repeatCnt";
    system "./getpwbyuid.pl -wait $waitForTime -userid 0,9,100,12000,bg,$$$$,%%%%,60000 -repeat $repeatCnt";
    system "./getpwbyuid.pl -entry \"%%%%%%.1.1.1::55555:10:Junk entry:/usr/junk:/bin/tcsh\" -userid 0,9,100,12000,bg,$$$$,%%%%,60000,55555 -repeat $repeatCnt";
	#change the passwd table entry in /etc/nsswitch
	print "passwd:          files nis\n";
	system "./modifyNsSwitch.pl -table passwd -order \"files nis\"";
	system "./getpwbyuid.pl -userid 0,9,100,12000,bg,$$$$" ;
	#Revert back
	print "passwd:          nis[success=return] files \n";
	system "./modifyNsSwitch.pl -table passwd -order \"files(compat) [notfound=return] nis \"";

    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getpwuid 10 0 1222222222 88 8888888  2838828323828382832838283 2382838238283 283823283 823 283 23828323  283 28328382 823 283 0 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table passwd -order \"nis\"";
    system "./getppwyuid.pl -userid 0,9,100,12000,bg,$$$$";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getpwbyuid.pl -userid 0,9,100,12000,bg,$$$$";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getpwbyuid.pl -userid 0,9,100,12000,bg,$$$$";
    system "./modifyNsSwitch.pl -table passwd -order \"files(compat) [notfound=return] nis\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getpwbyuid_test");
}

sub getrpcent_test()
{
	$res_file = $CURR_RESULT_DIR."getrpcent.res";
    $ref_file = $REF_RESULT_DIR."getrpcent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getrpcent.pl -repeat $repeatCnt";
    system "./getrpcent.pl -touch -repeat $repeatCnt" ;
    system "./getrpcent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt";
    system "./getrpcent.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./getrpcent.pl -wait $waitForTime  -repeat $repeatCnt";
    system "././modifyNsSwitch.pl -table rpc -order \"nis[success=return] files \"";
    system "./getrpcent.pl";
    system "./modifyNsSwitch.pl -table rpc -order \"files nis \"";
    system "./getrpcent.pl";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getrpcent_test");
}

sub getrpcbyname_test()
{
	$res_file = $CURR_RESULT_DIR."getrpcbyname.res";
    $ref_file = $REF_RESULT_DIR."getrpcbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getrpcbyname.pl -rpcname mountd,rstatd,yppasswdd,%%%%,mountd -repeat $repeatCnt" ;
    system "./getrpcbyname.pl -touch -rpcname mountd,rstatd,yppasswdd,%%%%,mountd -repeat $repeatCnt" ;
    system "./getrpcbyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -rpcname mountd,rstatd,%%%%,mountd,a,h  -repeat $repeatCnt";
    system "./getrpcbyname.pl -add a,b,c -stopnsd  -rpcname a,b,c,mountd,rstatd -repeat $repeatCnt";
    system "./getrpcbyname.pl -wait $waitForTime -rpcname mountd,rstatd,yppasswdd,bogus1  -repeat $repeatCnt";
    system "./getrpcbyname.pl -entry \"aaa 20000 aaa-alias aaa1 aaa2 aaa3 aaa4\naaaa1 40000 aap\nbbbb\ncccc 60000\ndddd eeeee\nnew-entry 70000\" -rpcname aaa,aaaa1,cccc,dddd,eeeee -repeat $repeatCnt";
	#change the rpc table entry in /etc/nsswitch
	print "rpc:          nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"nis\"";
	system "./getrpcbyname.pl -rpcname mountd,rstatd,yppasswdd,%%%%,mountd" ;
	#Revert back
	print "rpc:          files nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"files nis\"";
	system "./getrpcbyname.pl -rpcname mountd,rstatd,yppasswdd,%%%%,mountd" ;
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getrpcbyname 1 0 1 10 4 12 58 77 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table rpc -order \"nis\"";
    system "./getrpcbyname.pl -rpcname portmap,portmapper,quorumd";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getrpcbyname.pl -rpcname portmap,portmapper,quorumd";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getrpcbyname.pl -rpcname portmap,portmapper,quorumd";
    system "./modifyNsSwitch.pl -table rpc -order \"files nis \"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getrpcbyname_test");
}

sub getrpcbynumber_test()
{
	$res_file = $CURR_RESULT_DIR."getrpcbynumber.res";
    $ref_file = $REF_RESULT_DIR."getrpcbynumber.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getrpcbynumber.pl -rpcnumber 391000,100010,10008,%%%% -repeat $repeatCnt" ;
    system "./getrpcbynumber.pl -touch -rpcnumber 391000,100010,10008,%%%% -repeat $repeatCnt" ;
    system "./getrpcbynumber.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -rpcnumber 391000,100010,10008,%%%%,80000 -repeat $repeatCnt";
    system "./getrpcbynumber.pl -add a,b,c -stopnsd  -rpcnumber 391000,100010,10008,%%%%,80000,80001 -repeat $repeatCnt";
    system "./getrpcbynumber.pl -wait $waitForTime -rpcnumber 391000,100010,10008,%%%%,80000,80001 -repeat $repeatCnt";
    system "./getrpcbynumber.pl -entry \"aaa 20000 aaa-alias aaa1 aaa2 aaa3 aaa4\naaaa1 40000 aap\nbbbb\ncccc 60000\ndddd eeeee\nnew-entry 70000\" -rpcnumber 20000,60000,40000 -repeat $repeatCnt";
	#change the rpc table entry in /etc/nsswitch
	print "rpc:          nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"nis\"";
	system "./getrpcbynumber.pl -rpcnumber 391101" ;
	print "rpc:      files [notfound=return]    nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"files [notfound=return] nis\"";
	system "./getrpcbynumber.pl -rpcnumber 391101" ;
	print "rpc:      files [success=return]  nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"files [success=return] nis\"";
	system "./getrpcbynumber.pl -rpcnumber 391101" ;
	#Revert back
	print "rpc:          files nis\n";
	system "./modifyNsSwitch.pl -table rpc -order \"files nis\"";
	system "./getrpcbynumber.pl -rpcnumber 391000,100010,10008,%%%%" ;
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getrpcbynumber 391000 100010 10008 1 0 1 10 4 12 58 77 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    system "./modifyNsSwitch.pl -table rpc -order \"nis\"";
    system "./getrpcbynumber.pl -rpcnumber 391000,100010,10008,%%%%,391101";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getrpcbynumber.pl -rpcnumber 391000,100010,10008,%%%%,391101";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getrpcbynumber.pl -rpcnumber 391000,100010,10008,%%%%,391101";
    system "./modifyNsSwitch.pl -table protocols -order \"files nis \"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getrpcbynumber_test");
}

sub getservent_test()
{
	$res_file = $CURR_RESULT_DIR."getservent.res";
    $ref_file = $REF_RESULT_DIR."getservent.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getservent.pl -repeat $repeatCnt";
    system "./getservent.pl -touch -repeat $repeatCnt" ;
    system "./getservent.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -repeat $repeatCnt";
    system "./getservent.pl -add a,b,c -stopnsd -repeat $repeatCnt";
    system "./getservent.pl -wait $waitForTime  -repeat $repeatCnt";
    system "././modifyNsSwitch.pl -table rpc -order \"nis[success=return] files \"";
    system "./getservent.pl";
    system "./modifyNsSwitch.pl -table rpc -order \"files nis \"";
    system "./getservent.pl";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getservent_test");
}

sub getservbyname_test()
{
	$res_file = $CURR_RESULT_DIR."getservbyname.res";
    $ref_file = $REF_RESULT_DIR."getservbyname.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getservbyname.pl -servname shell,tcp,auth,tcp,%%%%,tcp,sgi-bznet,udp -repeat $repeatCnt" ;
    system "./getservbyname.pl -touch -servname shell,tcp,auth,tcp,%%%%,tcp,sgi-bznet,udp -repeat $repeatCnt" ;
    system "./getservbyname.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -servname shell,tcp,auth,tcp,%%%%,tcp,sgi-bznet,udp,a,tcp,b,tcp  -repeat $repeatCnt";
    system "./getservbyname.pl -add a,b,c -stopnsd  -servname shell,tcp,auth,tcp,%%%%,tcp,sgi-bznet,udp,a,tcp,b,tcp -repeat $repeatCnt";
    system "./getservbyname.pl -wait $waitForTime -servname shell,tcp,auth,tcp,%%%%,tcp  -repeat $repeatCnt";
    system "./getservbyname.pl -entry \"aaa 20000/udp aaa-alias aaa1 aaa2 aaa3 aaa4\naaaa1 40000/tcp  aap\nbbbb\ncccc 60000/udp \ndddd eeeee\nnew-entry 70000/tcp NEW\" -servname aaa,udp,aaaa1,tcp,cccc,udp,dddd,eeeee -repeat $repeatCnt";
	#change the services table entry in /etc/nsswitch
	print "services:          nis\n";
	system "./modifyNsSwitch.pl -table services -order \"nis\"";
	system "./getservbyname.pl -servname zycmgr,tcp,shell,tcp,auth,tcp" ;
	#Revert back
	print "services:          files nis1\n";
	system "./modifyNsSwitch.pl -table services -order \"files nis\"";
	system "./getservbyname.pl -servname zycmgr,tcp,shell,tcp,auth,tcp" ;
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getservbyname zycmgr tcp shell tcp auth tcp 1 0 1 10 4 12 58 77 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    print  "services:	nis\n";
    system "./modifyNsSwitch.pl -table services -order \"nis\"";
    system "./getservbyname.pl -servname zycmgr,tcp,shell,tcp,auth,tcp";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getservbyname.pl -servname zycmgr,tcp,shell,tcp,auth,tcp";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getservbyname.pl -servname zycmgr,tcp,shell,tcp,auth,tcp";
    system "./modifyNsSwitch.pl -table services -order \"files nis \"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getservbyname_test");
}

sub getservbyport_test()
{
	$res_file = $CURR_RESULT_DIR."getservbyport.res";
    $ref_file = $REF_RESULT_DIR."getservbyport.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./getservbyport.pl -servport 2033,tcp,514,tcp,113,tcp -repeat $repeatCnt" ;
    system "./getservbyport.pl -touch -servport 2033,tcp,514,tcp,113,tcp,%%%%,tcp -repeat $repeatCnt" ;
    system "./getservbyport.pl -add a,b,c,d,e,f,g,h,i,j,k,l,n,m,n,o,p,q,r,s,t,u,v,w,x,y,z -servport 2033,tcp,514,tcp,113,tcp,80000,tcp,80001,tcp  -repeat $repeatCnt";
    system "./getservbyport.pl -add a,b,c -stopnsd  -servport 2033,tcp,514,tcp,113,tcp,80000,tcp,80001,tcp -repeat $repeatCnt";
    system "./getservbyport.pl -wait $waitForTime -servport 2033,tcp,514,tcp,113,tcp,80000,tcp,80001,tcp -repeat $repeatCnt";
    system "./getservbyport.pl -entry \"aaa 20000/udp aaa-alias aaa1 aaa2 aaa3 aaa4\naaaa1 40000/tcp  aap\nbbbb\ncccc 60000/udp \ndddd eeeee\nnew-entry 70000/tcp NEW\" -servport aaa,udp,aaaa1,tcp,cccc,udp,dddd,eeeee -repeat $repeatCnt";
	#change the services table entry in /etc/nsswitch
	print "services:          nis\n";
	system "./modifyNsSwitch.pl -table services -order \"nis\"";
	system "./getservbyport.pl -servport 2033,tcp,514,tcp,113,tcp" ;
	#Revert back
	print "services:          files nis1\n";
	system "./modifyNsSwitch.pl -table services -order \"files nis\"";
	system "./getservbyport.pl -servport 2033,tcp,514,tcp,113,tcp" ;
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./getservbyport 2033 tcp 514 tcp 113 tcp 1 0 1 10 4 12 58 77 a.b.c.d---------------sdasd ddsad sdasda a.b.c.d ere.rtrt.sfsfsdfsdf sdfdsfksafk dsfjksdfjsafjsaf sdfkjlsdkfsajflsaf sdfkjsafjlsafj sadfjlskaflfsja sdfjlsafklsafjds dsfjsldafkjlfjalf jlsdflkjsdfkdsf ksdlfdsf lsdfjlskdf dsfjlksdf sdfjksdfl dsfjklsdfj dsfljsldkjf\"";
    #Manipulate the library
    print  "services:	nis\n";
    system "./modifyNsSwitch.pl -table services -order \"nis\"";
    system "./getservbyport.pl -servname zycmgr,tcp,shell,tcp,auth,tcp";
    print  "Moving libns_nis.so to libns_nis.so.mv\n";
    system "/sbin/mv /var/ns/lib/libns_nis.so /var/ns/lib/libns_nis.so.mv";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getservbyport.pl -servport 2033,tcp,514,tcp,113,tcp";
    print  "Moving libns_nis.so.mv back to libns_nis.so \n";
    system "/sbin/mv /var/ns/lib/libns_nis.so.mv /var/ns/lib/libns_nis.so";
    system "/sbin/killall -TERM nsd";
    system "/usr/sbin/nsadmin restart";
    system "./getservbyport.pl -servport 2033,tcp,514,tcp,113,tcp";
    system "./modifyNsSwitch.pl -table services -order \"files nis \"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "getservbyport_test");
}

sub ns_list_test()
{
	$res_file = $CURR_RESULT_DIR."nslist.res";
    $ref_file = $REF_RESULT_DIR."nslist.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list bootparams\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list capability\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list clearance\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list ethers\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list group\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list hosts\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list mac\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list mail\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list netgroup\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list netid.byname\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list rpc\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list networks\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list passwd\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list protocols\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list shadow\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list services\"";
    system "./doRepeat.pl -repeat $repeatCnt -cmd \"./ns_list ypservers\"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "ns_list_test");
}

sub multithread_test()
{
	$res_file = $CURR_RESULT_DIR."multithread.res";
    $ref_file = $REF_RESULT_DIR."multithread.res";
	open(SAVOUT, ">&STDOUT");
	open(SAVERR, ">&STDERR");
	open(STDOUT , "> $res_file");
	open(STDERR , ">&STDOUT");
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allxentP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allgroupsP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allhostsP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allnetsP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allpwP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allprotoP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allrpcP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allservP\"";
	system "./doRepeat.pl -repeat $repeatCnt -cmd \"./allXbyY emt emt1 sys surendra bogus xcxcxc sdwqeuiwudo sdofierueure 111111 343294329432832  2349832743274 3247392479234 937249723984  \"";
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
    &do_diff($ref_file, $res_file, "multithread_test");
}

