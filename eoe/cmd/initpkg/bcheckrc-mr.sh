#! /bin/sh

# This file has those commands necessary to insure the integrity of the
# root file system.
#
# $Revision: 1.36 $
# $Date: 1997/12/01 19:42:34 $

# remove unused unix.* boot images before calling
# MAKEDEV, to ensure that MAKEDEV doesn't run out
# of disk space, causing serious confusion.

. mrprofrc

# create /tmp (used by shell for tmp files)
test -d /tmp || mkdir /tmp > /dev/null 2>&1

CPU_TYPE=`uname -m`

if test ! -f /unix
then
    if test -f /unix.$CPU_TYPE
    then
        ln /unix.$CPU_TYPE
        rm -f /unix.*
        ln /unix /unix.$CPU_TYPE
    fi
fi

# grow the miniroot filesystem from it's shipped to it's working size
# the grown size *must* be <= swap mount offset defined in ./brc-mr.sh
# and master.d/system.gen.  For IP20/IP22/IP26 use the smaller swap space.


if [ "$CPU_TYPE" = "IP20" -o "$CPU_TYPE" = "IP22" -o "$CPU_TYPE" = "IP26" ]
then
	/usr/sbin/xfs_growfs -D 57000 / 2>&1 >/tmp/growfs.log
else
	/usr/sbin/xfs_growfs -D 63000 / 2>&1 >/tmp/growfs.log
fi

if [ $? -ne 0 ]
then
    echo "Error from xfs_growfs : "
    cat /tmp/growfs.log
fi
/bin/rm /tmp/growfs.log > /dev/null 2>&1

# re-create /dev if possible and necessary
if [ `/bin/ls /dev|wc -l` -lt 10 ]
then
	# because it takes so long, and looks like the system is
	# hung, unless you can see the system disk LED
	echo Creating miniroot devices, please wait...\\c
	cd /dev; ./MAKEDEV MAXPTY=10 MAXGRO=4 MAXGRI=4 mindevs scsi > /dev/null
	echo ''
fi

set `/sbin/devnm /`
rootfs=$1
/sbin/fsstat ${rootfs}  >/dev/null 2>&1

if [ $? -ne 0 ]
then
	echo "The root file system, ${rootfs}, is being checked automatically."
	/sbin/fsck -y -D -b ${rootfs}
fi

# Remove ps temporary file
rm -rf /tmp/.ps_data

