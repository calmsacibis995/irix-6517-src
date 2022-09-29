#!/bin/sh
# Front end for mkdisc
#
# Collects the files specified to stdout, piping through mkdisc
# and specifying the size
#
# Usage: burncd <file1> <file2> ...
#

if [ $# -lt 1 ]
then
    echo usage: $0 "<file1>" ...
    exit 1
fi

#MKDISC=/usr/etc/mkdisc
MKDISC=/usr/people/rogerc/makecd/mkdisc/mkdisc
DEV=/dev/scsi/sc0d5l0
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

cat $* | $MKDISC -d $DEV -s $SIZE
