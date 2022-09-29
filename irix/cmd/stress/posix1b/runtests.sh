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
# $Revision: 1.3 $

#
#
# runtests.sh
#	run programs to test posix.1b functions
#		message queues
#		semaphores (named & unnamed)
#		timers and clocks
#		memory locking
#		shared memory
#		scheduling
#

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

echo "\nPOSIX_1B TEST BEGIN";

while [ "$iter" -le "$1" ]
do
	echo "\n******* Iteration $iter ******** "; date ; echo "\n\n"

	runmqtests
	runsemtests
	runtimetests
	runmemtests
	runschedtests

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

iter=`expr $iter - 1`
echo "\nPOSIX_1B TEST COMPLETE: $iter iteration(s)"; echo "      " ; date
exit 0
