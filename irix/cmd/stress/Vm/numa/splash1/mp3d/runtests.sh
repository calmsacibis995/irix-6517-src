#!/bin/sh
# $Revision: 1.2 $

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
	echo "10000\ne" | ./mp3d 250000 4
        iter=`expr $iter + 1`
done

echo "\nMP3D TEST COMPLETE [`date`]"
exit 0
