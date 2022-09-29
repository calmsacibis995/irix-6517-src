#! /sbin/sh
#ident "$Revision: 1.8 $"
#
# This script is responsible for mounting the /proc (/debug) file system.
# This is typically invoked by /etc/brc upon bootup.
#

# The /proc (ie /debug) directory has to be present since ps
# and killall depend upon this now.
if [ ! -d /debug -o ! -d /proc -o ! -l /debug ] ; then
	/sbin/rm -rf /debug /proc > /dev/null 2>&1
	mkdir /proc
	ln -s proc /debug <&- > /dev/null 2>&1
fi

if /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -t proc /proc /proc"
then :
else
	echo "Unable to mount /proc - ps(1) and killall(1M) will not work"
	echo "Please see mntproc(1M)"
fi
