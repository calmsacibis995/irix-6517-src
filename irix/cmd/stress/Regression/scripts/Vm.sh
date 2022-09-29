#!/bin/sh
# $Revision: 1.1 $

/bin/rm -f core 2>/dev/null

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n"

	./nwalk &
#	/bin/sh ./dops
	wait

	# use mapped files
	TMPDIR=.
	./nmwalk &
#	/bin/sh ./dops
	wait

	iter=`expr $iter + 1`
done

echo "VM TEST COMPLETE @ `date`"
echo ""
exit 0
