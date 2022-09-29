#! /bin/sh

# This script makes ${2} copies of /unix in
# directory ${1}, then compares the copies
# with the original.
# Copies are made simultaneously.

homedir=${1:-/usr/tmp}
ncopies=${2:-10}
udir=${homedir}/U$$

if [ -f $udir ]
then	echo Cannot create directory $udir
	exit 1
fi

if [ -d $udir ]
then	/bin/rm -rf $udir
fi

mkdir $udir


#
# copy $ncopies of /unix to local directory, asynchronously
#
nc=$ncopies
while [ $nc != "0" ]
do
	cp /unix $udir/unix$nc &
	nc=`expr $nc - 1`
done

#
# wait for copies to complete
#
wait

#
# compare all copies with the original
#
nc=$ncopies
while [ $nc != "0" ]
do
	echo cmp /unix $udir/unix$nc
	cmp /unix $udir/unix$nc
	if [ $? != 0 ]
	then	echo cmp /unix $udir/unix$nc failed
		exit
	fi
	nc=`expr $nc - 1`
done

if [ -d $udir ]
then	/bin/rm -rf $udir
fi

