#!/bin/ksh
#
#	Shell wrapper for dsk.
#
# $Id: dsk.sh,v 1.1 1994/08/05 01:03:05 tin Exp $

# source in values for XFSDEV and XFSDIR
. /tmp/.envvars

PATH=.:$PATH

# cp binary to testdir
cp dsk ${XFSDIR}

cd ${XFSDIR}

# crank up the test load with each iteration

dsk -n 16 -w 1000
dsk -n 32 -w 10000
dsk -n 64 -w 100000

exit $?
