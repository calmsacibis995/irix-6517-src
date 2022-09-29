#!/bin/sh

# alas - when running locally - we only have 512 ports - each rsh takes 2..
# seems like we need even more of these ports..
rm -fr /usr/tmp/M
MACH=e91
TESTDIR=/usr/stress/session

./spawn -m $MACH -n150 \
	 $TESTDIR/s1 -c$TESTDIR \
	 $TESTDIR/s2 -c$TESTDIR \
	 $TESTDIR/s3 -d/usr/tmp/M \
	 $TESTDIR/s4 -d/usr/tmp/M \
	 $TESTDIR/s5 -c$TESTDIR \
