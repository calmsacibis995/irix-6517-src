#! /sbin/sh -e
#Tag 0x00000800

# Reboot the system--take it down and let it come back up
#
# $Revision: 1.11 $"

if test -n "$REMOTEHOST"; then
	echo "Reboot `hostname -s`?  \c"
	read reply
	case "$reply" in
	[yY]*)
		;;
	*)
		echo "'Reboot' cancelled."
		exit 0
		;;
	esac
fi

/etc/shutdown -y -g0 -i6

sleep 10 2> /dev/null
