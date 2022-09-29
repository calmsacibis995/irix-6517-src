#!/bin/ksh

# This is a skeleton of testing data pipe program. When it's called
# without any argument, all the tests are done; when it's called with
# arguments, each argument must be a number indicating which test is
# to be done.

# make sure this is not root directory.
if [ `pwd` = "/" ] 
then
	echo "This set of tests should be done in root directory"
	exit 1
fi

# a simple test with one iov for both input and output file. 
# the file size is small.
test1 ( ) {
	echo "dpcp test 1"
	./dpcp1 src1 src1iov sink1 sink1iov
	if [ $? != 0 ] 
	then
		echo "dpcp 1 test fail.";
		exit 1
	else 
		diff src1 sink1 > /dev/null
		if [ $? != 0 ] 
		then
			echo "file not copied right."
			exit 1
		else 
			echo "PASS!!"
			rm -f sink1
		fi
	fi
}

# copy a bigger file /unix to /tmp/unix
test2 ( ) {
	echo "dpcp test 2"
	./dpcp /unix /tmp/unix 
	if [ $? != 0 ] 
	then
		echo "dpcp test 2 fail."
		exit 1;
	else 
		diff /unix /tmp/unix > /dev/null
		if [ $? != 0 ] 
		then
			echo "file not copied right."
			exit 1
		else 
			echo "PASS!!"
			rm -f /tmp/unix
		fi
	fi
}

# testing multiple transfer ids
test3() {
	echo "dpcp test 3"
	./dpcp3 src1 src1iov sink3 sink1iov
	if [ $? != 0 ]
	then
		echo "dpcp test 3 fail."
		exit 1
	else
		echo "PASS!!"
	fi
}

# copy /unix in a loop
test4() {
	echo "dpcp test 4"
	loop=1
	while [ $loop -le 6 ]
	do
		./dpcp src1 sink4.$loop
		if [ $? != 0 ] 
		then
			echo "dpcp test 4 fail."
			exit 1
		fi
		loop=`expr $loop + 1`
	done
	echo "PASS!!"
}

# process command argument
if [ $# = 0 ]
then
	echo "test all"
	test1
	test2
	test3
	test4
else
	while [ $1 ]
	do
		case $1 in
			1)
			  test1
			  ;;
			2)
			  test2
			  ;;
			3)
			  test3
			  ;;
			4)
			  test4
			  ;;
			*)
			  echo "Invalid argument"
			  ;;
		esac
		shift
	done
fi
		
	