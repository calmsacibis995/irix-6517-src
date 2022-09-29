#!/usr/sbin/perl

#
#	1.This script sets the current machine as the NIS server for the
#	  domain specified with -domain option.
#		./setupNisServer.pl -domain <domain_name> -etcref <dir_containing_ref_etc_files>

$domainNameFlg = 0;
$etcRefDir = "/etc";

@options = @ARGV;

while(@ARGV) {
  $_ = shift;
  if(/^-domain(.*)/) {
	$domainName = shift(@ARGV);
	$domainNameFlg = 1;
  } elsif(/^-etcref(.*)/) {
	$etcRefDir = shift(@ARGV);;
  } else {
	&print_usage();
  }
  
}

if(!$domainNameFlg) {
	&print_usage();
}

if ($> != 0) {
   die "You need to be root to set the NIS server\n";
}

print "Setting domain name... to $domainName\n";
system "/usr/bin/domainname $domainName";

$hostname = `/usr/bsd/hostname`;
chop($hostname);

#run ypinit to make it NIS master
print "Running ypinit -m to make $hostname NIS master for domain $domainName\n";
system "/var/yp/ypinit -m";

print "ypinit completed.\n";
print "Hostname:$hostname\n";
$dname =`/usr/bin/domainname`;
chop($dname);
print "Doamin:$dname\n";
$nisBind = `/usr/bin/ypwhich`;
chop($nisBind);
print "NIS BIND:$nisBind\n";



sub print_usage
{
   print "Usage :./setupNisServer.pl \n\t-domain <domain_name>\n\t-etcref <dir_name_containing_ref_etc_files>\n";
   exit(0);

}

