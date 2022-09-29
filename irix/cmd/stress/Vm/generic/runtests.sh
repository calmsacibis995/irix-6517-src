#!/bin/sh
# $Revision: 1.5 $

#
# runtests.sh with subdirectories
# 

# Depth in the stress hierarchy
DEPTH=../..

#
# Usage
#

if [ $# -ne 1 ]
then
	echo "Usage: runtests cycles"
	exit
fi

#
# Remove core file in the current directory 
# 
/bin/rm -f core 2>/dev/null
/bin/rm -f scr*

#
# Start all the tests in current directory
#

iter=1

while [ "$iter" -le "$1" ]
do

	time ./nwalk &
	/bin/sh ./dops
	wait

	# use mapped files
	TMPDIR=. time ./nmwalk &
	/bin/sh ./dops

	# for fun one can try to do nmwalk over NFS! 
	wait

	iter=`expr $iter + 1`
done

/bin/rm -f scr*
echo "\n\nVM.GENERIC test is completed @ `data`\n\n"

exit 0
