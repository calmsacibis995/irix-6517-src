#!/usr/bin/perl

`cp /etc/syslog.conf /etc/syslog.conf.precached`;
open(FILE, "</etc/syslog.conf") || die "unable to open /etc/syslog,conf";

while(<FILE>) {
    next unless /cached/;
        print "Found $_\n";
    close(FILE);
    exit(0);
}
print "Adding user stuff\n";
close(FILE);
open(FILE, ">>/etc/syslog.conf") || die "unable to open /etc/syslog,conf";

printf FILE "user.alert\t\t|/var/adm/cached.filt\t/dev/console\n";
close(FILE);
