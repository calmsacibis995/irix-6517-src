#! /bin/sh
#
# Simple shell script to "burn" in a filesystem
#
# Written by: Kipp Hickman
#
# $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/test/RCS/burnin.sh,v $
# $Revision: 1.1 $
# $Date: 1987/02/12 17:06:09 $
#
set -e
if test -z "$ROOT"; then
	echo "burnin.sh: set ROOT first"
	exit 255
fi
verbose=1
if test "$1"x = "-v"x; then
	verbose=0
fi
curdir=`pwd`

# run in /bin, so that "*" will work nicely
cd /bin

pass=1
mydir=$ROOT/usr/tmp/$$
while `true`; do
	if test "$verbose" = "1"; then
		echo "burnin "$$": pass "$pass" @ "`date`
		pass=`expr $pass + 1`
	fi
	#
	# Reference every existing inode in the system
	#
	ls -Rl / > /dev/null
	#
	# Copy some files around.  Make sure the copied correctly
	#
	mkdir $mydir
	cp * $mydir
	for i in *; do
		if cmp $i $mydir/$i; then
			: files are identical
		else
			echo "Compare of /bin/$i and $mydir/$i failed!"
			exit -1
		fi
	done
	rm -rf $mydir
	#
	# Make some directories
	#
	mkdir $mydir
	mkdir $mydir/grot
	mkdir $mydir/grot/grotty
	mkdir $mydir/grot/barfy
	rmdir $mydir/grot/grotty
	rmdir $mydir/grot/barfy
	rm -rf $mydir
done
