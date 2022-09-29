#! /bin/csh


set OBJS="agpriv f msclose msync remap mmem m zero nfds ag noag agtrunc sprocsync agsync agrand pfork addrattach madvise mstat"


set ALLOBJS="agpriv f msclose msync remap mmem m zero nfds ag noag agtrunc sprocsync agsync agrand pfork addrattach madvise mstat"

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
	echo "\n\nIteration $iter @ `date`\n\n"
	foreach i ( $OBJS )
		echo "	>>>	running $i	<<<"
		./$i
	end

	# now all together now!
	foreach i ( $ALLOBJS )
		echo "	>>>	running $i	<<<"
		./$i&
	end
	wait
	@ iter ++
	if ( -f core ) then
	    if ( $repeat != 1 ) then
		echo ERROR - Mmap: CPU $testcpu failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
		echo ERROR - Mmap: Core from $corefile - repeating Mmap... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@ iter --
		goto restart
	    else
		echo ERROR - Mmap: CPU $testcpu failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo Mmap: Failure duplicated | tee -a $log
		else
		    echo Mmap: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
        endif
end

if ($repeat == 1) then
    echo Mmap: Failure could not be duplicated `date` | tee -a $log
endif

echo MMAP TEST COMPLETE; echo "      " ; date
exit 0
