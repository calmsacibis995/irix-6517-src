#!/bin/sh
#	
# POSIX.1b Scheduling tests
#

PATH=$PATH:.:
export PATH

echo "\n\t\tPOSIX SCHEDULER TESTS\n"

echo "\npsched - scheduler interfaces \n"
psched
echo "\nsched_bench - procs \n"
sched_bench -p	-l 10000 # with procs
