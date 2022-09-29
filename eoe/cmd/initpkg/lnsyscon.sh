#! /sbin/sh
#Tag 0x00000f00
#
# lnsyscon - link /dev/syscon to /dev/console
#
# /dev/syscon is the device init treats as the console for single user
# mode and /dev/console redirection in /etc/inittab.  The command ``init s''
# changes /dev/syscon to whereever the command is run.  This script changes
# it back to /dev/console when the system goes back to multi-user mode.
#
# $Revision: 1.6 $

# make syscon and console the same

set `ls -ilL /dev/console /dev/syscon |
	sed -e 's/.*\( [0-9]*\), *\([0-9]*\).*/\1 \2/'`
if test -c /dev/console \
	-a '(' ! -c /dev/syscon -o "$1" != "$3" -o "$2" != "$4" ')'
then
    rm -rf /dev/syscon
    ln /dev/console /dev/syscon
fi
