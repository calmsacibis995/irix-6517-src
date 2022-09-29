# miniroot's sh profile

# $Revision: 1.3 $

umask 022

# the absence of '.' in this path is quite necessary.  It closes an 
#	otherwise bad security breach.
PATH=/usr/sbin:/usr/bsd:/usr/bin:/bin:/etc:/usr/etc
export PATH

stty line 1 erase '^H' kill '^U' intr '^C' echoe 
