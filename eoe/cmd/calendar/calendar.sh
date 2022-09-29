#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)calendar:calendar.sh	1.8"
#	calendar.sh - calendar command, uses /usr/lib/calprog

PATH=/usr/bin
_tmp=/tmp/cal$$
trap "rm -f ${_tmp} /tmp/calendar.$$; trap '' 0; exit" 0 1 2 13 15
/usr/lib/calprog > ${_tmp}
case $# in
0)	if [ -f calendar ]; then
		egrep -f ${_tmp} calendar
	else
		echo $0: `pwd`/calendar not found
	fi;;
*)	/usr/lib/calnames | \
		while read _user _dir; do
			if [ -s ${_dir}/calendar ]; then
				egrep -f ${_tmp} ${_dir}/calendar 2>/dev/null \
					> /tmp/calendar.$$
				if [ -s /tmp/calendar.$$ ]; then
					/usr/sbin/Mail -s calendar ${_user} < /tmp/calendar.$$
				fi
			fi
		done;;
esac
exit 0
