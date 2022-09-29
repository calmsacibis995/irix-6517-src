#!/bin/ksh
#
# This runs the tomcatv tests on -- this expect there to be a directory:
# ~diag/testfpu_dir/tomcatv.  In this directory, there should be:
#	tomcatv (executable for the tomcatv from SPEC95)
#	ref (directory)
# 	train (directory)
# 	test (directory)
#	tomcatv_inputs (the names ref train and test go in here)
#
# each of "ref train test" should contain two directories:
#	input (directory)
#		TOMCATV.MODEL
#		tomcatv.in
#	output (directory)
#		tomcatv.out
#
# none of these files should ever be changed!

USAGE="$0 <input> ...\n\t<input> files can be one or more of: test, ref or train"
main_dir=~diag/online_diags/tomcatv
temp_file=/usr/tmp/tomcatv_tmpfile

test_dirs=$@

echo "RUNNING TOMCATV IN DIRECTORIES: $test_dirs"

for test in $test_dirs
do
	if [[ $test != "ref" && $test != "train" && $test != test ]]
	then
		echo $USAGE
		exit 1
	fi
done

for test in $test_dirs
do
	echo "\nDIRECTORY: $test"
	echo "\trunning tomcatv on the $main_dir/$test directory"
	cd $main_dir/$test/input
	$main_dir/tomcatv < $main_dir/$test/input/tomcatv.in > \
		${temp_file}_$test
	diff $main_dir/$test/output/tomcatv.out ${temp_file}_$test
	if [ $? -ne 0 ]
	then
		echo "ERROR: $main_dir/$test/output/tomcatv.out"
		echo "\tdiffers from output of run"
		echo "OUTPUT from run:"
		cat ${temp_file}_$test
		rm ${temp_file}_$test
		exit 1
	fi
	echo "\toutput in $main_dir/$test/output/tomcatv.out"
	echo "\t\tmatches the output from the run"
	rm ${temp_file}_$test
	echo "\tdone running in $test directory"
done

exit 0
