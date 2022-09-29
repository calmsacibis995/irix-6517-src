#!/bin/sh
# $Revision: 1.2 $

#
# runtests.sh with subdirectories
#

# Depth in the stress hierarchy
DEPTH=../..

#
# Name of running script
#  

RUNTESTS=./runtests

#
# Subdirectories
# 

SUBTESTS="migr pm"

#
# Usage
#

if [ $# -lt 1 ]
then
	echo "Usage: runtests -a cycles"
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

# Clean-up in current directory
/bin/rm -f core 2>/dev/null

#
# Start all the tests in subdirectories
#

for i in $SUBTESTS
do
	echo "\nSTARTING vm.numa.$i"
	if [ "$async" = "yes" ]
	then
		(cd $i ; $RUNTESTS $1 >../$DEPTH/Report/Vm.numa.$i.out 2>&1 &)
	else
		(cd $i ; $RUNTESTS $1 >../$DEPTH/Report/Vm.numa.$i.out 2>&1)
	fi
done

#
# Start all the tests in current directory
# Currently, there're no tests in this directory
#

echo VM.NUMA TEST COMPLETE; echo "      " ; date

exit 0




