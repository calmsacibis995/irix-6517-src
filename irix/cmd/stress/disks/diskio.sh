#!/bin/sh
#	To adjust these parameters, wrap this script in another one and
#	reset the values as you wish, and export them to this script.  This
#	insures that the default values remain the same, to serve as a
#	referencee.
#
if [ "$DEBUG" != "" ]
then
	set -x
fi

#
#	Multi-user numbers.  Usually smaller number of repeats and smaller
#	file size, since there will be multiple processes running.
#
MREPEATS=${MREPEATS:-5}
MPROCS=${MPROCS:-5}
MSIZE=${MSIZE:-0x40000}
RAWMREPEATS=${RAWMREPEATS:-5}
RAWMPROCS=${RAWMPROCS:-5}
RAWMSIZE=${RAWMSIZE:-0x40000}

#
#	Single-user numbers.  Usually larger to insure that the entire
#	buffer cache is blown out.
#
SREPEATS=${SREPEATS:-10}
SSIZE=${SSIZE:-0x300000}
RAWSREPEATS=${RAWSREPEATS:-10}
RAWSSIZE=${RAWSSIZE:-0x300000}

#
#	Other constants.
#
DISKIO=diskio
VERBOSE=

VERBOSE=-v
if [ "$1" = "-V" ]
then
	VERBOSE=
	shift;
fi
if [ "$VOLUME" != "" -a "$VOLUME" != "." ]
then
	cp diskio.sh diskio $VOLUME
	if [ $? -ne 0 ]
	then
		exit 1
	fi
	cd $VOLUME
	VOLUME=
	export VOLUME
	exec diskio.sh
fi

echo "*** DISKIO Benchmark `date`"
egrep "\/dsk\/" < /etc/fstab > /usr/tmp/$$a12
sed 's/^/\*\*\* /' < /usr/tmp/$$a12
echo "*** working directory: `pwd`"
rm /usr/tmp/$$a12

if [ "$1" != "nofs" ]
then
	echo "*** BEGIN READ single-user FS benchmarks ====================="

	echo "read fs single: default blocksizes"
	$DISKIO $VERBOSE -n1 -r$SREPEATS -s$SSIZE

	echo "*** BEGIN READ multi-user FS benchmarks ======================"

	echo "read fs multi: multi-user synced sizes"
	$DISKIO $VERBOSE -r$MREPEATS -n$MPROCS -s$MSIZE -1
	echo "read fs multi: multi-user unordered sizes"
	$DISKIO $VERBOSE -r$MREPEATS -n$MPROCS -s$MSIZE -2

	echo "*** BEGIN WRITE single-user FS benchmarks ===================="

	echo "write fs single: default blocksizes"
	$DISKIO $VERBOSE -d -n1 -r$SREPEATS -s$SSIZE

	echo "*** BEGIN WRITE multi-user FS benchmarks ====================="

	echo "write fs multi: multi-user synced sizes"
	$DISKIO $VERBOSE -d -r$MREPEATS -n$MPROCS -s$MSIZE -1
	echo "write fs multi: multi-user unordered sizes"
	$DISKIO $VERBOSE -d -r$MREPEATS -n$MPROCS -s$MSIZE -2

	echo "*** END FS benchmarks ========================================"
fi

if [ "$RAW" != "" ]
then
	if [ ! -r $RAW ]
	then
		echo "*** ERROR: can't read raw disk specified for test"
		exit 1
	fi
	echo "*** BEGIN RAW disk tests - using $RAW"
	echo "*** BEGIN READ raw access single-user benchmarks ============="

	echo "read raw single: non-aligned buffer default sizes"
	$DISKIO $VERBOSE -p$RAW -n1 -r$RAWSREPEATS -s$RAWSSIZE
	echo "read raw single: aligned buffer default sizes"
	$DISKIO $VERBOSE -p$RAW -n1 -a -r$RAWSREPEATS -s$RAWSSIZE

	echo "*** BEGIN READ raw access multi-user benchmarks ============="

	echo "read raw multi: non-aligned default blocksizes in sync"
	$DISKIO $VERBOSE -p$RAW -n$RAWMPROCS -r$RAWMREPEATS -s$RAWMSIZE -1
	echo "read raw multi: non-aligned default blocksizes unordered"
	$DISKIO $VERBOSE -p$RAW -n$RAWMPROCS -r$RAWMREPEATS -s$RAWMSIZE -2
	echo "read raw multi: aligned buffer default blocksizes in sync"
	$DISKIO $VERBOSE -p$RAW -n$RAWMPROCS -a -r$RAWMREPEATS -s$RAWMSIZE -1
	echo "read raw multi: aligned buffer default blocksizes unordered"
	$DISKIO $VERBOSE -p$RAW -n$RAWMPROCS -a -r$RAWMREPEATS -s$RAWMSIZE -2

	echo "*** END raw access benchmarks =============================="
fi

echo "*** DISKIO Benchmark Complete `date`"
