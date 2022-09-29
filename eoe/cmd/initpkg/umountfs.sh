#! /sbin/sh
#ident "$Revision: 1.6 $"

# unmount what we didn't in rc0.

if [ "`/sbin/nvram diskless 2> /dev/null`" -eq 1 ] ; then
	        _DLMNTS=",/swap,/sbin,/usr"
fi
/sbin/suattr -C CAP_MOUNT_MGT+ip -c "/etc/umount -ak -b /proc,/debug,/hw${_DLMNTS}" > /dev/console 2>&1

#
#  xlv_shutdown gracefully shuts down all xlv volumes (except
#  a root xlv volume, if one exists). The filesystems should be
#  unmounted before the underlying xlv volumes are 'disassembled'.
#  However, xlv_shutdown does make sure this is actually the case;
#  if they are still left mounted for some reason, it doesn't 
#  shutdown the corresponding logical volume.
#

if [ -x /sbin/xlv_shutdown ]; then
	/sbin/suattr -C CAP_DEVICE_MGT+ip -c "/sbin/xlv_shutdown"
fi
