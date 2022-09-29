#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# Easy stuff that is delayed until entering initstate 2.
# Easy in that it doesn't have any other initstate 2 prereqs
# and in that it is unlikely to fail.
# Delayed until state 2 because it does have significant
# impact and isn't essential for single user.

date '+%nCurrent system date is %a %b %d %T %Z %Y%n'

test -d /var/tmp || mkdir /var/tmp 2>/dev/null

if test ! -f /usr/tmp/swap.virtual
then
    touch /usr/tmp/swap.virtual
    swap -a -v 80000 /usr/tmp/swap.virtual 0 0
    echo "/usr/tmp/swap.virtual swap swap vlength=80000 0 0" >> /etc/fstab
fi

mrmode=`nvram mrmode 2>/dev/null`
if test "$mrmode" = test
then
    echo "Miniroot test mode - type exit when ready to continue"
    /bin/csh
fi
