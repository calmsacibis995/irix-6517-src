#!/bin/sh

svr4net_on ()
{
    test -c /dev/tcp && (echo </dev/tcp) >/dev/null 2>&1
    return
}

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

if svr4net_on; then
	echo "\n\nINET/LOOP TLI TEST STARTING"
else
	echo "SVR4 NETWORKING NOT ENABLED; INET/LOOP TLI TEST SKIPPED"
	exit 0
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\nIteration $iter @ `date`\n\n"
	echo "Ignore errors; if networking (and the system) is still up then \c"
	echo "life is good"

	# start servers
	for i in 1 2 3 4 5 6 7 8; do
		./is -p `expr 4304 + $i` -i $SRVRINT -n 1 &
		./ls -p `expr 4304 + $i` -i $SRVRINT -n 1 &
	done

	sleep 5

	# start clients

	for i in 1 2 3 4 5 6 7 8; do
		./lc $VERBOSE -i $INTERVAL -p `expr 4304 + $i` -n 1  127.1 &
		./lcclose $VERBOSE -p 9304 -i $INTERVAL -p `expr 4304 + $i` -n 1 127.1 &
		./ic $VERBOSE -i $INTERVAL -p `expr 4304 + $i` -n 1  127.1 &
		./icclose $VERBOSE -p 9304 -i $INTERVAL -p `expr 4304 + $i` -n 1 127.1 &
	done

	wait

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

echo INET/LOOP TLI TEST COMPLETE; echo "      " ; date

exit 0
