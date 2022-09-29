#!/bin/sh
# copy dir tree back and forth for 1000 iterations
#

# source in values for XFSDEV and XFSDIR
. /tmp/.envvars

VARDIR=/var
TMPDIR=/tmp

WAITTIME=5
LOOPCOUNT=1

while [ $LOOPCOUNT -le 1000 ]
do
	echo LOOP $LOOPCOUNT
	echo

	(
		echo Copying $VARDIR to $XFSDIR
		cd $VARDIR
		find . -depth -print | cpio -pvdm $XFSDIR >/dev/null 2>&1
	) &

	find . -name 'foobar' -depth -print &

	echo Waiting $WAITTIME seconds && sleep $WAITTIME

	(
		echo Copying $XFSDIR to $TMPDIR
		cd $XFSDIR
		find . -depth -print | cpio -pvdm $TMPDIR >/dev/null 2>&1
	) &

	echo Waiting $WAITTIME seconds && sleep $WAITTIME

	wait

	echo
	echo '---------------------------------------------'
	echo
	LOOPCOUNT=`expr $LOOPCOUNT + 1`
done

exit 0
