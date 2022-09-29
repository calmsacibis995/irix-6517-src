#! /bin/sh
#
# Simple script to test some of the filesystem switch operations.  This test
# is designed to be run in parallel with itself so as to stress the boundary
# cases in the filesystem switch.
#
# Written by: Kipp Hickman
#
# $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/test/RCS/switch.sh,v $
# $Revision: 1.1 $
# $Date: 1987/02/12 17:06:10 $
#
set -e
if test -z "$ROOT"; then
	echo "switch.sh: set ROOT first"
	exit 255
fi
verbose=1
if test "$1"x = "-v"x; then
	verbose=0
fi
pass=1
mydir=$ROOT/tmp/$$
while `true`; do
	if test "$verbose" = "1"; then
		echo "switch "$$": pass "$pass" @ "`date`
		pass=`expr $pass + 1`
	fi
	#
	# Create a temporary directory to work from.
	# Make each possible class of file type.  Change their modes
	# to be inaccessable. Remove the whole mess.
	# This tests:
	#	FS_OPENI, FS_NAMEI, FS_CLOSEI, FS_STATF, FS_IPUT,
	#	FS_ACCESS, FS_SETATTR, FS_ITRUNC, FS_IUPDAT,
	#	FS_READI, FS_WRITEI
	#
	mkdir $mydir
	mknod $mydir/fifo p
	mknod $mydir/blk b 0 0
	mknod $mydir/chr c 0 0
	touch $mydir/reg
	mkdir $mydir/dir
	ln $mydir/reg $mydir/link-reg
#	ln -s $mydir/reg $mydir/symlink
	ls -l $mydir > /dev/null
	chmod 0 $mydir/*
	chmod 777 $mydir/*
	cat < /bin/ls > $mydir/reg
	if cmp /bin/ls $mydir/reg; then
		: files are identical
	else
		echo "Comparison of /bin/ls and $mydir/reg failed"
		exit -1
	fi
	rm -rf $mydir
	#
	# Recreate the directory again and make a fifo in it.
	# Run some data through the fifo.  Remove the fifo.
	#
	mkdir $mydir
	mknod $mydir/fifo p
	dd if=$mydir/fifo of=/dev/null bs=1k count=10 2> /dev/null &
	dd if=/bin/ls of=$mydir/fifo bs=1k count=10 2> /dev/null
	wait
	rm -rf $mydir
done
