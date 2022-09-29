#!/bin/sh

rm -f core

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"
	# run istress 1 with different proc number 

	echo""
	echo ISTRESS TEST; echo "          "; date

	i=2
	while [ $i -le 7 ]
	do
	   i=`expr $i + 1`
	   ./istress 1 -n$i -r$1 
	done

	# run istress 1 with different sector number

	i=0
	while [ $i -le 7 ]
	do
	   i=`expr $i + 1`
	   j=`expr $i \* 32`
	   ./istress 1 -s$j -r$1 &
	done

	wait

	rm -f /usr/tmp/istress.* /tmp/istress.*

	echo ""
	echo istress1 test complete; echo "      "; date

	# run istress 2 with different process number

	i=2
	while [ $i -le 7 ]
	do
	   i=`expr $i + 1`
	   ./istress 2 -n$i -r$1 -f$i &
	done

	wait

	echo ""
	echo istress2 test complete; echo "      "; date

	echo""
	echo RAN TEST; echo "          "; date


	# run ran  with different process number and 1Mb

	i=2
	while [ $i -le 10 ]
	do
	   i=`expr $i + 1`
	   ./ran  -n$i -p`expr $1 \*  100`
	done


	# run ran  with different process number and 2Mb

	i=2
	while [ $i -le 10 ]
	do
	   i=`expr $i + 1`
	   ./ran  -n$i -p$1 -m2
	done

	echo ""
	echo ran test complete; echo "      "; date


	echo""
	echo STK TEST; echo "          "; date


	# run stk  test

	./stk $1

	echo""
	echo stk test complete; echo "      "; date

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
	fi
done

echo ISTRESS TEST COMPLETE; echo "      " ; date

exit 0
