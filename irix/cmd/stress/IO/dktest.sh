#!/bin/sh
# $Header: /proj/irix6.5.7m/isms/irix/cmd/stress/IO/RCS/dktest.sh,v 1.1 1989/02/22 16:29:12 lee Exp $

# Usage: run cycles
USAGE="echo Usage: $0 [-a -f fname -b bsize -o offset -s fsize -e seed -t tnum ] cycles"

if [ $# -lt 1 ]
then
	$USAGE
	exit
fi
#rm -f Report/* 2>/dev/null

fname=test
async=no
bsize=512
offset=0
fsize=1048576
tnum=1
seed=1

set - `getopt 'af:b:o:s:e:t:' $*`

for i
do
	case $i in
	-a)	async=yes
		shift;;
	-f)	fname=$2; 
		shift 2;;
	-b)	bsize=$2;
		shift 2;;
	-o)	offset=$2;
		shift 2;;
	-s)	fsize=$2;
		shift 2;;
	-e)	seed=$2;
		shift 2;;
	-t)	tnum=$2;
		shift 2;;
	--)	shift;
		break;;
	-*)	echo "Unknown option $i"
		exit 1
	esac
done

#echo "-f $fname -b $bsize -o $offset -s $fsize -e $seed -t $tnum"

if [ "$tnum" -lt 1 ]
then 
	$USAGE
	exit 2
fi

#cycle can not less than 1
if [ "$1" -lt 1 ]
then 
	$USAGE
	exit 2
fi

# use cycles to mean # times entire suite is done
iter=1
fsize_s=$fsize
offset_s=$offset
bsize_s=$bsize
seed_s=$seed

while [ "$iter" -le "$1" ]
do
	num=1
	echo "\nIteration $iter @ `date`\n"
	while [ "$num" -le "$tnum" ]
	do
		if [ "$async" = "yes" ]
		then 
		   ./dkstress $fname$num -b$bsize -o$offset -s$fsize -e$seed &
		else
		   ./dkstress $fname$num -b$bsize -o$offset -s$fsize -e$seed 
		fi

		fsize=`expr $fsize + $fsize_s`
		offset=`expr $offset + 3`
		bsize=`expr $bsize + 512`
		seed=`expr $seed + 2`
		num=`expr $num + 1`
	done
	wait

	iter=`expr $iter + 1`
	fsize=$fsize_s
	offset=$offset_s
	bsize=$bsize_s
	seed=$seed_s
done

echo "\nIO TEST COMPLETE"; echo "      " ; date
exit 0
