#! /sbin/sh
#Tag 0x00000f00
#
# Take the system single user.  This is a nop if the system is already
# single user.
#
# $Revision: 1.5 $

if test -n "$REMOTEHOST"; then
	echo "Shutdown `hostname`?  \c"
	read reply
	case "$reply" in
	[yY]*)
		;;
	*)
		echo "'single' cancelled."
		exit 0
		;;
	esac
fi

/sbin/telinit s
