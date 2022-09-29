#!/bin/sh
#
# Program to do regression testing on iso 9660 CDs
#
# Mount the cd, then run "sh testcd.sh /CDROM".
#
# After it's done, mount_iso9660 should still be running (not having
# dumped core), and there should be no error messages in the output
#

if [ $# -ne 1 ]
then
	echo usage: $0 "<mount point>"
	exit
fi

echo ==== dir test ====
find $1 -print

echo ==== file test ====
find $1 -type f -print -exec cp {} /dev/null \;

echo ==== parent test ====
for dir in `find $1 -type d -print`
do
	(cd $dir; pwd)
done
