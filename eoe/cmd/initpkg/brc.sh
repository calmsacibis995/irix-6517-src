#! /sbin/sh
#Tag 0x00000f00
#ident "$Revision: 1.48 $"
#
# This script is responsible for initializing the mounted filesystem table
# kept in /etc/mtab.  It also creates /etc/fstab if none exists.
#

if [ "`/sbin/nvram diskless 2> /dev/null`" -eq 1 ] ; then
    > /etc/mtab
    /sbin/mount -f /
    if [ -d /sbin ]; then
	/sbin/mount -f /sbin
    fi
    if [ -d /swap ]; then
	/sbin/mount -f /swap
    fi
    if [ -d /usr ]; then 
	/sbin/mount -f /usr
    fi
    if [ -d /var/share ]; then
	/sbin/mount -f /var/share
    fi
else
    rootdev=/dev/root
    usrdev=/dev/usr
    if [ ! -f /etc/fstab ] ; then
	/sbin/fsstat $usrdev  >/dev/null 2>&1
	if [ $? -eq 3 ] ; then
		echo "$rootdev /" | setmnt -f /etc/fstab
	else
		echo "$rootdev /\n$usrdev /usr" | setmnt -f /etc/fstab
    	fi
    fi
    if [ ! -f /etc/mtab ] ; then rm -f /etc/mtab; fi
    echo "$rootdev /" | setmnt
fi

mkdir /etc/fscklogs > /dev/null 2>&1

if test -n "`grep '^[^#].*[ 	]/var[/]*[ 	]' /etc/fstab`"; then
	if /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -c /var" > /etc/fscklogs/rvar 2>&1
        then
		cat /etc/fscklogs/rvar
        else
		cat /etc/fscklogs/rvar
                echo "Unable to Mount /var\n"
                sleep 5
        fi
else
	if test -n "`grep '^[^#].*[ 	]/usr[/]*[ 	]' /etc/fstab`"; then
		deflvl="`/sbin/nvram initstate 2>/dev/null`" 2>/dev/null
        	if [ "$deflvl" = "s" -o "$deflvl" = "1" ]; then
			if /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount /usr"
        		then :
        		else
                		echo "Unable to Mount /usr\n"
                		sleep 5
        		fi
		else
			if /sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -c /usr" > /etc/fscklogs/rusr 2>&1
        		then
				cat /etc/fscklogs/rusr
        		else
				cat /etc/fscklogs/rusr
                		echo "Unable to Mount /usr\n"
                		sleep 5
        		fi
		fi
	fi
fi
if [ ! -d /var/adm ]
then
	if [ -l /var ]
	then
		mkdir /usr/var > /dev/null 2>&1
	fi
        mkdir /var/adm
fi

/etc/mnthwgfs
  
/etc/mntproc

if [ ! -d /dev/fd ]; then
	/bin/rm -rf /dev/fd  > /dev/null 2>&1
	mkdir /dev/fd
fi

# redirect the output, since it's an optional filesystem.
# /dev/fd is used for exec of setuid and setgid scripts
/sbin/suattr -C CAP_MOUNT_MGT+ip -c "/sbin/mount -t fd /dev/fd /dev/fd" > /dev/null
