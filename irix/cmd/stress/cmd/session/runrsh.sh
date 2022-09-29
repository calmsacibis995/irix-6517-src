#!/bin/sh

# alas - when running locally - we only have 512 ports - each rsh takes 2..
# seems like we need even more of these ports..

#
# NOTE: to use this and test s6 you need to have set up a processor set
# mapping. See psettab
#
#
# Also - supset will need to be able to become root
#
rm -fr /usr/tmp/M
MACH=e91
TESTDIR=/usr/stress/session

./spawn -m $MACH -n100 \
	 $TESTDIR/s1 -c$TESTDIR -poct1 \
	 $TESTDIR/s2 -c$TESTDIR -poct2 \
	 $TESTDIR/s3 -d/usr/tmp/M \
	 $TESTDIR/s4 -d/usr/tmp/M \
	 $TESTDIR/s5 -c$TESTDIR \
	 $TESTDIR/s6 -c$TESTDIR
