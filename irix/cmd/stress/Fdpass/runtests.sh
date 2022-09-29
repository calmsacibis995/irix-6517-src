#!/bin/sh

#rm -f core
corenum=0

# hack for testing script
if [ X$INTERVAL = X ]; then
	INTERVAL=1800
fi

if [ X$VERBOSE != X ]; then
	VERBOSE=-v
	export VERBOSE
fi

# fudge
SRVRINT=`expr $INTERVAL + 30`

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

echo "\n\nUNIX DOMAIN FILE DESCRIPTOR PASSING TEST STARTING"

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\nIteration $iter @ `date`\n\n"

	# start servers
	./sp -n 5 -i $INTERVAL >/dev/null &
	./sp -d -n 5 -i $INTERVAL >/dev/null &

	wait

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo FILE DESCRIPTOR PASSING TEST COMPLETE; echo "      " ; date

exit 0
