#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.33 $"

#	"Run Commands" for init state 0
#	Leaves the system in a state where it is safe to turn off the power
#	or go to firmware.

if [ -d /etc/rc0.d ]
then
	for f in /etc/rc0.d/K*
	{
		if [ -s ${f} ]
		then
			/sbin/sh ${f} stop
		fi
	}

#	system cleanup functions ONLY (things that end fast!)	

	for f in /etc/rc0.d/S*
	{
		if [ -s ${f} ]
		then
			/sbin/sh ${f} start
		fi
	}
fi

trap "" 15
/sbin/suattr -C CAP_KILL,CAP_MAC_WRITE+ipe -c "kill -15 -1"
sleep 1
/sbin/suattr -C CAP_SHUTDOWN+ip -c "/sbin/killall 9"

# unmount anything that didn't get unmounted before.
_DLMNTS=""
if [ "`/sbin/nvram diskless 2> /dev/null`" -eq 1 ] ; then
	_DLMNTS=",/swap,/sbin"
fi
/sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/umount -ak -b /proc,/debug,/hw,/var,/usr,/dev/fd${_DLMNTS}"

# install reconfigured kernel
if test -x /unix.install; then
    mv /unix.install /unix
fi

# sometimes one of the umount -k's during shutdown will block, and keep
# / from being umounted cleanly
/sbin/killall umount

sync
