#!/bin/csh

setenv log /usr/tmp/stress$P.out
#rm -f core > /dev/null
@ repeat = 0

# Usage: mfgtests cycles

if ($#argv != 1) then
   echo "Usage: mfgtests cycles"
   exit
endif

echo "AF_UNIX stress not enabled for manufacturing yet."
exit 0

@ iter = 1
restart:

# use cycles to mean # times entire suite is done

while ( $iter <= $1 )
	echo "\n\nIteration $iter @ `date`\n\n"

	./sem0& ./sem1&
	wait
	./msg1&  ./msg2& ./msg0 -f $iter& 
	wait
	./shm0 -f

	@ iter ++
	if ( -f core ) then
	    if ( $repeat != 1 ) then
		echo ERROR - Ipc: CPU $testcpu failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
		echo ERROR - Ipc: Core from $corefile - repeating Ipc... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@ iter --
		goto restart
	    else
		echo ERROR - Ipc: CPU $testcpu failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo Ipc: Failure duplicated | tee -a $log
		else
		    echo Ipc: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
        endif
end

if ($repeat == 1) then
    echo Ipc: Failure could not be duplicated `date` | tee -a $log
endif

echo IPC TEST COMPLETE; echo "      " ; date

exit 0
