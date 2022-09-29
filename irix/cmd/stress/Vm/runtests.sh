#!/bin/sh
# $Revision: 1.3 $

#
# Subdirectories
# 

# Depth in the stress hierarchy
DEPTH=..

# Name of running script
RUNTESTS=./runtests

# Subdirectories
# Only run numa tests on a Lego machine
MACHTYPE=`uname -m`
if [ $MACHTYPE = "IP27" ] 
then
	SUBTESTS="lpg generic numa"
else 
	SUBTESTS="lpg generic"
fi

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

#
# Start all the tests in subdirectories in order to run them with the 
# tests in current directory asynchornously
#

for i in $SUBTESTS
do
	echo "\nSTARTING Vm.$i"
	if [ "$async" = "yes" ]
	then
		(cd $i; $RUNTESTS -a $1 >../$DEPTH/Report/Vm.$i.out 2>&1 &)
	else
		(cd $i; $RUNTESTS $1 >../$DEPTH/Report/Vm.$i.out 2>&1)
	fi
done

echo VM TEST COMPLETE; echo "      " ; date

exit 0
