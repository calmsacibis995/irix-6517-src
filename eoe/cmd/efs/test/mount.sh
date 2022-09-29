#! /bin/sh
#
# Script to flagellate the mount code in the system
#

pass=0
while `true`; do
	pass=`expr $pass + 1`
	echo "Pass "$pass
	mount -f efs /dev/usr /usr
	mount -f S51K /dev/usr0 /usr0
	umount /dev/usr
	mount -f S51K /dev/root0 /root0
	umount /dev/usr0
	umount /dev/root0
done
