#! /bin/sh

#/usr/stress/xfs/fsstress -n1000000 -p10 -w -f attr_remove=20 -f attr_set=20 -d $1

/usr/stress/xfs/fsstress -n1000000 -p10 -d $1 -z -f mkdir=1 -f creat=5 -f attr_set=20 -f attr_remove=1 
