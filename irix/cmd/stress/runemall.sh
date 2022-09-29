#!/bin/ksh
#
# Run different ABI versions of the main stress tests.
#

ptests='aio|ipc|gfx|vm|sproc|mmap|misc|fsr|dofuser|net|select|usock|posix1b|tli|fdpass|dv'

USAGE="$0: [-h][-i iterations][-x {32|n32|64}][-k][-s seed][-r][-n numtests][-h] [$ptests]\n\t\t-h help gets you this message\n\t\t-i number or iterations to run the stress tests for\n\t\t-x exclude 32bit, n32bit, or 64 bit versions\n\t\t-k keep core files\n\trandomizing options:\n\t\t-s randomly pick tests to run but set with given seed\n\t\t-r randomly pick tests to run with random seed\n\t\t-n set number of random tests run for -s or -r\n\t\tnote:  to reproduce a previous test, set the -s option\n\t\t\twith the seed of that run and the -n option\n\t\t\twith the number (if -n was not given for the\n\t\t\toriginal run it does not have to be given to\n\t\t\treproduce the run)"

no32=0
non32=0
no64=0
keepcore=0
pids=
otheropts=""
iter=1

# these are for random picking of what test gets done
rand=0
set -A tests aio ipc gfx vm sproc mmap misc fsr dofuser net select usock posix1b tli fdpass dv io istress pigs disks
numtests=0
maxtests=${#tests[*]}

#
# since we run runtests with an iteration of 1, the core files keep
# piling up on the same file name. So we move them aside
#
mkdir cores >/dev/null 2>&1
mkdir Report >/dev/null 2>&1

while getopts ki:x:rs:n:h opts
do
	case $opts in
	k)
		keepcore=1
		;;
	i)
		iter=$OPTARG
		;;
	x)
		if [ $OPTARG = "32" ] ; then no32=1;
		elif [ $OPTARG = "n32" ] ; then non32=1;
		elif [ $OPTARG = "64" ] ; then no64=1;
		fi
		;;
	r)
		if [ rand -eq 1 ] 
		then
		    echo "ERROR: -r and -s not compatable"
		    echo $USAGE
		    exit 1
		fi
		rand=1
		seed=$RANDOM
		;;
	s)
		if [ rand -eq 1 ] 
		then
		    echo "ERROR: -r and -s not compatable"
		    echo $USAGE
		    exit 1
		fi
		rand=1
		seed=$OPTARG
		;;
	n)
	        numtests=$OPTARG
		;;
	h|\?)
		echo $USAGE
		exit 1
		;;
	esac
done

