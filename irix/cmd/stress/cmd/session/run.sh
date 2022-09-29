#!/bin/sh

# alas - when running locally - we only have 512 ports - each rsh takes 2..
# seems like we need even more of these ports..
rm -fr /usr/tmp/M
MACH=`uname -n`
TESTDIR=/usr/stress/session
NSESSIONS=150
SLPTIME=
USAGE="Usage:$0 -v -l -u uid -U #uids -m machine -d testdir -n procs -t minutes -s slp duration"

while getopts t:U:vlu:m:d:n:s: c
do
        case $c in
        m)      MACH=$OPTARG;;
        d)      TESTDIR=$OPTARG;;
        n)      NSESSIONS=$OPTARG;;
        U)      NUID="-U $OPTARG";;
        u)      UID="-u $OPTARG";;
        t)      TIME="-t $OPTARG";;
        v)      VERBOSE="$VERBOSE -v";;
        l)      DOLOGIN=-l;;
	s)	SLPTIME=-s$OPTARG;;
        \?)     echo $USAGE; exit 2;;
        esac
done
shift `expr $OPTIND - 1`

# all tests
TEST2="$TESTDIR/s1 -c$TESTDIR $TESTDIR/s2 -c$TESTDIR $TESTDIR/s3 -d/usr/tmp/M $TESTDIR/s4 -d/usr/tmp/M $TESTDIR/s5 -c$TESTDIR $TESTDIR/s6 -d/c/tmp $TESTDIR/s7"
# all tests w/ sleep
TEST3="$TESTDIR/s1 $SLPTIME -c$TESTDIR $TESTDIR/s2 $SLPTIME -c$TESTDIR $TESTDIR/s3 $SLPTIME -d/usr/tmp/M $TESTDIR/s4 $SLPTIME -d/usr/tmp/M $TESTDIR/s5 $SLPTIME -c$TESTDIR $TESTDIR/s6 $SLPTIME -d/c/tmp $TESTDIR/s7"

TEST=$TEST3
./spawn $DOLOGIN $VERBOSE $NUID $UID -m $MACH -n $NSESSIONS $TIME \
         $TEST
