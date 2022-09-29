#!/bin/sh

/bin/rm -f core 2>/dev/null
/bin/rm -f pigpen 2>/dev/null

# Usage: runtests cycles

if test $# -ne 1 
then
    echo "Usage: runtests cycles"
    exit
fi

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]; do
    echo "\n\nIteration $iter @ `date`\n"

# run boing test
    echo ""
    echo "pig.boing test"
    pig.boing
    pig.boing
    if [ -f core ]; then
	echo "BOING TEST FAILED RUN $iter"
        mv -f core core.boing
    fi
    echo""
    echo "pig.boing completed"

# run duck test
    echo""
    echo "duck test"
    duck 2
    duck 3
    if [ -f core ]; then
	echo "DUCK TEST FAILED RUN $iter"
	mv -f core core.duck
    fi
    echo""
    echo "duck completed"

# run quit test
    echo ""
    echo "pig.quit test"
    pig.quit &
    pig.quit &
    wait
    if [ -f core ]; then
        echo "QUIT TEST FAILED RUN $iter"
	mv -f core core.quit
    fi
    echo""
    echo "quit test completed"

# run poly test
    echo ""
    echo "pig.poly test"
    i=2
    while [ $i -le 10 ];
    do
	i=`expr $i + 1`
	pig.poly -f 017 $i 1
    done
    if [ -f core ]; then
        echo "POLY TEST FAILED RUN $iter"
	mv -f core core.poly
    fi
    echo""
    echo "pig.poly test completed"

    iter=`expr $iter + 1`
done

echo ""
echo "PIGS TESTS COMPLETED"
echo ""
date
exit 0
