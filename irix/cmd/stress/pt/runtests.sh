#!/bin/ksh

#
# Subdirectories
# 

SUBTESTS="sanity stress"

# Usage: $0 [iterations]
#
#	Pass iterations to each SUBTEST

echo "\nPT start \c"; date
for i in ${SUBTESTS}; do
	(cd $i; ./runtests $1 >../../Report/pt.$i.out 2>&1)
done

echo "\nPT complete \c"; date
exit 0
