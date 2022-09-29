#!/bin/sh

/bin/rm -f core 2>/dev/null

# Usage: runtests cycles
# Note: DISPLAY environment variable must be set upon entry

if [ "X$DISPLAY" = X ]; then
    echo "The DISPLAY environment variable must be set to run the X tests"
    exit
fi

if test $# -ne 1
then
    echo "Usage: runtests cycles"
    exit
fi

iter=1

while [ "$iter" -le "$1" ]
do
    # Validate core X rendering
    x11perf -all -time 1 -repeat 1 > /dev/null

    # ReadDisplay extension
    snoop

    # IRIX4 binary compatibility
    xdpyinfo.irix401 > /dev/null

    # XBell beeping
    beep

    # Multi-buffering touch test
    xmbufinfo > /dev/null

    # X stress testing
    xstress -clients 85 xlogo

    # SGI-NewMultibuffering extension
    nmbxinfo > /dev/null

    # Random core X rendering touch tests via xcrash
    xcrash -tests 150 | grep ERROR

    # Overlay windows support
    xlayerinfo > /dev/null

    # XInput extension
    xlist > /dev/null

    iter=`expr $iter + 1`
done

echo "X TESTS COMPLETE"
echo ""
date

exit
