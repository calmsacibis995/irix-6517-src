#!/bin/sh
# Front end for mkdisc; user level scsi CD-R driver
#
# Collects the files specified to stdout, piping through mkdisc
# and specifying the size
# the images to be written to the CD-R must have already been
# created (see make_efs_image).

if [ $# -lt 2 ]
then
    echo usage: $0 directory cd-r_device
    exit 1
fi
directory="$1"
device="$2"

MKDISC=/etc/mkdisc

# be sure we don't get in the wrong order; I've done that...
files="cdrom.vh cdrom.efs"

set -e
cd $directory
set +e

SIZE=`ls -l $files | nawk '

BEGIN {
    size = 0
}
{
    size += $5
}
END {
    print size
}'`

cat $files | $MKDISC -v -W -d $device -s $SIZE
