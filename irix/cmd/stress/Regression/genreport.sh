#!/bin/sh
#
# Report generator script for regression tests
#
# Usage:  genreport [-d output_dir] [-r report_file]
#
# Initialize variables
USAGE="Usage: $0 [-d output_dir] [-o report_file]"
OUTPUT_DIR=./Report
BEGIN_TIME=""
END_TIME=""
REPORT_FILE=""
TEST_MACHINE=`hostname`
OS_RELEASE=`uname -r`
OS_VERSION=`uname -v`
TEST_LIST="acceptance:Sherwood_Acceptance
	   disks:Disk_Perf_Benchmarks
	   IO:I/O_Stress_Tests
	   Ipc:Interprocess_Stress
	   Istress:Filesystem_Stress
	   Misc:System_Call_Stress
	   Mmap:mmap_File_Stress
	   Pigs:Sherwood_Regression
	   Vm:Virtual_Mem_Stress
	   X:X11_Stress_Tests"

# Parse command line
set -- `getopt b:d:e:o: $*`
if [ $? != 0 ]; then
    echo $USAGE
    exit 2
fi
for i in $*; do
    case $i in
    -b)  BEGIN_TIME=`echo $2 | tr '_' ' '`; shift 2;;
    -d)  OUTPUT_DIR=$2; shift 2;;
    -e)  END_TIME=`echo $2 | tr '_' ' '`; shift 2;;
    -o)  REPORT_FILE=$2; shift 2;;
    --)  shift; break;;
    esac
done

# Generate the report
(
    echo "REGRESSION TESTING SUMMARY REPORT"
    echo ""
    echo "Machine:    $TEST_MACHINE"
    echo "OS Release: $OS_RELEASE"
    echo "OS Version: $OS_VERSION"
    if [ "X$BEGIN_TIME" != X ]; then
	echo "Begin time: $BEGIN_TIME"
    fi
    if [ "X$END_TIME" != X ]; then
	echo "End time:   $END_TIME"
    fi
    echo ""
    echo "Test			#Runs	Status"
    echo "------------------------------------------------------"
    for i in $TEST_LIST; do
	t="$IFS";IFS=":";set $i;IFS="$t"
	of=$1
	test=$2
	if [ -f $OUTPUT_DIR/$of.out ]; then
	    errors=`egrep "ERROR|FAILED" $OUTPUT_DIR/$of.out | wc -l`
	    set $errors
	    status="cumulative errors: $1"
	    t=`grep "Iteration" $OUTPUT_DIR/$of.out | tail -1`
	    if [ "X$t" != X ]; then
		set $t
		runs=$2
	    else
		runs="?"
	    fi
	else
	    runs=0
	    status="tests not run"
	fi
	echo "$test\t$runs\t$status"
    done
    echo "------------------------------------------------------"
    echo ""
    echo "Hardware Inventory:"
    echo ""
    /sbin/hinv
) > $REPORT_FILE

exit 0
