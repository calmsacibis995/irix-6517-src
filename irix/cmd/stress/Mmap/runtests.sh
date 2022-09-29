#! /bin/sh
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.16 $

# agfull is a bit of a pain..
OBJS="agpriv f msclose msync remap mmem m zero nfds ag noag \
	agtrunc sprocsync agsync agrand pfork addrattach madvise mstat \
	mprotect shadows aglock zeroex elfmap mapdev"
ALLOBJS="agpriv f msclose msync remap mmem m zero nfds ag noag \
	agtrunc sprocsync agsync agrand pfork addrattach madvise mstat \
	mprotect shadows aglock zeroex elfmap mapdev"

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
	for i in $OBJS
	do
		echo "	>>>	running $i	<<<"
		./$i
	done

	# now all together now!
	for i in $ALLOBJS
	do
		echo "	>>>	running $i	<<<"
		./$i&
	done
	wait
	./mmap_ftrunc -vVp 8 60

	iter=`expr $iter + 1`

	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo MMAP TEST COMPLETE; echo "      " ; date
exit 0
