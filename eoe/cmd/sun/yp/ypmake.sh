#!/bin/sh
# NAME
#	ypmake - NIS master script to update databases and to rotate logfiles
# SYNOPSIS
#	/var/yp/ypmake
# DESCRIPTION
#	ypmake is run on NIS master servers by cron(1M) to ensure that their 
#	NIS databases are up-to-date and pushed out to slave servers.
#
#	You can also use this script to push a map after updating its 
#	data file. Any output from the make is printed on the standard output.
#
#	An optional configuration file, /etc/config/ypmaster.options,
#	lets you customize the variables used by make when it creates
#	the NIS databases. See ypmake(1) for details.

cd /var/yp

LOG=ypmake.log

test -t 0
interactive=$?

# Keep the log file open during all actions for fuser test
touch $LOG
exec < $LOG

# Make sure another ypmake isn't running already.  Do an echo after fuser output
# because sed doesn't always work correctly without a newline
pids="`(/sbin/fuser -q $LOG $LOG.old; echo '') < /dev/null | sed -e s/$$,*//g -e 's/,*$//'`"

if test -n "$pids"; then
	err="already in use by processes $pids"
	if  test $interactive = 0; then
	    echo "ypmake: $err"
	else
	    logger -t ypmake -p daemon.warning "$err"	# log it in SYSLOG
	fi
	exit 1
fi

if test $interactive = 1; then		# not interactive
	if /etc/chkconfig ypmaster; then :
	else
		exit 0
	fi
	PATH=$PATH:`dirname $0`
	exec >> $LOG 2>&1

	# Between 12:00am and 12:14am, redo the maps if they haven't been
	# made in the past day.
	if test `date +%H%M` -le 14; then
		if test -n "`find . -name '*.time' -mtime +0 \
				-type f -print 2>/dev/null`"; then
			rm -f *.time
		fi
		mv -f $LOG $LOG.old
		exec >> $LOG 2>&1
		date
	fi
fi

/usr/sbin/ypset `hostname` 2>/dev/null
make -ksf make.script `cat /etc/config/ypmaster.options 2>/dev/null` $*

#
# Build the databases required for any of the extended (C2, B1 and/or CMW)
# security options.
#
# Note:	These are only appropriate in safe-network envioronments.
#	The "secure" map feature of NIS simply verifies that the response
#	is destined for a Privileged Port. If all machines on the network
#	obey that rule, and nobody goes net snooping, we're OKay.
#
if /etc/chkconfig ypsecuremaps ; then

	if test -f /etc/capability ; then
		make -sf make.script \
		    `cat /etc/config/ypmaster.options 2>/dev/null` \
		    $* capability
	fi

	if test -f /etc/clearance ; then
		make -sf make.script \
		    `cat /etc/config/ypmaster.options 2>/dev/null` \
		    $* clearance
	fi

	if test -f /etc/shadow ; then
		make -sf make.script \
		    `cat /etc/config/ypmaster.options 2>/dev/null` \
		    $* shadow
	fi
fi
