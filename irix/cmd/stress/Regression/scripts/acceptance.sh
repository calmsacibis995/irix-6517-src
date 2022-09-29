#!/bin/sh

/bin/rm -f core 2>/dev/null

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

# don't bother running tests unless sinead.wpd is up
check=`/usr/etc/ping -c 1 sinead.wpd | tail -1`
set $check
if [ $4 -ne 1 ]; then
	echo "Date: `date`"
	echo ""
	echo "Acceptance test cannot be run because sinead.wpd is down."
	exit 1
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
        echo "\n\nIteration $iter @ `date`\n\n"

	su root -c ./acceptance > /dev/null 2>&1
	sed '$d' /usr/tmp/accept_err.log | grep -v dump
#	fail=`tail -1 /usr/tmp/accept_err.log`
#	set $fail
#	i=0
#	while [ $i -lt $fail ]; do
#		echo "FAILED"
#		i=`expr $i + 1`
#	done

        iter=`expr $iter + 1`
done

echo "Acceptance test complete on `date`"
exit 0
