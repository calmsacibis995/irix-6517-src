#!/bin/sh

# Usage: run cycles
USAGE="Usage: $0 [-a] cycles"

if [ $# -lt 1 ]; then
    echo $USAGE
    exit
fi

if [ -d Report ]; then
    rm -f Report/* 2>/dev/null
else
    mkdir Report
fi

set - `getopt 'a' $@`
async=no
for i
do
    case $i in
    -a)     async=yes
	    shift;;
    --)     shift;
	    break;;
    -*)     echo "Unknown options $i"
	    exit 1
    esac
done

EXTERN_TESTS="Pigs Istress Ipc Vm IO Mmap Misc disks"
LOCAL_TESTS="X acceptance"
iter=$1

for i in $EXTERN_TESTS
do
    echo "\nStarting $i"
    if [ "$async" = "yes" ]; then
	( cd ../crash/$i; ../../regression/scripts/$i $iter ) \
	  > Report/$i.out 2>&1 &
    else
	( cd ../crash/$i; ../../regression/scripts/$i $iter ) \
	  > Report/$i.out 2>&1
    fi
done

for i in $LOCAL_TESTS
do
    echo "\nStarting $i"
    if [ "$async" = "yes" ]; then
	( cd $i; ../scripts/$i $iter ) > Report/$i.out 2>&1 &
    else
	( cd $i; ../scripts/$i $iter ) > Report/$i.out 2>&1
    fi
done

echo ""
echo "Done $1 iterations on `date`"
