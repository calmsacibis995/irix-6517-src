#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# miniroot script - clean disks for clean installs.
#
# This script is invoked from miniroot inst when the
# user invokes "admin mkfs" with no arguments to mkfs.
#
# It umounts all the users file systems, cleans (mkfs)
# partitions 0 and (if present) 6 of the boot device,
# and remounts just these partitions as root and (if
# there was a partition 6) usr.
#
# Whatever was there before, such as other mounts, logical
# volumes, user data, inst history, ... is lost.

cat <<-!!

                   ** Clean Disks Procedure **

      If you agree to it, this procedure will clean your disks,
      removing all data from the root and (if present) the user
      file systems.

      Boot device partitions zero (0) and, if present, six (6)
      will be erased (using mkfs).  This will destroy all data on
      them.  These partitions will then be remounted under /root
      and (if present) /root/usr.

      If you have data on these file systems you want to save,
      answer "no" below, and backup the data before cleaning
      your disks.

      Any other file systems or logical volumes will be unmounted
      and forgotten about until you choose to reconfigure and
      remount them.

!!

while : asking if really want to clean disks
do
    echo '\n\tAre you sure you want to clean your disks ?'
    echo '\t\t   { (y)es, (n)o, (sh)ell, (h)elp }: \c'
    read line
    
    case "$line" in
    [yY]*)
	break
	;;
    [nN]*)
	echo
	echo Disks not cleaned.  Returning to inst.
	echo
	exit
	;;
    sh|SH|shell|SHELL)
	( echo set prompt="'miniroot> '" > /.prompt ) 2>/dev/null
	csh
	echo
	continue 1
	;;
    shroot|SHROOT)
	chroot /root /bin/csh
	echo
	continue 1
	;;
    [hH]*)
	cat <<-\!!
	
	yes - Will clean your disks.
	      
	      New file systems will be made on partitions zero (0) and,
	      if present, six (6).  This will destroy all data on them.
	      These partitions will be remounted under /root and /root/usr.
	      
	      If you have data on these file systems you want to save,
	      answer "no" here, and backup the data before proceeding
	      with a clean install.
	
	      Any other file systems or logical volumes will be unmounted
	      and forgotten about until you choose to reconfigure them.
	
	no  - Will not touch your disks, and return to the inst menus.
	
	sh  - Invokes a subshell (csh) in the miniroot, as root.
	
	help - show this message
	
	!!
	continue 1
	;;
    *)
	cat <<-\!!
		
		To clean your disks, type "yes" and press ENTER.
		To resume installing software, type "no" and press ENTER.
		Type "help" for additional explanation.
		
		!!
	continue 1
	;;
    esac
done	# asking if really want to clean disks

# Use same do_mkfs() function for both root and usr partitions.

do_mkfs()
{
	device=$1
	bsize=$2
	fsstat $device >&- 2>&-
	
	case $? in
	0|1)	# 0: The file system is not mounted and appears okay
		# 1: File system is not mounted and is dirty (needs fsck)
	    echo
	    echo WARNING: There appears to be a valid file system on $device already.
	    echo Making a new file system will destroy all existing data on it.
	    ;;
	2)	# fsstat says file system is mounted (still? - darn!)
	    echo
	    echo ERROR: Unmount of $device failed.
	    echo Disks not cleaned.  Returning to inst.
	    echo
	    exit
	    ;;
	3)	# fsstat didnt find any file system - cool
	    ;;
	esac
	
	while : asking to mkfs device
	do
	    echo
	    echo Make new file system on $device? '\c'
	    read line
	    
	    case "$line" in
	    yes|YES)
		echo '\n'Doing: mkfs -b size=$bsize $device
		mkfs -b size=$bsize $device
		return
		;;
	    no|NO)
		echo
		echo Skipping mkfs of $device.
		echo Disks not cleaned.  Returning to inst.
		echo
		mrmountrc mntlocal
		exit
		;;
	    sh|SH)
		( echo set prompt="'miniroot> '" > /.prompt ) 2>/dev/null
		csh
		echo
		continue 1
		;;
	    shroot|SHROOT)
		chroot /root /bin/csh
		echo
		continue 1
		;;
	    [hH]*)
		cat <<-!!
		
		yes - Will cause mkfs to be run on partition:
		
				$device
		
		      Any data on this partition will be lost.
		
		      If there might be useful data here, you
		      should back it up before consenting to this
		      mkfs.
		
		no  - Will leave $device unchanged.
		
		sh  - Invokes a subshell (using C Shell csh), in the
		      miniroot, as root.  This permits the use of available
		      IRIX utilities to administer your system.  You can
		      also type "shroot" to chroot csh below /root.
		
		help - show this message
		
		!!
		continue 1
		;;
	    *)
		echo Please answer \"yes\" or \"no\".
		continue 1
		;;
	    esac
	done	# asking to mkfs device

}   # end of do_mkfs() function


#######################################################################


umount -k /root/hw >/dev/null 2>&1
umount -k -T efs,xfs -b / >/dev/null 2>&1

root=`devnm / | sed 's/. .*/0/'`	# Jam '0' on end of swap device
usr=`devnm / | sed 's/. .*/6/'`		# Jam '6' on end of swap device

## mkfs and mount root partition

    if prtvtoc -s -h $root |
	    awk '$1 == 6 && ( $2 == "efs" || $2 == "xfs" || $2 == "xlv" )' |
	    grep . > /dev/null
    then
	root_bsize=512
	haveusr=1
    else
	root_bsize=4096
	haveusr=0
    fi

    do_mkfs $root $root_bsize root
    test -d /root || mkdir /root > /dev/null 2>&1
    mount -c $root /root || {
	echo
	echo ERROR: Mount of $root on /root failed.
	echo Returning to inst.
	echo
	exit
    }
    
    mount -p > /etc/fstab.Sav
    cp /etc/fstab.Sav /etc/fstab
    mkdir /root/etc /root/usr
    echo "/dev/root / xfs rw,raw=/dev/rroot 0 0" > /root/etc/fstab

## mount hwgraph
    mkdir /root/hw
    /etc/mnthwgfs /root/hw

## mkfs and mount usr partition (if present)

    if [ $haveusr = 1 ]
    then
	# There is a partition 6, mkfs it and mount as usr file system.
    
	do_mkfs $usr 4096
	mount -c $usr /root/usr || {
	    echo
	    echo ERROR: Mount of $usr on /root/usr failed.
	    echo Returning to inst.
	    echo
	    exit
	}

	echo "/dev/usr /usr xfs rw,raw=/dev/rusr 0 0" >> /root/etc/fstab
	echo "$usr /root/usr xfs rw 0 0" >> /etc/fstab

	mkdir /root/usr/var
	ln -s usr/var /root/var
    else
	mkdir /root/var
    fi

# show user what's mounted

echo "\nMounting file systems:\n"

mount -p | awk '
    $3 == "efs" || $3 == "xfs" { printf "    %-23s  on  %s\n", $1, $2; }
'
echo
echo

# restart syslogd
/etc/mrlogrc start
