#!/bin/sh
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.39 $

#/bin/rm -f core core.* 2>/dev/null
corenum=0

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

PATH=$PATH:.:
export PATH
# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter\n\n"
	gettime
	mal 4
	amal 4
	libmal 4
	echo "Time malspeed 100000"
	time malspeed 100000
	echo "Time amalspeed 100000"
	time amalspeed 100000
	echo "Time libmalspeed 100000"
	time libmalspeed 100000
	aremal -l 4
	aremal -l 4 -s
	trunc 20 verbose

	# a bunch of malloc tests
	achk&
	libchk&
	wait

	# test gettimeofday(), UST, and cycle counter clocks
	testclocks

	# if timed/ntpd is running, testclocks suspends it. 
        # testclocks continues it on successful completion.
        # in case testclocks crashed and timed/ntpd was running:
	killall -CONT timed
	killall -CONT ntpd

	librecalloc
	arecalloc -s 20000000
	mtest -l 20 -n5 -p10000&
	libmtest -l 20 -n5 -p10000&
	wait

	# some more malloc tests
	libmemalign
	amemalign
	# ATT memalign fails ...
	#memalign
	re
	libre


	# something more STRESSFUL
	mtest -l 20 -b 64000 -f 100 -n5&
	rt -r5 -w4 -l2&
	mpin -l 10 -n 20&
	fchk -f 100 -n 800 &
	slinkchk -c2 -l40 -s23010 &
	dirchk -p /tmp -f 100 -d 100&
	mandflock&
	vmlimit&
	wait

	trunc 30 &
	watch &
	lockstep &
	watchread &
	procwrite -v -m 1000 -l 100 -s &
	procwrite -v -m 1000 -l 100 -s -p &
	ptwatch &
	sigwatch &
	procpoll -u &
	flock&
	fcntl -l&
	flimit&
	fds -n100 -p20&
	wait

	strfds -n100 -p20&
	strfds -h&
	aremal -g&
	wait

	fetchop&
	fds -h&
	flimitquark&
	wait

	rm -f 0[0-2][0-9][0-9]
	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo MISC TEST COMPLETE; echo "      " ; date
exit 0