if [ $rand -eq 0 ]
then
    shift `expr $OPTIND - 1`
    if [ $# -eq 0 ]
    then
	echo "Nothing to do!"
	exit 1
    fi
    set -A teststorun $@
    numtests=`expr ${#teststorun[*]}`
else 
    outfile=Report/GENERAL_OUTFILE_$seed
    command_line=$0
    for arg in $@
    do
	if [ $arg != "-r" ]
	then
	    command_line="$command_line $arg"
	else
	    command_line="$command_line -s $seed"
	fi
    done

    # picking random tests to run
    echo "Notes on reproducing and the running of this test can be found in:"
    echo "\t$outfile"
    echo "Run the following command line to reproduce this test:" > $outfile
    echo "\t$command_line" >> $outfile
    echo "\nThe results from this run of the test will end with:" >> $outfile
    echo "\t.out_${seed}_<test num>" >> $outfile
    RANDOM=$seed
    if [ $numtests -le 0 ]
    then
	numtests=`expr $RANDOM % $maxtests + 1`
    fi
    echo "\nWe are running the following tests:" >> $outfile
    i=0
    while [ $i -ne $numtests ]
    do
	teststorun[$i]=${tests[ `expr $RANDOM % ${#tests[*]}` ]} 
	echo "\ttest $i:  ${teststorun[$i]}" >> $outfile
	i=`expr $i + 1`
    done
fi


cleanup() {
	killall runemall runtests usemem dofuser myfsr
	exit 0
}

movecores() {
	num=$1
	cdir=$2
	lnum=
	sep=

	for i in $cdir/core*
	do
		if [ $i != "$cdir/core\*" ]
		then
			mv $i cores/$cdir/core.$num$sep$lnum
			rm $i
			if [ "$lnum" = "" ]
			then
				lnum=0
				sep=.
			else
				lnum=`expr $lnum + 1`
			fi
		fi
	done
}


trap cleanup 2 3 

#
# tests
#
doall() {
	iters=$1
	tdir=$2
	otheropts=$3

	if [ $keepcore -eq 0 ]
	then
		rm -f $tdir/core $tdir/core* $tdir/*/core $tdir/*/core*
		rm -fr cores/$tdir
	fi

	mkdir cores/$tdir cores/$tdir/n32bit core/$tdir/n32 cores/$tdir/64bit  >/dev/null 2>&1
	while [ $iters -gt 0 ]
	do
		if [ $no32 = 0 ]
		then
			echo "Running o32 version"
			(cd $tdir; ./runtests $otheropts 1;)
			movecores $iters $tdir
		fi
		if [ $non32 = 0 ]
		then
			echo "Running n32 version"
			if [ -d $tdir/n32bit ]
			then
				(cd $tdir/n32bit; ./runtests $otheropts 1;)
				movecores $iters $tdir/n32bit
			else
				(cd $tdir/n32; ./runtests $otheropts 1;)
				movecores $iters $tdir/n32
			fi
		fi
		if [ $no64 = 0 ]
		then
			echo "Running 64 version"
			(cd $tdir/64bit; ./runtests $otheropts 1;)
			movecores $iters $tdir/64bit
		fi
		iters=`expr $iters - 1`
	done
}

runfsr() {
	while true
	do
		fsr /var/tmp
		date '+fsr done %r'
	done
}

i=0
while [ $i -ne $numtests ]
do
	tail=${seed:+\_$seed\_$i}
	test=${teststorun[$i]}
	case $test in
	aio)
		doall $iter aio -x10 >Report/aio.out$tail 2>&1&
		;;
	posix|posix1b)
		doall $iter posix1b >Report/posix1b.out$tail 2>&1&
		;;
	ipc|Ipc)
		doall $iter Ipc >Report/Ipc.out$tail 2>&1&
		;;
	gfx|Gfx)
		doall $iter Gfx >Report/Gfx.out$tail 2>&1&
		;;
	vm|Vm)
		doall $iter Vm >Report/Vm.out$tail 2>&1&
		;;
	sproc|Sproc)
		doall $iter Sproc >Report/Sproc.out$tail 2>&1&
		;;
	mmap|Mmap)
		doall $iter Mmap >Report/Mmap.out$tail 2>&1&
		;;
	misc|Misc)
		doall $iter Misc >Report/Misc.out$tail 2>&1&
		;;
	net|Net)
		doall $iter Net >Report/Net.out$tail 2>&1&
		;;
	dv|DV)
		doall $iter DV >Report/DV.out$tail 2>&1&
		;;
	select|Select)
		doall $iter Select >Report/Select.out$tail 2>&1&
		;;
	usock|Usock)
		doall $iter Usock >Report/Usock.out$tail 2>&1&
		;;
	tli|Tli)
		doall $iter Tli >Report/Tli.out$tail 2>&1&
		;;
	fdpass|Fdpass)
		doall $iter Fdpass >Report/Fdpass.out$tail 2>&1&
		;;
	fsr)
		runfsr&
		;;
	dofuser)
		# XXX this never stops
		cmd/dofuser >/dev/null&
		;;
	test)
		echo testing&
		;;
	slp)
		echo sleeping 5
		sleep 5&
		;;
	\?)
		echo "Unknown test $i"
		;;
	esac
	pids="$pids $!"
	i=`expr $i + 1`
done
	
echo "jobs started $pids"
while true
do
	for i in $pids
	do
		if jobs $i 2>&1|grep "no such job" >/dev/null
		then
			echo "job $i complete"
			pids=`echo $pids | sed -e "s/$i//"`
		fi
	done
	if [ "$pids" = "" ]
	then
		exit 0
	fi
	echo waiting for jobs $pids to finish
	sleep 10
	date
	ls -l *.out
	find . -name core\* -print
done
