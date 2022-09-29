#!/bin/sh 
#
#**************************************************************************
#*
#* 		  Copyright (c) 1992 Silicon Graphics, Inc.
#*			All Rights Reserved
#*
#*	   THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
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
#* File: mksvr4pr.sh 
#*
#*
#* Description: Shell script for adding a printer to
#*	the System V Release 4 spooling system.
#*
#* usage: mksvr4pr device printer-type PRINTER
#*
#* 	where PRINTER is the local name you want for the printer,
#*	printer-type is currently one of of the printer types listed
#*	in the usage message. Where applicable, paper sizes are
#*	designated by letters and/or numbers appended to the printer type.
#*
#**************************************************************************


usage()
{
	echo "usage: mksvr4pr [-h] [-v] [-l] <device> <type> <name>"
	echo "       -h	Display the usage message"
	echo "       -v	Keep a verbose activity log"
	echo "       -l	Make a unique log file for the printer"
	echo "     <device>	Specify the printer device."
	echo "     <name>	Specify the printer's name"
}

if [ ! -w /usr/lib/lp ]
then
	if [ -r /usr/lib/lp ]
	then
		echo "You must be logged in as root to use this program."
		exit 1
	else
		echo "The SVR4 lp spooling system is not installed on this machine."
		exit 1
	fi
fi

#
# Process command options
  
while getopts hv:l flag
do
	case $flag in
		v)
			# Turn on verbose flag in the interface file.
			# This turns the verbose flag on by default in the
			# printer filter/driver. The verbose flag may be 
			# controlled in each print job by use of the 
			# -overb/-onoverb options to the lp command.
			VFLAG="VERBOSE=1"
			;;
		l)
			# Use a separate log file.
			SPECIALLOG=1
			;;
		\? | h)
			usage
			exit 1
			;;
	esac
done

shift `expr $OPTIND - 1`

# Should have  <device> <printer type> <printer name> left as arguments.
if [ $# != 3 ]
then
	echo "$0: Insufficient number of arguments"
	usage
	exit 1
fi
DEVICE=$1
MODEL=$2
PRINTER=$3
echo DEVICE $DEVICE
echo MODEL $MODEL
echo PRINTER $PRINTER

if [ ! -w $DEVICE ]
then
	echo "Device file \"$DEVICE\" not found. The selected device"
	echo "may not be supported on this machine."
fi

MODELDIR=/etc/lp/model

# Test for existence of model file
if [ ! -r $MODELDIR/$MODEL ] ; then
   echo "Model file \"$MODEL\" not found. The selected printer"
   echo "may not be supported on this machine."
   exit 1
fi

# Make a unique log file for this printer in /var/lp/logs
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
# Assemble substitution arguments so they can be done in
# one sed command.
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
	cp $MODELDIR/$MODEL /tmp/$PRINTER
else
	# recursive quoting in $sc requires this nastiness
        sh -c "sed $sc $MODELDIR/$MODEL  > /tmp/$PRINTER"
fi
 
/usr/lib/lp/lpadmin -p "$PRINTER" -v "$DEVICE" -i "/tmp/$PRINTER"
rc=$?
if [ $rc != 0 ]
then	
rm -f /tmp/$PRINTER
	echo "Failed to add new printer."
	exit 1
else
rm -f /tmp/$PRINTER
	# run enable and accept on $PRINTER
	/usr/sbin/enable $PRINTER > /dev/null 2>&1
	/usr/sbin/accept $PRINTER > /dev/null 2>&1
	# report what we have done
	echo "This is what you just created for ${PRINTER}:"
	/usr/lib/lp/lpstat -t
	exit 0
fi
