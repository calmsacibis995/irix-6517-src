#! /bin/sh
#Tag 0x00000f00
#ident "$Revision: 1.33 $"

# mini-root version
#
#
# This script does the early, easy post-boot stuff on the miniroot:
#    Initialize /etc/fstab and /etc/mtab
#    Adds swap (on the remainder of partition 1)
#    Mounts /proc

ln -s /dev/root /dev/miniroot 1>&- 2>&-
rootdev=/dev/miniroot
if test ! -f /etc/fstab
then
	echo "$rootdev /" | setmnt -f /etc/fstab
fi
echo "$rootdev /" | setmnt

# In the miniroot, swap is always added from here.
# We need fstab around for the swap command
# The swaplo amount should match what is found in the master.d/system.gen
# and >= the value to grow the fs in the file ./bcheckrc-mr.sh
# For IP20/IP22/IP26 use the smaller swap space.

CPU_TYPE=`uname -m`
if [ "$CPU_TYPE" = "IP20" -o "$CPU_TYPE" = "IP22" -o "$CPU_TYPE" = "IP26"  ]
then
	su root -C CAP_SWAP_MGT+ip -c "/sbin/swap -i -a /dev/swap 57000"
else
	su root -C CAP_SWAP_MGT+ip -c "/sbin/swap -i -a /dev/swap 63000"
fi

# Mount the /proc file system
/etc/mntproc

/etc/mnthwgfs
