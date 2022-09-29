#!/bin/sh
#	
# POSIX.1b Semaphore tests
#

PATH=$PATH:.:
export PATH

echo "\n\t\tPOSIX SEMAPHORE TESTS\n"
sem_bench -u	# unnamed semaphores
sem_bench -n	# named semaphores

echo "\n\n"
sem_sbench -u -c 10000	# unnamed semaphores with sprocs
sem_sbench -a -c 10000	# arena   semaphores with sprocs
sem_sbench -n -c 10000	# named   semaphores with sprocs
sem_sbench -s -c 10000	# SystemV semaphores with sprocs

#
# No-contention case
#

echo "\n\n"
sem_speed -u	# unnamed semaphores
sem_speed -a	# arena semaphores
sem_speed -s	# svr4 semaphores
sem_speed -n	# named semaphores

mach=`uname -m`
if [ $mach = "IP19" ] || [ $mach = "IP21" ] || [ $mach = "IP25" ] || \
			 [ $mach = "IP26" ] || [ $mach = "IP27" ]
then
	echo "\n\t\tPOSIX UNNAMED SEMAPHORE ABI TESTS\n"
	if [ -x ./n32bit/sem_abi ] && [  -x ./64bit/sem_abi ]
	then
	  	sem_fieldtest
		sem_fieldtest2
		./n32bit/sem_fieldtest2
		./64bit/sem_fieldtest2
		sem_fieldtest 1
		echo "\n"

		sem_abi -c 0
		./n32bit/sem_abi -p 1000
		./64bit/sem_abi -w 500
		sem_abi -w 500
		./n32bit/sem_abi -w 1000 &
		./64bit/sem_abi -p 1000
		sem_abi -d
	else
		echo -n "WARNING: cannot execute ./n32bit/sem_abi"
		echo "and/or ./64bit/sem_abi"
	fi
fi

if [ $mach = "IP22" ] || [ $mach = "IP32" ]
then
	echo "\n\t\tPOSIX UNNAMED SEMAPHORE ABI TESTS\n"
	if [ -x ./n32bit/sem_abi ]
	then
	  	sem_fieldtest
		sem_fieldtest2
		./n32bit/sem_fieldtest2
		sem_fieldtest 1
		echo "\n"

		sem_abi -c 0
		./n32bit/sem_abi -p 500
		sem_abi -w 500
		./n32bit/sem_abi -w 500 &
		sem_abi -p 500
		./n32bit/sem_abi -d
	else
		echo -n "WARNING: cannot execute ./n32bit/sem_abi"
	fi
fi

#
# Misc. semaphore cases
#

echo "\n\n"
sem_destroy
echo "\n\n"
sem_pbench -u
sem_pbench -n
echo "\n\n"
sem_ptbench -s
sem_ptbench -n
echo "\nsem_create_destroy - test open/close/unlink operations"
sem_create_destroy
sem_create_destroy -u
sem_create_destroy -i
rm /usr/tmp/semaphore*.posix
