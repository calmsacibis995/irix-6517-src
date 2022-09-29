#!/bin/sh 
#Tag 0x00000700
#
#**************************************************************************
#*
#*                Copyright (c) 1992 Silicon Graphics, Inc.
#*                      All Rights Reserved
#*
#*         THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
#*
#* The copyright notice above does not evidence any actual of intended
#* publication of such source code, and is an unpublished work by Silicon
#* Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is
#* the property of Silicon Graphics, Inc. Any use, duplication or
#* disclosure not specifically authorized by Silicon Graphics is strictly
#* prohibited.
#*
#* RESTRICTED RIGHTS LEGEND:
#*
#* Use, duplication or disclosure by the Government is subject to
#* restrictions as set forth in subdivision (c)(1)(ii) of the Rights in
#* Technical Data and Computer Software clause at DFARS 52.227-7013,
#* and/or in similar or successor clauses in the FAR, DOD or NASA FAR
#* Supplement. Unpublished - rights reserved under the Copyright Laws of
#* the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.
#* Shoreline Blvd., Mountain View, CA 94039-7311
#**************************************************************************
#*
#* File: mksvr4netpr.sh
#*
#*
# usage: mksvr4netpr PRINTER HOSTNAME HOSTPRINTER

# mknetpr takes PRINTER HOSTNAME HOSTPRINTER
# 	where PRINTER is the local name you want for a remote printer
#	HOSTNAME is the machine the real printer is on; HOSTPRINTER
#	is the name of the printer on that machine.
MODEL=/etc/lp/model
usage() {
	echo "usage: mksvr4netpr [-v] [-l] [-i <remote login id>] <name> <host> <host printer>"
	echo "	-v		Keep a verbose activity log"
	echo "	-l		Make a unique log file for the printer"
	echo "	-i		Specify login id to be used when communicating with"
	echo "			the remote machine.  (default is \"lp\")"
	echo "	<name>		Local printer name"
	echo "	<host>		Name of remote host with printer"
	echo "	<host printer>	Name of printer on <host>"
	}

if [ ! -w /usr/lib/lp ]
then
	if [ -r /usr/lib/lp ]
	then
		echo "You must be logged in as root to use this program."
		exit 1
	else
		echo "The lp spooling system is not installed on this machine."
		exit 1
	fi
fi

while getopts vi:l flag
do
	case $flag in
		v)
			# Turn on verbose flag in printer driver.
			# This turns the verbose flag on by default in the
			# printer driver.  The verbose flag may be controlled
			# in each print job by use of the -overb/-onoverb
			# options to the lp command.
			VFLAG="VERBOSE=1"
			;;
		i)
			# Set the login id to use for remote shells.
			# The hostid defaults to "lp" but may be set to
			# something else.
			HOSTID=$OPTARG
			HFLAG="HOSTID=$OPTARG"
			;;
		l)
			# Use a separate log file.
			SPECIALLOG=1
			;;
		\?)
			usage
			exit 1
			;;
	esac
done

shift `expr $OPTIND - 1`

if [ $# != 3 ] ; then
	echo "$0: Insufficient number of arguments"
	usage
	exit 1
fi

PRINTER=$1
HOSTNAME=$2
HOSTPRINTER=$3

# see if the remote host can listen before we get into the installation

echo "\nTesting connection to print server \"$HOSTNAME\"."

# chkremote returns a list of printers on the remote host
REMOTENAMES=`/usr/lib/lputil chkremote "$HOSTNAME" "$HOSTID"`


if [ $? != 0 ]
then 
    echo "\nFailed to connect to $HOSTNAME."
    echo "Make sure \"$HOSTNAME\" allows this machine to connect to it."
    echo "You may need to run \"addclient\" on $HOSTNAME."
    exit 1
fi

# Scan list of printers found on the remote machine for the one the
# user asked for.
foundit=0
set $REMOTENAMES
for i in $*
do
	if [ "$i" = "$HOSTPRINTER" ]
	then
		foundit=1
	fi
done

if [ $foundit = 0 ]
then
	echo "There is no such printer as \"$HOSTPRINTER\" on $HOSTNAME"
	echo "Known printers on $HOSTNAME are:"
	echo $*
	exit 1
fi

echo "Connection okay."

# Find the type and name of the remote printer.
IFS='
'
set -- `/usr/lib/lputil remoteinfo $HOSTNAME $HOSTPRINTER $HOSTID`
IFS=' 	
'
for i
do
	rmtype=`expr "$i" : "^TYPE=\(.*\)"`
	if [ -n "$rmtype" ]
	then
		TFLAG=TYPE=$rmtype
	else
		rmname=`expr "$i" : "^NAME=\(.*\)"`
		if [ -n "$rmname" ]
		then
			NFLAG="NAME=$rmname"
		fi
	fi
done

# Make a unique log file for this printer in /var/spool/lp/etc/log
# if requested with the -l option.  The default is to use the communal
# log file /var/spool/lp/log.

if [ "$SPECIALLOG" != "" ]
then
	PDIR=/var/lp/logs
	if test ! -d $PDIR ; then
		mkdir $PDIR
	fi

	LOGFILE=${PDIR}/${PRINTER}-log
	rm -rf ${LOGFILE}*

	cp /dev/null ${LOGFILE}
	chown lp $PDIR ${LOGFILE}
	chgrp bin $PDIR ${LOGFILE}
	chmod 664 ${LOGFILE}
	chmod 775 $PDIR
	LFLAG="LOGFILE=$LOGFILE"
	echo "Log file for printer \"$PRINTER\" will be $LOGFILE"
fi

set -- $HOSTNAME $HOSTPRINTER $VFLAG $LFLAG $HFLAG $TFLAG "$NFLAG"

for i
do
	lhs=`expr "$i" : "\(.*\)="`
	if [ -n "$lhs" ]
	then
		sc="${sc}-e 's@^${lhs}=.*@${i}@' "
	fi
done
if [ -z "$sc" ]
then
	cp $MODEL/netface /tmp/$PRINTER
else
	# recursive quoting in $sc requires this nastiness
	sh -c "sed $sc $MODEL/netface > /tmp/$PRINTER"
fi
 
/usr/lib/lp/lpadmin  -v /dev/null -i /tmp/$PRINTER -p "$PRINTER" 
rc=$?
if [ $rc != 0 ]
then
	echo "Failed to add new printer."
	exit 1
else
        # run enable and accept on $PRINTER
        /usr/sbin/enable $PRINTER > /dev/null 2>&1
        /usr/sbin/accept $PRINTER > /dev/null 2>&1
	# report what we have done
	echo "This is what you just created for ${PRINTER}:"
	/usr/lib/lp/lpstat -t
	exit 0
fi
