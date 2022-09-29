#!/bin/csh

setenv log /usr/tmp/stress$P.out
#rm -f core > /dev/null
@ repeat = 0

if ( $#argv != 1 ) then
   echo "Usage: mfgtests cycles"
   exit
endif

@ iter = 1
restart:

# use cycles to mean # times entire suite is done

while ( $iter <= $1 )
	echo "\n\nIteration $iter @ `date`\n\n"
	# run istress 1 with different proc number 

	echo""
	echo ISTRESS TEST; echo "          "; date

	@ i = 2
	while ( $i <= 7 )
	   @ i ++
	   ./istress 1 -n$i -r$1 
	end

	# run istress 1 with different sector number

	@ i = 0
	while ( $i <= 7 )
	   @ i ++
	   @ j = $i * 32
	   ./istress 1 -s$j -r$1 &
	end

	wait

	echo ""
	echo istress1 test complete; echo "      "; date

	# run istress 2 with different process number

	@ i = 2
	while ( $i <= 7 )
	   @ i ++
	   ./istress 2 -n$i -r$1 -f$i &
	end

	wait

	echo ""
	echo istress2 test complete; echo "      "; date

	echo""
	echo RAN TEST; echo "          "; date


	# run ran  with different process number and 1Mb

	@ i = 2
	while ( $i <= 10 )
	   @ i ++ 
	   ./ran  -n$i -p`expr $1 \*  100`
	end


	# run ran  with different process number and 2Mb

	@ i = 2
	while ( $i <= 10 )
	   @ i ++
	   ./ran  -n$i -p$1 -m2
	end

	echo ""
	echo ran test complete; echo "      "; date


	echo""
	echo STK TEST; echo "          "; date


	# run stk  test

	./stk $1

	echo""
	echo stk test complete; echo "      "; date

	@ iter ++
	if ( -f core ) then
	    if ( $repeat != 1 ) then
		echo ERROR - Istress: CPU $testcpu failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
		echo ERROR - Istress: Core from $corefile - repeating Istress... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@ iter --
		goto restart
	    else
		echo ERROR - Istress: CPU $testcpu failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo Istress: Failure duplicated | tee -a $log
		else
		    echo Istress: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
        endif
end

if ($repeat == 1) then
    echo Istress: Failure could not be duplicated `date` | tee -a $log
endif

rm -f /usr/tmp/istress.*
echo ISTRESS TEST COMPLETE; echo "      " ; date

exit 0
