#!/sbin/sh

# Copyright 1995 Silicon Graphics, Inc.
# All rights reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
# the contents of this file may not be disclosed to third parties, copied or
# duplicated in any form, in whole or in part, without the prior written
# permission of Silicon Graphics, Inc.
#
# RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
# and Computer Software clause at DFARS 252.227-7013, and/or in similar or
# successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
# rights reserved under the Copyright Laws of the United States.


# Start or stop the miser daemon 

IS_ON=/etc/chkconfig	# tool to check if miser is configured on

if $IS_ON verbose; then
	ECHO=echo
else			# quiet startup and shutdown
	ECHO=:
fi

CONFIG=/etc/config	# location for config files
MISER=/usr/etc/miser	# path to miser daemon
OPTS=			# options to miser daemon 

case "$1" in

	'cleanup')		# release miser reserved resources
		$MISER -C -v -d	# C - cleanup, v - verbose, d - debug output
		;;

	'stop')			# kill miser
		trap 2; /sbin/killall -INT miser; # send signal 'INT' to miser
		$MISER -C -v -d	# C - cleanup, v - verbose, d - debug output
		;;

	'start')		# startup miser with options and config file
		if $IS_ON miser; then

			if test -x $MISER; then

				if test -s $CONFIG/miser.options; then
					OPTS="$OPTS `cat $CONFIG/miser.options`"
				fi

			$ECHO "Resource management daemon: miser\n"
			/usr/sbin/npri -r 39 -s RR $MISER $OPTS
		fi

		else 
			$ECHO "Resource management daemon:"
		fi
		;;

	*)

	echo "Usage: $0 {start|stop|cleanup}"
	;;

esac
