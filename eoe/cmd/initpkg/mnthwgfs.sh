#! /sbin/sh
#ident "$Revision: 1.7 $"
#
# This script is responsible for mounting the hardware graph file system.
# This is typically invoked by /etc/brc upon bootup.  The filesystem is
# mounted on /hw, if that directory exists.
#
# added an argument so we don't need two script that do the same thing.

case $1 in
'/root/hw')
        ARG1="/root/hw"
        ;;
*)
        ARG1="/hw"
        ;;
esac

if [ ! -d $ARG1 ]; then
        /bin/rm -rf $ARG1  > /dev/null 2>&1
        mkdir $ARG1
fi

if [ -d $ARG1 ] 
then 
	if [ -d $ARG1/.devhdl ]
	then
		# already mounted, so just make sure there's an mtab entry
		/sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -f -t hwgfs /hw $ARG1"
	else
		if /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -t hwgfs /hw $ARG1"
		then :
		else
			echo "Unable to mount $ARG1"
		fi
	fi
else
	echo "warning: $ARG1 mount point does not exist"
fi
