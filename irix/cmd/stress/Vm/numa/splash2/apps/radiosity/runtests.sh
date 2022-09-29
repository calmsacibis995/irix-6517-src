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
	radiosity -p 8 -largeroom -batch
        iter=`expr $iter + 1`
done

echo "\nRADIOSITY TEST COMPLETE [`date`]"
exit 0
