#!/bin/sh
#	
# POSIX.1 Timer and Clock Tests
#

PATH=$PATH:.:
export PATH

echo "\n\t\tPOSIX TIMER/CLOCK TESTS\n"
ptimer
echo "\n\t\tPOSIX TIMER/REALTIME/MEMORY LOCKING TEST\n"
ptimer_rtsched
ptimer_absclock