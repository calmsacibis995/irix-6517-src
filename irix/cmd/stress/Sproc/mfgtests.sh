#! /bin/csh
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
#
# 1/21/92 created script based on runtests.sh for mfg use:
#         changed parameters for diners hardtests usemaintr; 
#         added and direct failure messages to specific file;
#         attempt to duplicate failure.
#
# Note - This script must be invoked through /usr/diags/crash/stress so
#        that 'testcpu' and 'P' are defined

setenv log /usr/tmp/stress$P.out
rm -f core > /dev/null
@ repeat = 0

# Usage: mfgtests cycles

if ($#argv != 1) then
   echo "Usage: mfgtests cycles"
   exit
endif

@ iter = 1
restart:

# use cycles to mean # times entire suite is done

while ($iter <= $1) 
	echo "\n\nIteration $iter @ `date`\n\n"
	# run timings
	./semaspeed
	./lockspeed

	./spgrp 50 5&
	./shmap 5&
	./shfile 15&
	./shpipe 18&
	./shcreate 90&
	./execit 10 5&
	./sexec 10 5&
	./doshuid 100&
	./thring 10 10&
#	./thset 4 4&
#	./usema -u 30 -n 4&

	wait

	# next round
	./pcreate  30&
	./shdma 10 20 4096&
	./sfork 20&
	./diners 10&
#	./hardtest -n 10&
	./m_test&
#	./userr&
#	./userrf&
	./saddr&
#	./shprda 10 32&

	wait

	# next round
	./lockstress&
	./lockstress -s&
	./csettest &
	./csettest -s&
#	(./alloc; ./alloc -c; ./alloc -d)&
#	./attach -n25&

	wait

#	./usema&
#	./usemapoll &
	./usemaintr &
	./usemaalloc &
	./aio1 -l10 -n 5&

	wait
	@ iter += 1
	if (-f core ) then
	    if ( $repeat != 1 ) then
		echo ERROR - Sproc: failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
		echo ERROR - Sproc: Core from $corefile - repeating Sproc... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@ iter --
		goto restart
	    else
		echo ERROR - Sproc: failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo Sproc: Failure duplicated | tee -a $log
		else
		    echo Sproc: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
	endif
end

if ($repeat == 1) then
    echo Sproc: Failure could not be duplicated `date` | tee -a $log
endif

rm -f /usr/tmp/shuid > /dev/null
rm -f /usr/tmp/usemahist > /dev/null
echo SPROC TEST COMPLETE; echo "      " ; date
exit 0
