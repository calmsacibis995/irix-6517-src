#!/bin/sh
# Copyright 1990, Silicon Graphics, Inc. 
# All Rights Reserved.
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
#
# $Revision: 1.42 $

#/bin/rm -f core core.* 2>/dev/null
corenum=0

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

# use cycles to mean # times entire suite is done
iter=1

#
# this seems to stress things!
#
_RLD_ARGS="-force checksum"
export _RLD_ARGS
while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"
	# run timings
	./semaspeed
	./lockspeed

	./spgrp 50 5&
	./shmap 5&
	./shfile 15&
	./shpipe 18&
	./shcreate -s 32 90&
	./shcreate -s 3200 -l  90&
	./execit 10 5&
	./sexec -l 10 -n 5&
	./sexec -o -l 10 -n 5&
	./doshuid 100&
	#./thring 10 10&
	./usema -u 30 -n 4&
	./usema -z -r -m 100000&

	wait

	# next round
	./pcreate  30&
	./shdma 10 20 4096&
	./sfork 20&
	./shpin 7&
	./diners 20&
	./hardtest -l 64 -n 10&
	./m_test&
	./userr&
	./userrf&
	./saddr&
	./shprda 10 70&

	wait

	# next round
	./lockstress&
	./lockstress -p4 -d -w4&
	./lockstress -p4 -w4&
	./lockstress -s&

	./lockstress_pt&
	./lockstress_pt -p4 -d -w4&
	./lockstress_pt -p4 -w4&
	./lockstress_pt -s&
	./locktest_pt&
	./usemarecurse_pt &

	./csettest &
	./csettest -s&
	(./alloc; ./alloc -c; ./alloc -d)&
	./attach -n25&
	./semstdio &
	./semlocale &

	wait

	./usema&
	./usemapoll &
	./usemaintr -d&
	./usemaalloc &
	./abilock&
	./cas&
	./spfirst&

	wait

	./slock_mode
	./slock_mode -d
	./slock_mode -u
	./slock_mode -u -d

	wait

	./usmalloc -z -r &
	./usmalloc &
	./sprocsp -n 30 -s 32 -g 10&
	./sprocsp -i -n 30 -s 32 -g 10&
	./shclose&
	./sfork2 -p 20 -l 5&
	_RLD_ARGS="-force checksum" ./shcreate -f 10 -p 4 20&
	./settest &
	./settest -d &

	wait 

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo SPROC TEST COMPLETE; echo "      " ; date
exit 0
