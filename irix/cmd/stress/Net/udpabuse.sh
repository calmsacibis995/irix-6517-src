#!/bin/ksh
#
# don't set alarm; do intvl*10 packets (e.g. 6000 for 600 seconds)

set +u

if [ X$1 = X ]; then
	echo "udpabuse: no hostname supplied"
	exit 1
fi

if [ X$2 = X ]; then
	echo "udpabuse: no port supplied"
	exit 1
fi

if [ X$3 = X ]; then
	intvl=300
else
	intvl=$3
fi

let intvl="$intvl * 10"

while [ $intvl -gt 0 ]; do
	./udpping -n -C 5 -p $2 -h $1
	let intvl="$intvl - 5"
done
