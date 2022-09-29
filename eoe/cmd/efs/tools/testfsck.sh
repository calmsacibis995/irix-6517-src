#! /bin/sh
#
# Test fsck.  Clean the argument filesystem using mkfs and the
# given proto file, then run corrupt on the filesystem, then fsck
# it to clean it
#
usage="usage: testfsck device mkfs-protofile"

if test "$#" != 2; then
	echo $usage
	exit 255
fi

device=$1
protofile=$2

set -x

mkfs $device $protofile

corrupt $device

fsck -D -y $device

# next fsck should be clean
fsck -D $device
