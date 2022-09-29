#!/usr/sbin/perl

if($#ARGV < 0) {
   &print_usage();
}

sub print_usage()
{
	print "Usage: ./nsnisattr.pl <host_name>";
}

#Hosts
print "==hosts: nis dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis dns files\"";
system "./gethostbyname @ARGV";
print "==hosts: nis(domain=engr.sgi.com,nis_servers=ankit)  dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis(domain=engr.sgi.com,nis_servers=ankit) dns files\"";
system "./gethostbyname @ARGV";
print "==hosts: nis(domain=engr.sgi.com,multicast=0) dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis(domain=engr.sgi.com,multicast=0) dns files\"";
system "./gethostbyname @ARGV";
print "==hosts: nis(domain=engr.sgi.com,nis_servers=ankit,multicast=0) dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis(domain=engr.sgi.com,nis_servers=ankit,multicast=0) dns files\"";
system "./gethostbyname @ARGV";
print "==hosts: nis(timeout=1)  dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis(timeout=1) dns files\"";
system "./gethostbyname @ARGV";
print "==hosts: nis dns files\n";
system "./modifyNsSwitch.pl -table hosts -order \"nis dns files\"";
system "./gethostbyname @ARGV";

