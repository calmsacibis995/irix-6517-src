#!/bin/csh

setenv log /usr/tmp/stress$P.out
#rm -f core > /dev/null
@ repeat = 0

# Usage: mfgtests cycles

if ($#argv != 1) then
   echo "Usage: mfgtests cycles"
   exit
endif

@ iter = 1
restart:

# use cycles to mean # times entire suite is done

while ( $iter <= $1 )
	time ./nwalk &
	/bin/sh ./dops
	wait

	# use mapped files - turned off due to core dumps
	# TMPDIR=. time ./nmwalk &
	# /bin/sh ./dops

	# for fun one can try to do nmwalk over NFS! 
	wait
	@ iter ++
	if ( -f core ) then
	    if ( $repeat != 1 ) then
		echo ERROR - Vm: failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
		echo ERROR - Vm: Core from $corefile - repeating Vm... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@iter --
		goto restart
	    else
		echo ERROR - Vm: failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo Vm: Failure duplicated | tee -a $log
		else
		    echo Vm: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
        endif
end

if ($repeat == 1) then
    echo Vm: Failure could not be duplicated `date` | tee -a $log
endif

echo VM TEST COMPLETE; echo "      " ; date
exit 0
