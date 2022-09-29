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

echo "\n\nUNIX DOMAIN TEST STARTING"

# use cycles to mean # times entire suite is done
iter=1

ncpus=`mpadmin -u | wc -l`
echo "ncpus = " $ncpus

while [ "$iter" -le "$1" ]
do
	echo "\nIteration $iter @ `date`\n\n"

	# start servers
	./s -i $SRVRINT -n 16 &

	for i in 9001 9002 9003 9004 9005 9006 9007 9008 9009 9010 9011; do
		./udgrecv -i $SRVRINT /tmp/$i &
	done &
	for i in 8001 8002 8003 8004 8005 8006 8007 8008 8009 8010 8011; do
		./udgclose -i $SRVRINT /tmp/$i &
	done &
	sleep 5

	# start clients

	for i in 9001 9002 9003 9004 9005 9006 9007 9008 9009 9010 9011; do
		./udgabuse $i $INTERVAL &
	done &
	./c $VERBOSE -i $INTERVAL -n 8 &
	./cclose $VERBOSE -i $INTERVAL -n 8 &
	./sp -n 2 -i $INTERVAL &
	./sp -d -n 2 -i $INTERVAL &

	sleep $SRVRINT

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo UNIX DOMAIN SOCKET TEST COMPLETE; echo "      " ; date

exit 0
