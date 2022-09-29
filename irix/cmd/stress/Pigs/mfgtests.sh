#!/bin/csh

setenv log /usr/tmp/stress$P.out
#rm -f core > /dev/null

if ( $#argv != 1 ) then
   echo "Usage: mfgtests cycles"
   exit
endif

echo " "
echo pigs test; date

# run boing test

@ repeat = 0
@ i = 0
rst_boing:

echo"";echo pig.boing test; date
while ( $i <= $1 )
    @ i ++
    pig.boing
    pig.boing
    if ( -f core ) then
	if ( $repeat != 1 ) then
	    echo ERROR - Pigs/pig.boing: CPU $testcpu failed `date` | tee -a $log
	    set corefile = `file core | awk '{print $6}'`
	    echo ERROR - Pigs/pig.boing: Core from $corefile - repeating Pigs/pig.boing... | tee -a $log
	    mv core /usr/tmp/ovendir/core.$corefile
#	    rm -f core > /dev/null
	    @ repeat = 1
	    @ i --
	    goto rst_boing
	else
	    echo ERROR - Pigs/pig.boing: CPU $testcpu failed `date` | tee -a $log
	    if ( $corefile == `file core | awk '{print $6}'` ) then
		echo Pigs/pig.boing: Failure duplicated | tee -a $log
	    else
		echo Pigs/pig.boing: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
	    endif
	    @ repeat = 0
	endif
    endif
end

if ($repeat == 1) then
    echo Pigs/pig.boing: Failure could not be duplicated `date` | tee -a $log
endif
echo"";echo pig.boing completed;date


@ repeat = 0
@ i = 0
rst_duck:

echo"";echo duck test; date
while ( $i <= $1 )
    @ i ++
    duck 2
    duck 3
    if ( -f core ) then
	if ( $repeat != 1 ) then
	    echo ERROR - Pigs/duck: CPU $testcpu failed `date` | tee -a $log
	    set corefile = `file core | awk '{print $6}'`
	    echo ERROR - Pigs/duck: Core from $corefile - repeating Pigs/duck... | tee -a $log
	    rm -f core > /dev/null
	    @ repeat = 1
	    @ i --
	    goto rst_duck
	else
	    echo ERROR - Pigs/duck: CPU $testcpu failed `date` | tee -a $log
	    if ( $corefile == `file core | awk '{print $6}'` ) then
		echo Pigs/duck: Failure duplicated | tee -a $log
	    else
		echo Pigs/duck: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
	    endif
	    @ repeat = 0
	endif
    endif
end

if ($repeat == 1) then
    echo Pigs/duck: Failure could not be duplicated `date` | tee -a $log
endif
echo"";echo duck completed;date

# run quit test

@ repeat = 0
@ i = 0
rst_quit:

echo "";echo pig.quit test; date
while ( $i <= $1 )
    @ i ++
    pig.quit& pig.quit&
    wait
    if ( -f core ) then
	if ( $repeat != 1 ) then
	    echo ERROR - Pigs/pig.quit: CPU $testcpu failed `date` | tee -a $log
	    set corefile = `file core | awk '{print $6}'`
	    echo ERROR - Pigs/pig.quit: Core from $corefile - repeating Pigs/pig.quit... | tee -a $log
	    rm -f core > /dev/null
	    @ repeat = 1
	    @ i --
	    goto rst_quit
	else
	    echo ERROR - Pigs/pig.quit: CPU $testcpu failed `date` | tee -a $log
	    if ( $corefile == `file core | awk '{print $6}'` ) then
		echo Pigs/pig.quit: Failure duplicated | tee -a $log
	    else
		echo Pigs/pig.quit: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
	    endif
	    @ repeat = 0
	endif
    endif
end

if ($repeat == 1) then
    echo Pigs/pig.quit: Failure could not be duplicated `date` | tee -a $log
endif
echo"";echo quit test completed;date

# run poly test

@ repeat = 0
@ i = 2
rst_poly:

echo "";echo pig.poly test; date
while ( $i <= 10 )
    @ i ++
    pig.poly -f 017 $i $1
    if ( -f core ) then
	if ( $repeat != 1 ) then
	    echo ERROR - Pigs/pig.poly: CPU $testcpu failed `date` | tee -a $log
	    set corefile = `file core | awk '{print $6}'`
	    echo ERROR - Pigs/pig.poly: Core from $corefile - repeating Pigs/pig.poly... | tee -a $log
	    rm -f core > /dev/null
	    @ repeat = 1
	    @ i --
	    goto rst_poly
	else
	    echo ERROR - Pigs/pig.poly: CPU $testcpu failed `date` | tee -a $log
	    if ( $corefile == `file core | awk '{print $6}'` ) then
		echo Pigs/pig.poly: Failure duplicated | tee -a $log
	    else
		echo Pigs/pig.poly: Core from `file core | awk '{print $6}'` - inconsistent failure | tee -a $log
	    endif
	    @ repeat = 0
	endif
    endif
end

if ($repeat == 1) then
    echo Pigs/pig.poly: Failure could not be duplicated `date` | tee -a $log
endif

rm -rf pigpen
echo"";echo pig.poly test completed;date

echo"";echo pigs test completed; date
exit 0


