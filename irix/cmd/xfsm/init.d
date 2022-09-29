#! /bin/sh
#
# This script accompanies the xfsmgr product.  It starts and stops the
# daemon for xfsmgr.
#
#

IS_ON=/sbin/chkconfig

if $IS_ON verbose ; then
    ECHO=echo
else
    ECHO=:
fi

XFS_DIR=/usr/opt/xfsm/bin
DB_DIR=/var/IRIXpro


case "$1" in
    'start')
	#
	# Start xfsmgr daemon.
	#
	$ECHO "xfsmgr daemon:\c"
	if test -x ${XFS_DIR} ; then
		/sbin/killall xfsmd
		${XFS_DIR}/xfsmd &
		$ECHO " xfsmd\c"
	fi
	$ECHO "."
	;;

    'stop')
	#
	# Kill the lock manager.
	#
	/sbin/killall xfsmd
	;;


    *)
	echo "usage: $0 {start|stop}"
	;;
esac

exit 0
