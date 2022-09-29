#!/usr/sbin/perl

$shadowfied=0;

#check to see if /etc/shadow file exists
if(-e "/etc/shadow") {
    $shadowfied=1;
}

if(!$shadowfied) {
    ##Just do lookup without shadow
    print "###Doing lookup without shadow\n";
    &do_looup();
    &do_shadowfy();
}

print "###Doing lookup with shadow\n";
&do_looup();
&do_goback();


#### SUBS ###############
sub do_shadowfy()
{
   print "###Creating shadow file....\n";
   system "/sbin/pwconv ";
}

sub do_looup()
{
   print "./getspnam ",join(' ',@ARGV)," \n";
   system "./getspnam @ARGV";
}

sub do_goback()
{
   if(!$shadowfied) {
	print "###Reverting back to original state\n";
	system "/sbin/mv /etc/opasswd /etc/passwd;touch /etc/passwd";
	system "/sbin/rm /etc/shadow";
	system "/usr/sbin/nsadmin restart";
   }
}
