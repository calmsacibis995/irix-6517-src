#!/bin/sh
#
# POSIX.1b message queue tests
#

PATH=$PATH:.:
export PATH

#
# Low stress tests
#

echo "\n\t\tPOSIX MESSAGE QUEUE TESTS - LOW STRESS\n"

mqtest
mqtest_sproc
mqtest_pthread
echo " "
mq_pro

#
# Something more STRESSFUL
#

echo "\n\t\tPOSIX MESSAGE QUEUE TESTS - MEDIUM STRESS\n"

mqtest_sproc -n 40 -s 16384 -m 50 -t 3
mqtest_pthread -n 40 -s 16384 -m 50 -t 10
echo " "
mq_pro -c 60 -m 555

#
# Test compatibility between ABIs
#

mach=`uname -m`
if [ $mach = "IP19" ] || [ $mach = "IP21" ] || [ $mach = "IP25" ] || \
			 [ $mach = "IP26" ] || [ $mach = "IP27" ]
then
	echo "\n\t\tPOSIX MESSAGE QUEUE TESTS - MULTIPLE ABIs\n"
	if [ -x ./n32bit/mqtest ] && [  -x ./64bit/mqtest ]
	then
		mqname=/usr/tmp/"mq$$`date '+%m%d%y%H%M%S'`"
		./mqtest -c -s 128 -n 32 $mqname	 #create a queue
		./64bit/mqtest -S 200 $mqname &	         #send messages
		./n32bit/mqtest -R 50 $mqname &	         #receive messages
		./mqtest -R 100 $mqname &                #receive messages
		./64bit/mqtest -R 50 $mqname	         #receive messages
		wait
		./n32bit/mqtest -u $mqname		 #unlink the mq
	else
		echo -n "\nWARNING: cannot execute ./n32bit/mqtest "
		echo "and/or ./64bit/mqtest"
	fi
fi

if [ $mach = "IP22" ] || [ $mach = "IP32" ]
then
	echo "\n\t\tPOSIX MESSAGE QUEUE TESTS - MULTIPLE ABIs\n"
	if [ -x ./n32bit/mqtest ]
	then
		mqname=/tmp/"mq$$`date '+%m%d%y%H%M%S'`"
		./mqtest -c -s 128 -n 32 $mqname	 #create a queue
		./n32bit/mqtest -S 200 $mqname &         #send messages
		./mqtest -R 100 $mqname &                #receive messages
		./n32bit/mqtest -R 100 $mqname &         #receive messages
		wait
		./n32bit/mqtest -u $mqname		 #unlink the mq
	else
		echo -n "\nWARNING: cannot execute ./n32bit/mqtest "
		echo "and/or ./64bit/mqtest"
	fi
fi

#	
# Posix message queue performance
#

mq_bench -p
mq_bench -p -n

