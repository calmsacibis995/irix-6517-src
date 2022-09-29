#!/bin/sh
# $Header: /proj/irix6.5.7m/isms/irix/cmd/stress/Regression/scripts/RCS/IO.sh,v 1.1 1994/10/19 01:13:58 billd Exp $

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

	b=512
	./dkstress test1 -b$b &

	s=0x200000; o=10; e=4
	./dkstress test2 -s$s -o$o -e$e &

	s=0x500000; o=7; b=8192; e=2
	./dkstress test3 -o$o -b$b -e$e &

	wait

	./copy

	./sectorcheck -p 2
	rm 0????

	iter=`expr $iter + 1`
done

echo IO TEST COMPLETE; echo "      " ; date
exit 0
