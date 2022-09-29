#!/usr/sbin/perl

##Host data
$localHost="ankit.engr.sgi.com";
$nisHost="nuptse";
$localHostAddr="150.166.75.52";
$nisHostAddr="150.166.61.71";
print "Host data:local:$localHost:$localHostAddr, NIS:$nisHost:$nisHostAddr\n";


##Passwd data
$localUser="sys";
$nisUser="emt";
$localUseruid=0;
$nisUseruid=0;
$localUseruid = getpwnam($localUser);
$nisUseruid = getpwnam($nisUser);
print "Passwd data:Local:$localUser:$localUseruid, NIS:$nisUser:$nisUseruid\n";

###Grp data
$localGroup="sys";
$nisGroup="emt";
$localGroupid=0;
$nisGroupid=0;
$localGroupid = getgrnam($localGroup);
$nisGroupid = getgrnam($nisGroup);
print "Group data:Local:$localGroup:$localGroupid, NIS:$nisGroup:$nisGroupid\n";

###rpc data
$localrpcname="portmapper";
$nisrpcname="quorumd";
$localrpcnumber=100000;
$nisrpcnumber=391101;
print "RPC DATA:Local:$localrpcname:$localrpcnumber, NIS:$nisrpcname:$nisrpcnumber\n";

###service data
$localservname="tcpmux";
$nisservname="zycmgr";
$localservnumber=getservbyname($localservname, "tcp");
$nisservnumber=getservbyname($nisservname, "tcp");
print "Service data:Local:$localservname:$localservnumber, NIS:$nisservname:$nisservnumber\n";


####Proto data
$localprotoname="ip";
$nisprotoname="iso-tp4";
$localprotonumber=getprotobyname($localprotoname);
$nisprotonumber=getprotobyname($nisprotoname);
print "Protocol data:Local:$localprotoname:$localprotonumber, NIS:$nisprotoname:$nisprotonumber\n";


###/etc/hosts####
#Host lookup
print "./bMark.pl -kudzu -o 1 -host -f gethostbyname -t 10000 -n $localHost\n";
system "./bMark.pl -kudzu -o 1 -host -f gethostbyname -t 10000 -n $localHost";
print "./bMark.pl -kudzu -o 2 -host -f gethostbyaddr -t 10000 -n $localHostAddr\n";
system "./bMark.pl -kudzu -o 2 -host -f gethostbyaddr -t 10000 -n $localHostAddr";
##NIS Lookup
print "./bMark.pl -kudzu -o 1 -host -f gethostbyname -t 10000 -n $nisHost\n";
system "./bMark.pl -kudzu -o 1 -host -f gethostbyname -t 10000 -n $nisHost";
print "./bMark.pl -kudzu -o 2 -host -f gethostbyaddr -t 10000 -n $nisHostAddr\n";
system "./bMark.pl -kudzu -o 2 -host -f gethostbyaddr -t 10000 -n $nisHostAddr";

###/etc/passwd####
#local lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getpwnam -n $localUser\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getpwnam -n $localUser";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getpwuid -n $localUseruid\n";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getpwuid -n $localUseruid";
#NIS Lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getpwnam -n $nisUser\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getpwnam -n $nisUser";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getpwuid -n $nisUseruid\n";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getpwuid -n $nisUseruid";

###/etc/group
#local lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getgrnam -n $localGroup\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getgrnam -n $localGroup";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getgrgid -n $localGroupid\n";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getgrgid -n $localGroupid";
#NIS lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getgrnam -n $nisGroup\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getgrnam -n $nisGroup";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getgrgid -n $nisGroupid\n";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getgrgid -n $nisGroupid";

###/etc/rpc####
#local lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getrpcbyname -n $localrpcname\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getrpcbyname -n $localrpcname";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getrpcbynumber -n $localrpcnumber\n";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getrpcbynumber -n $localrpcnumber";
##NIS lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getrpcbyname -n $nisrpcname\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getrpcbyname -n $nisrpcname";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getrpcbynumber -n $nisrpcnumber\n";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getrpcbynumber -n $nisrpcnumber";

####/etc/services###
#local lookup
print "./bMark.pl -o 1 -kudzu -service -t 1000 -f getservbyname -n $localservname\n";
system "./bMark.pl -o 1 -kudzu -service -t 1000 -f getservbyname -n $localservname";
print "./bMark.pl -o 2 -kudzu -service -t 1000 -f getservbyport -n $localservnumber\n";
system "./bMark.pl -o 2 -kudzu -service -t 1000 -f getservbyport -n $localservnumber";
#NIS lookup
print "./bMark.pl -o 1 -kudzu -service -t 1000 -f getservbyname -n $nisservname\n";
system "./bMark.pl -o 1 -kudzu -service -t 1000 -f getservbyname -n $nisservname";
print "./bMark.pl -o 2 -kudzu -service -t 1000 -f getservbyport -n $nisservnumber\n";
system "./bMark.pl -o 2 -kudzu -service -t 1000 -f getservbyport -n $nisservnumber";


###/etc/protocols####
#local lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getprotobyname -n $localprotoname\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getprotobyname -n $localprotoname";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getprotobynumber -n $localprotonumber\n";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getprotobynumber -n $localprotonumber";
##NIS lookup
print "./bMark.pl -o 1 -kudzu -t 10000 -f getprotobyname -n $nisprotoname\n";
system "./bMark.pl -o 1 -kudzu -t 10000 -f getprotobyname -n $nisprotoname";
print "./bMark.pl -o 2 -kudzu -t 10000 -f getprotobynumber -n $nisprotonumber\n";
system "./bMark.pl -o 2 -kudzu -t 10000 -f getprotobynumber -n $nisprotonumber";

