#!/bin/sh

npp=2		# -n number to do in parallel
ldir=		# -d local directory to use
verbose=	# -v
cmdloc=.	# -c helper command locations
slp=0		# -s amount of type to 'idle'

while getopts c:vn:d:s: c
do
	case $c in
	n)	npp=$OPTARG;;
	d)	ldir=$OPTARG;;
	v)	verbose=1;;
	c)	cmdloc=$OPTARG;;
	s)	slp=$OPTARG;;
	esac
done
shift `expr $OPTIND - 1`
if [ "$ldir" != "" -a ! -d "$ldir" ]
then
	mkdir $ldir
fi
cd $ldir

#
# XXX if you want rt to do ndpri, then you must set rt suid
#
while [ $npp -gt 0 ]
do
	/usr/stress/Misc/rt -w4 -l1 >/dev/null&
	npp=`expr $npp - 1`
done

wait

# this test already goes for a minute, and sleeps...
#if [ $slp -gt 0 ]
#then
	#sleep $slp
#fi

exit
