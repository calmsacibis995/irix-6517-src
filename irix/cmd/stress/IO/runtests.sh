#!/bin/sh
# $Header: /proj/irix6.5.7m/isms/irix/cmd/stress/IO/RCS/runtests.sh,v 1.11 1997/09/17 18:41:34 hahn Exp $

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

	if [ -x ./diocmpall ]
	then
		./genfile /tmp/file.8m `expr 8 \* 1024 \* 1024`

		cp /tmp/file.8m /usr/tmp/diocmp.$$
		./diocmpall -D -v -s 1 -n 10 /tmp/file.8m /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$

		cp /tmp/file.8m /usr/tmp/diocmp.$$
		./diocmpall -D -v -m -s 1 -n 10 /tmp/file.8m /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$

		cp /unix /usr/tmp/diocmp.$$
		./diocmpall -s 1 -n 10 /unix /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$

		cp /unix /usr/tmp/diocmp.$$
		./diocmpall -m -s 1 -n 10 /unix /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$
#
# comment out the next test, since its not determined where
# whether we're really onto a 
# bug #484198 - O_DIRECT gets bad data w/ sproc/forks
# or there is a bug in the test
#
		cp /tmp/file.8m /usr/tmp/diocmp.$$
		./diocmpall -D -v -V -m -s 1 -n 100 /tmp/file.8m /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$

		cp /unix /usr/tmp/diocmp.$$
		./diocmpall -V -m -s 1 -n 100 /unix /usr/tmp/diocmp.$$
		rm -f /usr/tmp/diocmp.$$

		rm -f /tmp/file.8m
	fi

	iter=`expr $iter + 1`
done

echo IO TEST COMPLETE; echo "      " ; date
exit 0
