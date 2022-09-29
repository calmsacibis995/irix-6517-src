#!/bin/ksh
if ! test -f /usr/sbin/dplace
then
	echo "Cannot find /usr/sbin/dplace -- please install"
	exit 1
fi

if test $# -lt 2 
then
	echo "Usage: $0 <node-name> command"
	exit 1
fi
	
if ! test -d $1
then
	echo "Usage: $0 <node-name> command"
	echo "The node can only be one of the following:"
	find /hw -name node -print
	exit 1
fi

TMP=/usr/tmp/ndrun$$
echo "memories 1 topology physical near $1" > $TMP
echo "threads 1" >> $TMP
echo "run thread 0 on memory 0" >> $TMP

shift
/usr/sbin/dplace -migration 0 -place $TMP $@

rm -f $TMP
exit 0