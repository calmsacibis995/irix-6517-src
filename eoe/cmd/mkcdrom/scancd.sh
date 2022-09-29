#!/bin/sh
# Front end for mkdisc; user level scsi WROM driver for philips CDD 521 unit.
#
# Collects the files specified to stdout, piping through mkdisc
# and specifying the size
#
# Usage: scancd <file1> <file2> ...
#

if [ $# -lt 1 ]
then
    echo usage: $0 "<file1>" ...
    exit 1
fi

MKDISC=/etc/mkdisc
DEV=/dev/scsi/sc0d7l0
SIZE=`ls -l $* | nawk '

BEGIN {
    size = 0
}
{
    size += $5
}
END {
    print size
}'`

$MKDISC -V -v -d $DEV -s $SIZE
