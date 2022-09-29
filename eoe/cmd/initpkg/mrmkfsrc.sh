#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# miniroot script - handles running mkfs on specified file system.
#
#		    This script is called from mrmountrc,
#		    and from inst to handle admin:mkfs.
#		    It is not invoked directly from inittab.

interactive=1
if [ "$1" = "-a" ]; then
    interactive=0
    shift
fi

if [ $# -lt 1 ]; then
    echo "Usage: mrmkfsrc device" 1>&2
    echo "       mrmkfsrc -a device fstype blocksize" 1>&2
    exit 1
fi

# type, blocksize and mkfsopts are only used in automatic mode (-a)
device="$1";	test $# -gt 0 && shift
fstype="$1";	test $# -gt 0 && shift
bsize="$1";	test $# -gt 0 && shift
mkfsopts="$*"	# additional args for mkfs

test -b "$device" || {
    { test -c "$device" && echo Specify block device, not raw.; } ||
    { test -n "$device" && echo Unable to mkfs \""$device"\": No such device; } ||
			 echo mrmkfsrc device option missing - internal error
    exit 1
}

# If device is mounted, need to unmount it.
    directory=`mount -p | awk '$1 == "'"$device"'" { print $2 }'`
    test -d "$directory" && {
	echo Unmounting device \""$device"\" from directory \""$directory"\".
	# umount below directory, silently.
	umount /root/hw 2>&-
	umount -T efs,xfs -b /,/root,"$directory" 2>&-
	# umount directory itself.  Let user see errors.
	umount "$directory" || { echo Unmount failed.; exit 1; }
    }

# Raw device is much faster, especially on large disks.	
    mntflags=`awk < /etc/fstab '$1 == "'"$device"'" { print $4}'`
    trialrawdevice=`echo "$mntflags" | sed 's/.*,raw=\([^,]*\).*/\1/'`

    # If fstab doesn't say what the rawdevice is, try simply
    # replacing "/dsk/" with "/rdsk/" in device pathname.
    test -c "$trialrawdevice" || trialrawdevice=`echo "$device" |
			    sed -n '/\/dev\/dsk\//s;/dev/dsk/;/dev/rdsk/;p'`

    # If trialrawdevice is raw match to device, mkfs it instead.
    #
    # I show the user the device name "$device", but actually
    # mkfs "$mkfsdevice", which is either equal to "$device" or
    # is the raw character device equivalent to the block device
    # "$device".  I do this because otherwise I have to show the
    # user that we are not mkfs'ing exactly what they asked to
    # mkfs, and I fear such a discrepancy in what the user sees
    # will cause more pain and suffering than will this white lie.
    # Seems better not to confuse the user with the details of
    # this performance tweak.

    mkfsdevice="$device"
    if test -b "$device" -a -c "$trialrawdevice" &&
       test `stat -qd "$device"` -eq `stat -qd "$trialrawdevice"`
    then
	mkfsdevice="$trialrawdevice"
    fi

    # If non-interactive, make fs now and exit without prompting
    if [ "$interactive" = 0 ]; then
	if [ "$fstype" = efs ]; then
	    mkfs -t efs $mkfsopts $mkfsdevice
	    exit $?
	elif [ "$fstype" = xfs ]; then
	    bsopts=""
	    if test "$bsize" != "" &&
		echo "$mkfsopts" | grep -vq "\-b[ \t]*size=" ; then
		# mkfsopts overrides bsize as mkfs disliked multiple -b opts
		bsopts="-b size=$bsize"
	    fi
	    mkfs -t xfs $bsopts $mkfsopts $mkfsdevice
	    exit $?
	else
	    echo "Invalid filesystem type $fstype for $device"
	    exit 1
	fi
    fi
    
while : user calls sh or help
do
    echo '\n'Make new file system on "$device" '[yes/no/sh/help]: \c'
    read line
    
    case "$line" in
    yes|YES)
	echo '\n'About to remake '(mkfs)' file system on: "$device"
	echo This will destroy all data on disk partition: "$device".
	echo '\n\tAre you sure? [y/n] (n): \c'
	read ok
	case "$ok" in
	[yY]*)
	    echo '\n\tBlock size of filesystem 512 or 4096 bytes? \c'
	    read bsize
	    case "$bsize" in
	    512|1024|2048|4096)
		echo '\n'Doing: mkfs -b size=$bsize $device
		mkfs -t xfs -b size=$bsize $mkfsdevice
		exit 0
		;;
	    *)
		echo Did not attempt mkfs, no valid size given.
		;;
	    esac
	    ;;
	*)
	    echo Did not attempt mkfs.
	    exit 1
	    ;;
	esac
	;;
    no|NO)
	echo Did not attempt mkfs.
	exit 1
	;;
    sh|SH)
	( echo set prompt="'miniroot> '" > /.prompt ) 2>/dev/null
	csh
	echo
	;;
    [hH]*)
	cat <<-!!
	
	yes - Will cause mkfs to be run on partition:
	
	                $device
	
	      Any data on this partition will be lost.
	
	      If there might be useful data here, you
	      should back it up before consenting to this
	      mkfs.
	
	no  - Will return without attempting mkfs.
	
	sh  - Invoke a subshell, permitting use of available
	      IRIX utilities to repair this partition.
	
	help - show this message
	
	!!
	;;
    *)
	cat <<-\!!
    
	Type one of the words: "yes", "no", "sh" or "help".
	Then press the Enter key.
    
	!!
	;;
    esac
done
