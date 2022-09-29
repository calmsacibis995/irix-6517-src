#!/usr/sbin/perl
#To test this case give the network user name as an argument 
#which does not exist in local /etc/password
while(<@ARGV>) {
print "User:", $_, "\n";
#change the passwd table entry in /etc/nsswitch
print "passwd:		files(compat) [notfound=return] nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"files(compat) [notfound=return] nis\"";
#Reinitialize the nsd
system "/sbin/killall -HUP nsd";
#User is a nis user with no entry in the local /etc/passwd file
system "./getgrmember $_";

#now change the entry to take out the condition
print "passwd:		files(compat) nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"files(compat) nis\"";
#Reinitialize the nsd
system "/sbin/killall -HUP nsd";
#User is a nis user with no entry in the local /etc/passwd file
system "./getgrmember $_";

#now change the entry to take out the compat
print "passwd:		files [notfound=return] nis\n";
system "./modifyNsSwitch.pl -table passwd -order \"files [notfound=return] nis\"";
#Reinitialize the nsd
system "/sbin/killall -HUP nsd";
#User is a nis user with no entry in the local /etc/passwd file
sleep(35);
system "./getgrmember $_";
}

 

