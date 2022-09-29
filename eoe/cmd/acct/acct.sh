#! /bin/sh
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"$Revision: 1.5 $"

if /etc/chkconfig acct; then :
else
	exit
fi

state=$1
set `who -r`
case $state in

'start')
	# if coming from state 2 or 3 theres nothing to do
	if [ $9 = "2" -o $9 = "3" ]
	then
		exit
	fi
	echo "Starting process accounting"
	/usr/lib/acct/startup
	;;

'stop')
	echo "Stopping process accounting"
	if test -x /usr/lib/acct/shutacct
	then
		/usr/lib/acct/shutacct
	fi
	;;
esac
