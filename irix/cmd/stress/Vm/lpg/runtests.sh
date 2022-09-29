#! /bin/sh

#**************************************************************************
#*									  *
#* 		 Copyright (C) 1989 Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.1 $

OBJS=lpgfault
NUMITER=1000

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

#/bin/rm -f core core.* 2>/dev/null
corenum=0

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"
	./$OBJS -o fault -i $NUMITER &
	./$OBJS -o sproc -i $NUMITER &
	./$OBJS -o map_local -i $NUMITER &
	./$OBJS -o stack -i $NUMITER &
	./$OBJS -o fork -i $NUMITER &
	./$OBJS -o shm -i $NUMITER &
	./$OBJS -o mmap -i $NUMITER &
	./$OBJS -o msync -i $NUMITER &
	wait
	iter=`expr $iter + 1`

	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo LARGE PAGE TEST COMPLETE; echo "      " ; date
exit 0
