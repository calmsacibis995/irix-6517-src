#!/usr/bin/perl5 

$TEST_DIR = "/tmp";
$HOSTNAME_PREFIX = "dynamic";

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
	$etherByName{"$fields[1]"} = $fields[0];
	$etherByMAC{"$fields[0]"} = $fields[1];
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
	$hostsByName{"$fields[2]"} = $fields[0];
	$hostsByAddr{"$fields[0]"} = [ ( $fields[1], $fields[2] ) ];
	$prevHostsAddr = $fields[0];
	$prevHostsName = $fields[2];
    }
}
close (HOSTS);

$etherToIPFile = "$TEST_DIR/etherToIP";
open (ETHERTOIP, "$etherToIPFile") || die "Can't open $etherToIPFile\n";
%etherToIPByAddr = ();
while ($line = <ETHERTOIP>) {
    if ($line =~ /^#/) {
	next;
    }
    if ($line =~ /^$/) {
	next;
    }
    chop($line);
    @fields = split(/[ \t]/, $line);
    $address = "$fields[1]";
    if ($etherToIPByAddr{$address}) {
	print STDOUT "Duplicate $etherToIPByAddr{$address}\n";
    } else {
	$etherToIPByAddr{$address} = "$line";
    }
}
close(ETHERTOIP);

open(ETHERSNEW, ">$ethersFile.new");
open(HOSTSNEW, ">$hostsFile.new");
open(ETHERTOIPNEW, ">$etherToIPFile.new");
while (($addr, $line) = each(%etherToIPByAddr)) {
    @fields = split(/[ \t]/, $line);
    @hfields = split(/\./, $fields[2]);
    print HOSTSNEW "$addr\t$fields[2]\t$hfields[0]\n";
    print ETHERSNEW "$fields[0]\t$hfields[0]\n";
    print ETHERTOIPNEW "$line\n";
}
close (ETHERSNEW);
close (ETHERTOIPNEW);
close (HOSTSNEW);

print STDOUT "Differences in ethers\n------------\n";
system("sort -k2 $ethersFile | grep $HOSTNAME_PREFIX > $ethersFile.old.sort");
system("sort -k2 $ethersFile.new | grep $HOSTNAME_PREFIX > $ethersFile.new.sort");
system("diff $ethersFile.old.sort $ethersFile.new.sort");

print STDOUT "\n\nDifferences in hosts\n------------\n";
system("sort -k3 $hostsFile | grep $HOSTNAME_PREFIX > $hostsFile.old.sort");
system("sort -k2 $hostsFile.new | grep $HOSTNAME_PREFIX > $hostsFile.new.sort");
system("diff $hostsFile.old.sort $hostsFile.new.sort");

print STDOUT "\n\nDifferences in etherToIP\n------------\n";
system("sort -k3 $etherToIPFile | grep $HOSTNAME_PREFIX > $etherToIPFile.old.sort");
system("sort -k2 $etherToIPFile.new | grep $HOSTNAME_PREFIX > $etherToIPFile.new.sort");
system("diff $etherToIPFile.old.sort $etherToIPFile.new.sort");

