#! /sbin/sh
#ident "$Revision: 1.15 $"

#
# Unmounts all but the root file system (/).
#
exec /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/umount -ak -b /proc,/debug,/hw"
