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
	echo "\n\nIteration $iter @ `date`\n\n"

	set b = 512
	./dkstress test1 -b$b &

	set s = 0x200000;  set o = 10;  set e = 4
	./dkstress test2 -s$s -o$o -e$e &

	set s = 0x500000; set o = 7; set b = 8192; set e = 2
	./dkstress test3 -o$o -b$b -e$e &

	wait

	# make only 2 copies of /unix to avoid running out of space
	./copy /usr/tmp 2

	mkdir /usr/tmp/ovendir/I$testcpu$P
	./sectorcheck -d /usr/tmp/ovendir/I$testcpu$P -c 20 -p 2
	rm -rf /usr/tmp/ovendir/I$testcpu$P

	@ iter ++
	if ( -f core ) then
	    if ( $repeat != 1 ) then
                echo ERROR - IO: CPU $testcpu failed `date` | tee -a $log
		set corefile = `file core | awk '{print $6}'`
                echo ERROR - IO: Core from $corefile - repeating IO... | tee -a $log
		mv core /usr/tmp/ovendir/core.$corefile
#		rm -f core > /dev/null
		@ repeat = 1
		@ iter --
		goto restart
	    else
                echo ERROR - IO: CPU $testcpu failed `date` | tee -a $log
		if ( $corefile == `file core | awk '{print $6}'` ) then
		    echo IO: Failure duplicated | tee -a $log
		else
		    echo IO: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
		endif
		@ repeat = 0
	    endif
	endif
end

if ($repeat == 1) then
    echo IO: Failure could not be duplicated `date` | tee -a $log
endif

echo IO TEST COMPLETE; echo "      " ; date
exit 0
