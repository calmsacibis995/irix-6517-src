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

echo "\n\nDATA VALIDATION TEST STARTING"

# use cycles to mean # times entire suite is done
iter=1

freeK=`df -kl /usr/tmp | tail -1l | awk '{print $5}'`
if [ $freeK -lt 65536 ]; then
	echo '*** Insufficient free disk space for data validation test ***'
	echo '*** currently we think we need 64MB to run this test ***'
	exit 1
fi

dir=/usr/tmp/netdv
(
if [ ! -d $dir ]; then
	mkdir -p $dir
	cp -r * $dir
	cd $dir;
	./genfiles
fi
)

while [ "$iter" -le "$1" ]
do
	echo "\nIteration $iter @ `date`\n\n"

	ncpu=`mpadmin -u | wc -l`
#	echo "ncpu=$ncpu"
	# start servers

	odir=`pwd`
	cd $dir
	./us -n 2 -i $SRVRINT &
	./s -n 2 -i $SRVRINT &
	sleep 5

	# start clients

	./c -i $INTERVAL -n 2 127.1 &
	./uc -i $INTERVAL -n 2 &

	sleep $SRVRINT
	wait

	if [ -f core ]
	then
		banner "CORE FILE FOUND in $dir!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
	cd $odir
	iter=`expr $iter + 1`
	killall ./us
	killall ./s
	sleep 2
done

if [ ! -f $dir/core.* ]; then
	rm -rf $dir
fi

echo DATA VALIDATION TEST COMPLETE; echo "      " ; date

exit 0
