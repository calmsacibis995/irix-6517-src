#!/bin/sh
#
# For mini-root only
#
if [ `/usr/sbin/sysconf KERN_POINTERS` -eq 64 ]
then
exec /usr/etc/netstat_64 $*
else
exec /usr/etc/netstat_32 $*
fi
