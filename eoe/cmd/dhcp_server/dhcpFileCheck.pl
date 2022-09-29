#!/usr/bin/perl5 

$TEST_DIR = "/tmp";
$HOSTNAME_PREFIX = "dynamic";

# file consistency checks

# check ethers file for duplicate names

$ethersFile = "$TEST_DIR/ethers";
open(ETHERS, "sort -k2 $ethersFile|");
$prevEtherName = "";
while ($line = <ETHERS>) {
    if ($line =~ /^#/) {
	next;
    }
    if ($line =~ /^$/) {
	next;
    }
    chop($line);
    @fields = split(/[ \t]/, $line);
    if ($fields[1] =~ /$HOSTNAME_PREFIX.*/) {
	if ($fields[1] eq $prevEtherName) {
	    print STDOUT "Ether: Duplicate name found $prevEtherName\n";
	    next;
	}
	$etherByName[$fields[1]] = $fields[0];
	$etherByMAC[$fields[0]] = $fields[1];
	$prevEtherName = $fields[1];
    }
}
close(ETHERS);


$hostsFile = "$TEST_DIR/hosts";
$prevHostsAddr = "";
$prevHostsName = "";
open(HOSTS, "sort -k1 $hostsFile|");
while ($line = <HOSTS>) {
    if ($line =~ /^#/) {
	next;
    }
    if ($line =~ /^$/) {
	next;
    }
    chop($line);
    @fields = split(/[ \t]/, $line);
    if ($fields[2] =~ /$HOSTNAME_PREFIX.*/) {
	if ($prevHostsAddr eq $fields[0]) {
	    print STDOUT "Hosts: Duplicate address $prevHostsAddr\n";
	    next;
	} elsif ($prevHostsName eq $fields[2]) {
	    print STDOUT "Hosts: Duplicate address $prevHostsAddr\n";
	    next;
	}
	$hostsByName[$fields[2]] = $fields[0];
	$hostsByAddr[$fields[0]] = [ ( $fields[1], $fields[2] ) ];
	$prevHostsAddr = $fields[0];
	$prevHostsName = $fields[2];
    }
}
close (HOSTS);

#$etherToIPFile = "$TEST_DIR/etherToIP
