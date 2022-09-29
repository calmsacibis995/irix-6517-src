#!/bin/sh

SCRIPT_NM=`basename $0`
SCRIPT_PID=$$

#
# This version of runtests executes a series of subtests (as defined
# below). First, the subtests are run sequentially to verify that each
# test functions correctly (i.e. that each test succeeds). Secondly,
# multiple copies of each subtest is run to stress the system. During
# the stress phase, a timeout period is set for the completion of
# each subtest so that hangs can be detected.
# 
SUBTESTS="
	aio
	aio_cb
	aio_append
	aio_fs
	aio_lio
"
# Excluded tests - these don't seem to work reliably or don't terminate:
#	aio_fs2

STRESS_FACTOR=100	# default number of instances of each test
TIMEOUT=300		# allow 5 mins for each stress

while getopts fx: opt; do
	case $opt in
	f)	functional_only=yes;;
	x)	STRESS_FACTOR=$OPTARG;;
	\?)	echo "Unknown option $opt"
		exit 1
	esac
done

shift `expr $OPTIND - 1`
if test $# -ne 1
then
   echo "Usage: runtests [-x stress_factor] cycles"
   exit
fi

rm -f core

# use cycles to mean # times entire suite is done
iter=1

while [ "$iter" -le "$1" ]
do
	echo "\n\n$SCRIPT_NM: Iteration $iter @ `date`\n\n"
	iter=`expr $iter + 1`

	# From the top, lightly:
	echo "$SCRIPT_NM: Starting functional pass though subtests..."
	for test in $SUBTESTS; do
		./$test
		if [ $? -ne 0 ]; then
			echo "$SCRIPT_NM: *** $test failed, exiting ***"
			exit 1
		fi
	done
	echo "$SCRIPT_NM: ...completed."
	[ "$functional_only" = "yes" ] && break

	#
	# Now heavily...
	#
	trap 'echo "$SCRIPT_NM: *** Aborted ***";	\
	      kill $WAITER 1>/dev/null 2>&1;		\
	      exit 1' TERM
	for test in $SUBTESTS; do
		(
			#
			# This subshell provides a time-out for
			# the test about to start.
			#
			trap 'kill $SLEEPER 1>/dev/null 2>&1; exit' TERM
			sleep $TIMEOUT &
			SLEEPER=$!
			wait $SLEEPER
			echo "$SCRIPT_NM: *** $test timed-out, incomplete ***" 
			kill $SCRIPT_PID
		)&
		TIMER=$!
		echo "$SCRIPT_NM: Starting $STRESS_FACTOR instances of $test..."
		(
			#
			# This subshell spawns multiple instances of a
			# test and waits for all to complete.
			#
			i=0; while [ $i -lt $STRESS_FACTOR ]; do
				./$test </dev/null &
				i=`expr $i + 1`
			done	
			wait
		) &
		WAITER=$!
		wait $WAITER
		echo "$SCRIPT_NM: ...completed."
		kill $TIMER 1>/dev/null 2>&1
	done
done

echo "\n\n$SCRIPT_NM: AIO TEST COMPLETE @ `date`\n\n"

exit 0
