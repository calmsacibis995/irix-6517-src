#! /bin/sh
# $Revision: 1.21 $

#
# The script we use to run stress tests in subdirectories
# 
RUNTESTS=./runtests

#
# Subdirectories
# 
SUBTESTS="Pigs Istress Ipc Sproc Gfx Vm Misc IO Mmap Net posix1b Select Usock disks Tli Fdpass DV"

#
# Usage
#
if [ $# -lt 1 ]
then
	echo Usage: $0 [-a] cycles
	exit
fi

#
# Parse option
#
set - `getopt 'a' $@`
async=no

for i
do
	case $i in
	-a)	async=yes
		shift;;
	--)	shift;
		break;;
	-*)	echo "Unknown option $i"
		exit 1
	esac
done

#
# Remove all the stress test reports
#

rm -f Report/* 2>/dev/null

#
# Start tests from each subdirectory.
# Since Vm has subdirectories, we need treat it differently
# 

for i in $SUBTESTS
do
	echo "\nStarting $i"
	if [ "$async" = "yes" ]
	then
		if [ $i = "Vm" ]
		then
			(cd $i; $RUNTESTS -a $1 > ../Report/$i.out \
			2>&1 &)
		else
			(cd $i; $RUNTESTS $1 >../Report/$i.out \
			2>&1 &)
		fi
	else
		(cd $i;	$RUNTESTS $1 >../Report/$i.out 2>&1)
	fi
done

echo "Done $1 iterations"

