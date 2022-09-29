#!/bin/sh

# source in values for XFSDEV and XFSDIR
if [ -f /tmp/.envvars ]
then
	. /tmp/.envvars
fi

# find out how many blks we need to create to overflow fs
NBLKS=`df | grep ${XFSDIR} | awk '{ print $3; }'`
if [ $NBLKS -lt 1 ]
then
	NBLKS=1024
fi

if [ -x creattest ]
then
	cp creattest ${XFSDIR}
else
	exit 1
fi

# assuming 8k blocks; it is ok if we overestimate
cd ${XFSDIR}
creattest 8192 $NBLKS

# there was one panic caused by copying a file to a full fs
cp /unix .

exit 0
