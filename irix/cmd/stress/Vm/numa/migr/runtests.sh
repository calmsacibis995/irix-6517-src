#!/bin/sh
# $Revision: 1.3 $ 

#
# runtests.sh without subdirectories
# 

# Depth
DEPTH=../../..

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

numnodes=0
nodes=`find /hw -name node -print`
for i in $nodes
do
    numnodes=`expr $numnodes + 1`
done

#
# Run the tests in the current directory
# 

iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"

	# mapcnt
	echo "mapcnt"
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    ./mapcnt $thread $mem 0&
	  done
	done
	
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    wait
	  done
	done

	echo "mcontrol"
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    ./mcontrol $thread $mem&
	  done
	done
	
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    wait
	  done
	done

	echo "muser"
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    for dest in $nodes
	    do
	      ./muser $thread $mem $dest&
	    done
	  done
	done
	
	for mem in $nodes
	do
	  for thread in $nodes
	  do
	    wait
	  done
	done

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		exit
	fi
done

wait 

echo "\n\nVM.NUMA.MIGR test is completed @ `date`\n\n"

exit 0
