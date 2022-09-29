#!/bin/sh
#	
# POSIX.1b memory locking tests
#

PATH=$PATH:.:
export PATH

echo "\n\t\tPOSIX MEMORY LOCK & SHARED MEMORY TESTS\n"

echo "\nmlocker - mlockall test"
mlocker
echo "\nmlocker - mlockall PM test"
mlocker -l
echo "\nmlocker - mlockall shared test"
mlocker -S
echo "\nmlockstress - stressing memory lock facilities"
mlockstress -i 4