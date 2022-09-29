#!/bin/sh

# This loops forever inducing errors while fsstress being run.
# It assumes the presence of a executable 'killemall' which
# kills all references to the filesystem being shutdown.
# It then umounts the filesystem, xfs_checks it and loops again.
#
# This expects the files errorinduce, randsleep, killemall, runall
# be present in the $XFSPATH directory.
#

set -x

RDSK=/dev/rxlv/test
DSK=/dev/xlv/test
FS=/xfs
DIR=/xfs/test
XFSPATH=/usr/stress/xfs/error
RUNTIME=300
KILLTIME=200

while /usr/bin/true
do
	echo "========================="
	umount $FS

	if [ $? != 0 ]
	then
	    banner "umount failed"
	    ps -edaf
	    exit 3
	fi

	xfs_check $RDSK

	if [ $? != 0 ]
	then
	    banner "File System Corrupted"
	    exit 3
	else
	    mount $DSK $FS
	    $XFSPATH/runall $DIR &
	    $XFSPATH/randsleep $RUNTIME

	    # set xlv errors, verbose
	    # use -k option if you want dksc driver
	    # return errors instead.
	    $XFSPATH/errorinduce -sv $RDSK
	    
	    $XFSPATH/randsleep $KILLTIME
	    $XFSPATH/killemall
	    umount $FS
	    if [ $? != 0 ]
	    then
		$XFSPATH/killemall
		$XFSPATH/randsleep $KILLTIME
		umount -k $FS
		if [ $? != 0 ]
		then
		    banner "umount failed"
		    ps -edaf
		    exit 3
		fi
	    fi
	fi
	$XFSPATH/errorinduce -uv $RDSK
	mount $DSK $FS
done	
