#!/bin/sh
# $Revision: 1.6 $

/bin/rm -f core 2>/dev/null

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

#
# on IRIX 6 and later, run N32 versions as well as regular o32 versions,
# if N32 support is installed
#
major=`uname -r | awk -F. '{print $1}'`
N32=
N32p=.
if [ $major -ge 6 -a -r /usr/lib32/libgl.so -a -r /lib32/rld ]
then
	#
	# check to see if we're running from the build area rather the
	# installed directory; accomodate it
	#
	if [ -d N32bit -a -r N32bit/flash ]
	then
		N32p=N32bit
	else
		N32="-N32"
	fi
fi


# use cycles to mean # times entire suite is done
iter=1
time=600

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"

	x=1150; y=880; loop=6

	while test "$loop" -gt "0"
	do
		./flash -t$time -x$x -y$y &
		x=`expr $x - 20`
		y=`expr $y - 20`
		$N32p/flash$N32 -t$time -x$x -y$y &
		x=`expr $x - 20`
		y=`expr $y - 20`
		loop=`expr $loop - 2`
	done

	./winset -t$time -x950 -y880 -n6 &
	$N32p/winset$N32 -t$time -x550 -y880 -n6 &

	x=750; y=880; loop=6

	while test "$loop" -gt "0"
	do
		./badifred -t$time -x$x -y$y &
		x=`expr $x - 20`
		y=`expr $y - 20`
		$N32p/badifred$N32 -t$time -x$x -y$y &
		x=`expr $x - 20`
		y=`expr $y - 20`
		loop=`expr $loop - 2`
	done

	./mode -t$time -x730 -y300 &
	./cbuf -t$time -x730 -y395 &
	./rbuf -t$time -x730 -y490 &
	./gsync -t$time -x730 -y585 &

	$N32p/mode$N32 -t$time -x700 -y170 &
	$N32p/cbuf$N32 -t$time -x860 -y170 &
	$N32p/rbuf$N32 -t$time -x1020 -y170 &
	$N32p/gsync$N32 -t$time -x1170 -y170 &

	./worms -t$time -x850 -y490 &
	$N32p/worms$N32 -t$time -x850 -y300 &
	./worms -t$time -x1070 -y490 &
	$N32p/worms$N32 -t$time -x1070 -y300 &

	$N32p/rectwrite$N32 -t$time -x700 -y30 &
	./rectwrite -t$time -x860 -y30 &
	$N32p/rectwrite$N32 -t$time -x1020 -y30 &
	./rectwrite -t$time -x1170 -y30 &

	./bounce -t$time -x10,300 -y425,300 ./logo.bin &
	$N32p/bounce$N32 -t$time -x335,300 -y425,300 ./logo.bin &

	./parallel -t$time -x335 -y50 &
	$N32p/parallel$N32 -t$time -x10 -y50 &

	wait
	iter=`expr $iter + 1`
done

echo GRAPHICS TEST COMPLETE; echo "      " ; date
exit 0
