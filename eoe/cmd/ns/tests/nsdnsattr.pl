#!/usr/sbin/perl

if($#ARGV < 0) {
   &print_usage();
}

#Hosts
print "==hosts: files nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files nis dns\"";
system "./gethostbyname @ARGV";
print "==hosts: dns(domain=engr.sgi.com,dns_timeout=0)  nis files \n";
system "./modifyNsSwitch.pl -table hosts -order \"dns(domain=engr.sgi.com,timeout=0)  nis files \"";
system "./gethostbyname @ARGV";
print "==hosts: dns(dns_parallel=1,dns_retries=1) nis files \n";
system "./modifyNsSwitch.pl -table hosts -order \"dns(dns_parallel=1,dns_retries=1) nis files \"";
system "./gethostbyname @ARGV";
print "==hosts: dns(dns_timeout=0) nis files \n";
system "./modifyNsSwitch.pl -table hosts -order \"dns(dns_timeout=0) nis files \"";
system "./gethostbyname @ARGV";
print "==hosts: files  nis dns \n";
system "./modifyNsSwitch.pl -table hosts -order \"files nis dns\"";
system "./gethostbyname @ARGV";

sub print_usage()
{
	print "Usage: ./nsdnsattr.pl <host_name> ";
	exit;
}
