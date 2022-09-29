#!/bin/ksh

# make sure this is not root directory.
if [ `pwd` = "/" ] 
then
	echo "This set of tests should be done in root directory"
	exit 1;
fi

# a simple test with one iov for both input and output file. 
# the file size is small.
test1 ( ) {
	echo "dpcp test 1"
	./dpcp1 src1 src1iov sink1 sink1iov
	if [ $? != 0 ] 
	then
		echo "dpcp 1 test fail.";
		exit 1;
	else 
		diff src1 sink1 > /dev/null
		if [ $? != 0 ] 
		then
			echo "file not copied right."
			exit 1;
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
		echo "dpcp 2 test fail.";
		exit 1;
	else 
		diff /unix /tmp/unix > /dev/null
		if [ $? != 0 ] 
		then
			echo "file not copied right."
			exit 1;
		else 
			echo "PASS!!"
			rm -f /tmp/unix
		fi
	fi
}

# process command argument
if [ $# = 0 ]
then
	echo "test all"
	test1
	test2
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
			*)
			  echo "Invalid argument"
			  ;;
		esac
		shift
	done
fi
		
	