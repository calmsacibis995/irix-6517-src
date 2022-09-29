#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.16 $"
#
# Mount file systems according to file system table /etc/fstab.
# check all filesystems if necessary, in parallel.

if [ -x /sbin/fsck ] ; then
	/sbin/fsck -m -c -y
fi

# mount all local filesystems; caller gets exit status of mount as exit status
mount -a -T efs,xfs
