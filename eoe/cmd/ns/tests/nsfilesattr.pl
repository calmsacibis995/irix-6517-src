#!/usr/sbin/perl

if($#ARGV < 0) {
   &print_usage();
}

sub print_usage()
{
	print "Usage: ./nsfilesattr.pl <host_name>";
}

#Hosts
print "==hosts: files nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files nis dns\"";
system "./gethostbyname @ARGV";
print "==hosts: files(domain=engr.sgi.com,file=/etc/hosts,timeout=0)  nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files(domain=engr.sgi.com,file=/etc/hosts)  nis dns\"";
system "./gethostbyname @ARGV";
print "==hosts: files(file=/etc/junk) nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files(file=/etc/junk) nis dns\"";
system "./gethostbyname @ARGV";
print "==hosts: files(timeout=0) nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files(timeout=0) nis dns \"";
system "./gethostbyname @ARGV";
print "==hosts: files  nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files nis dns\"";
system "./gethostbyname @ARGV";

