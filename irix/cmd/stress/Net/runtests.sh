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

echo "\n\nTCP/UDP TEST STARTING"

# use cycles to mean # times entire suite is done
iter=1

ncpu=`mpadmin -u | wc -l`
echo "ncpu = " $ncpu

while [ "$iter" -le "$1" ]
do
	echo "\nIteration $iter @ `date`\n\n"
	echo "Ignore errors; if networking (and the system) is still up then \c"
	echo "life is good"

	# start servers
	./mute -i $SRVRINT &
	./s -i $SRVRINT -n 16 &
	./ab -p 9304 -i $SRVRINT -n 16 &

	for i in 9001 9002 9003 9004 9005 9006 9007 9008 9009 9010 9011; do
		./udprecv -i $SRVRINT $i &
	done &
	for i in 8001 8002 8003 8004 8005 8006 8007 8008 8009 8010 8011; do
		./udpclose -i $SRVRINT $i &
	done &
	sleep 5

	# start clients

	for i in 9001 9002 9003 9004 9005 9006 9007 9008 9009 9010 9011; do
		./udpabuse 127.1 $i $INTERVAL &
	done &
	./c $VERBOSE -i $INTERVAL -n 8 127.1 &
	./cclose $VERBOSE -i $INTERVAL -n 8 127.1 &
	./c $VERBOSE -p 9304 -i $INTERVAL -n 8 127.1 &
	./cclose $VERBOSE -p 9304 -i $INTERVAL -n 8 127.1 &
	./snd -i $INTERVAL 127.1 &

	sleep $SRVRINT

	#
	# send ten packets to the echo port and see whether the network is
	# still up

	if ./netver -h 127.1 -p 7 -C 10 ; then
		echo
		echo "Networking survived."
		echo
	else
		echo
		echo '*** Networking died. ***'
		echo
	fi

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo TCP/UDP TEST COMPLETE; echo "      " ; date

exit 0
