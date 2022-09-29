#!/bin/sh

#rm -f core
corenum=0

# Usage: runtests cycles

if test $# -ne 1
then
   echo "Usage: runtests cycles"
   exit
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\nIteration $iter @ `date`\n\n"

	./sem0& ./sem1& ./sem2& ./sem3 1000 &
	wait
	./msg1&  ./msg2& ./msg0 -f $iter& 
	wait
	./shm0 -f& ./shm1& ./shm4& ./shm7&
	wait

	./sem0& ./sem1&
	./msg1&  ./msg2& ./msg0 -f 20&
	./shm0 -f&
	./shm1&
	./shm3 -i100 -p 5&
	./shm4&
	./shm7&
	wait

	wait

	iter=`expr $iter + 1`
	if [ -f core ]
	then
		banner "CORE FILE FOUND!"
		mv core core.$corenum
		corenum=`expr $corenum + 1`
	fi
done

echo IPC TEST COMPLETE; echo "      " ; date

exit 0
