#!/bin/sh
# $Revision: 1.8 $ 

#
# runtests.sh without subdirectories
# 

#
# Usage
#
if [ $# -ne 1 ]
then
	echo "Usage: runtests cycles"
	exit
fi

#
# Parse commande line: get number of iterations
# 
iter=1

#
# Remove core file in the current directory 
# 
/bin/rm -f core 2>/dev/null

#
# Run the tests in the current directory
# 
numnodes=0
nodes=`find /hw -name node -print`
for i in $nodes
do
    numnodes=`expr $numnodes + 1`
done

while [ "$iter" -le "$1" ]
do
       
        echo "\n\nIteration $iter @ `date`\n\n"

	echo "\tccarray"

	./ccarray $numnodes 32768 PlacementCacheColor
	./ccarray $numnodes 32768 PlacementFixed

	echo "\tcrash"
	./crash	

	for mem in $nodes
	do
	  echo "\tnmalloc $mem"
	  ./nmalloc $mem
	done

	echo "\trepfactor"
	limit stacksize 1024k
	i=0
	while test $i -le 64
	do
	  ./repfactor $i
	  let i=$i+1
	done
	
	for policy in 0 1 2 3 4 5
	do
	  for fallback in 0 1
	  do
	    for pgsz in 16 64 256 1024 4096 16384 
	    do
	      for pflag in 0 1
	      do
		for mode in 0 1
		do
		  for dsize in 1024 32768
		  do

		    echo "\tptest -x -pl$policy -fp$fallback -rp0 -mp0 -pa0 -ps$pgsz -fl$pflag -rm$mode -d$dsize -t<each-node> -n$numnodes"

		    cnode=0
		    while test $cnode -lt numnodes
		    do
		      
		      ./ptest -x -pl$policy \
		       -fp$fallback -rp0 -mp0 -pa0 \
		       -ps$pgsz -fl$pflag -rm$mode -d$dsize \
		       -t$cnode -n$numnodes &
		      let cnode=$cnode+1
		    done

		    cnode=0
		    while test $cnode -lt numnodes
		    do
		      wait
		      let cnode=$cnode+1
		    done
		    
		  done
		done
	      done
	    done
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

echo "\n\nVM.NUMA.PM test is completed @ `date`\n\n"

exit 0

