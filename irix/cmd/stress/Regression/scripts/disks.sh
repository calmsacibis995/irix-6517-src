#!/bin/sh

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
    echo "\n\nIteration $iter @ `date`\n\n"

    diskio.sh

    iter=`expr $iter + 1`
done

echo "Disk IO tests complete"
echo ""
date
exit 0
