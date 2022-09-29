#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.18 $"
#	"Run Commands" for init state 1
#	Leaves the system in state S for system maintenance.
#	The sequence is the same as for state 0 except for the
#	transition to state S

echo 'The system is coming down.  Please wait.'

#	The following segment is for historical purposes.
#	There should be nothing in /etc/shutdown.d.
if [ -d /etc/shutdown.d ]
then
	for f in /etc/shutdown.d/*
	{
		if [ -f /etc/shutdown.d/$f ]
		then
			/bin/sh ${f}
		fi
	}
fi
#	End of historical section

if [ -d /etc/rc0.d ]
then
	for f in /etc/rc0.d/K*
	{
		if [ -s ${f} ]
		then
			/bin/sh ${f} stop
		fi
	}

#	system cleanup functions ONLY (things that end fast!)	

	for f in /etc/rc0.d/S*
	{
		if [ -s ${f} ]
		then
			/bin/sh ${f} start
		fi
	}
fi

trap "" 15
/sbin/suattr -C CAP_KILL,CAP_MAC_WRITE+ipe -c "kill -15 -1"
sleep 10
/sbin/suattr -C CAP_SHUTDOWN+ip -c "/sbin/killall 9"
sleep 3

# unmount anything that didn't get unmounted before.
/sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/umount -ak -b /proc,/debug,/hw"
sync

sync;  sync
echo '
The system is down.'
sync
/sbin/init S
