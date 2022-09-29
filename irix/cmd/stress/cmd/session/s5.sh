#!/bin/sh

npp=2		# -n number to do in parallel
ldir=		# -d local directory to use
verbose=	# -v
cmdloc=.	# -c helper command locations
meg=10		# megabytes to use
slp=0		# -s amount of type to 'idle'

while getopts m:c:vn:d:s: c
do
	case $c in
	n)	npp=$OPTARG;;
	d)	ldir=$OPTARG;;
	v)	verbose=1;;
	c)	cmdloc=$OPTARG;;
	m)	meg=$OPTARG;;
	s)	slp=$OPTARG;;
	esac
done
shift `expr $OPTIND - 1`
if [ "$ldir" != "" -a ! -d "$ldir" ]
then
	mkdir $ldir
fi

while [ $npp -gt 0 ]
do
	( date ; $cmdloc/usemem -m $meg -l3; date )&
	npp=`expr $npp - 1`
done

wait
if [ $slp -gt 0 ]
then
	sleep $slp
fi

exit
