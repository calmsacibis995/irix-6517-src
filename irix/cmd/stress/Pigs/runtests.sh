rm -f core

if test $# -ne 1 
then
   echo "Usage: runtests cycles"
   exit
fi

# run boing test

echo " "
echo pigs test; date

echo"";echo pig.boing test; date
i=0
while [ $i -le $1 ]
do
    i=`expr $i + 1`
    if ( test  -f core ) 
    then
       echo boing test failed run $i;date;exit 1
    fi
    ./pig.boing
    ./pig.boing
done
echo"";echo pig.boing completed;date


echo"";echo duck test; date
i=0
while [ $i -le $1 ]
do
    i=`expr $i + 1`
    if ( test  -f core ) 
    then
       echo duck test failed run $i;date;exit 1
    fi
    ./duck 2
    ./duck 3
done
echo"";echo duck completed;date

# run quit test

echo "";echo pig.quit test; date
i=0
while [ $i -le $1 ]
do
    i=`expr $i + 1`
    if ( test  -f core ) 
    then
       echo quit test failed run $i;date;exit 1
    fi
    ./pig.quit& ./pig.quit&
    wait
done
echo"";echo quit test completed;date

# run poly test

echo "";echo pig.poly test; date
# remove pigpen directory
rm -rf pigpen
i=2
while [ $i -le 10 ]
do
    i=`expr $i + 1`
    if ( test  -f core ) 
    then
       echo quit test failed ;date;exit 1
    fi
    ./pig.poly -f 017 $i $1
done

# clean up afterwards too
rm -rf pigpen

echo"";echo pig.poly test completed;date

echo"";echo pigs test completed; date

exit 0


