#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# miniroot script - mount normal file systems when entering state 2.
#
# This script is run in the miniroot to mount the normal file systems.
# Since it is marked with action "bootwait" in inittab, it is processed
# the first time init goes from single-user to multi-user state after
# the system is booted.
#
# It has to:
#	Mount the normal root file system under /root
#	Initialize any logical volumes specified in /root/etc/lvtab
#	Mount "some" of the other file systems specified in /root/etc/fstab,
#	or, if there is no such fstab, try to mount partition 6 as /root/usr.
#	With the user's permission and if necessary, mkfs root or usr.
#
# Notes:
#
#	The normal file system is presumed to be on the 0 partition
#	of the same device as the miniroot was booted from.
#
#	That is, the result of `devnm /` (usually swap partition 1)
#	has the last byte of the special file name forced to '0',
#	and the result is assumed to be the normal root partition.
#
#	If this assumed normal root partition does not seem to mount
#	then we offer to mkfs it for the user.
#
#	However, with larger disks and RoboInst (non-interactive installs),
#	sometimes it is desired to install software on a different disk or
#	partition than the default. Allow a different partition to be
#	mounted on /root, but query before proceding if an interactive
#	install.
#
#	The only other file systems in /root/etc/fstab that are mounted
#	are those that are "efs" or "xfs" but not "noauto", as determined
#	the command: mount -c -T efs,xfs -b /root.
#
#	This script makes use of a new mount -P and -M options to cause
#	pathnames in the fstab to be interpreted relative to /root.
#
#	Note also that we send the 'mrquota' flag to mount.This instructs the
#	kernel to keep quotas turned on if the superblock already has quotas
#	enabled. Without this quotas will always get turned off on the /root
#	filesystem.
		
# Returns success (0) if able to create any directories that did
# not previously exist, which are listed as mount points in fstab.
# Returns 1 otherwise.

mkmountdirs()
{
    mstat=1
    for mdir in `nawk '{ if (match("^#",$1)) next;
		         if ($3=="efs" || $3=="xfs") print $2}' /etc/fstab`
    do
	test ! -d "$mdir" && mkdir -p "$mdir" 2>&- && test -d "$mdir" && mstat=0
    done
    return $mstat
}

case $1 in

mntlocal)

    echo "Mounting file systems:\n"

    test -d /root || mkdir /root > /dev/null 2>&1
    
    normalroot=`devnm / | sed 's/. .*/0/'`	# Jam '0' on end of swap device
    
    while { mount -o mrquota -c $normalroot /root; test $? -ne 0; }
    do					    # while unable to mount /root
	fsstat $normalroot >&- 2>&-

	case $? in
	0)		# fsstat says file system clean but not mounted?

	    if mrcustomrc iscustom; then
		LOGGER="/usr/bsd/logger -t roboinst"
		if [ "`devnm /root 2>/dev/null`" != "`devnm /`root" ] ; then
		    # in nvram mrmode=custom mode -- can't allow interaction!
		    #  must assume that it is OK
		    whatsthere=`devnm /root | sed 's/ .*//'`
		    echo "\n$whatsthere is mounted on /root instead of the default $normalroot."
		    echo "There is no way to interactively change this when nvram mrmode=custom."
		    break
		else
		    # uh, oh.  Nothing mounted!
		    # Since non-interactive, must abandon ship!
		    # this section changes when 'on error' gets added.
		    msg="Error mounting partition $normalroot when mrmode=custom"
		    msg2="This is your system disk: without it we have nothing"
		    msg3="on which to install software, and no recourse."
		    $LOGGER "state=mrmountrc status=$msg"
		    $LOGGER "state=mrmountrc status=$msg2"
		    $LOGGER "state=mrmountrc status=$msg3"
		    echo "$msg"
		    echo "$msg2"
		    echo "$msg3 Let the cascade failures begin!"
		    exit 1
		fi
	    else
		if [ "`devnm /root 2>/dev/null`" = "`devnm /`root" ] ; then
		    # nothing mounted there, problem!
		    echo '\n'Unable to mount partition: $normalroot on /root.
		    echo This is your system disk: without it we have nothing
		    echo on which to install software.
		    echo
		    echo Please manually correct your configuration and try again.
		    echo '\n\tPress Enter to invoke C Shell csh: \c'
		    read line
		    ( echo set prompt="'\@nAfter repairs, type "'"exit"'" to try mounting /root again.\@nminiroot> '" > /.prompt ) > /dev/null
		    csh
		    echo
		    ( echo set prompt="'miniroot> '" > /.prompt ) > /dev/null
		else
		    # interactive mode, make sure that it's OKAY
		    whatsthere=`devnm /root | sed 's/ .*//'`
		    echo "\n$whatsthere is mounted on /root instead of the default partition $normalroot."
		    echo "This is where the software will be installed."
		    echo "\n\tIs this OK? [y|sh][y] \c"
		    read line
		    if [ "$line" = "" -o "$line" = "y" -o "$line" = "Y" ] ; then
			echo "Installig software on $whatsthere.\n"
			break
		    else
			( echo set prompt="'\@nAfter repairs, type "'"exit"'" to try mounting /root again.\@nminiroot> '" > /.prompt ) > /dev/null
			csh
			echo
			( echo set prompt="'miniroot> '" > /.prompt ) > /dev/null
		    fi
		fi
	    fi
	    ;;
	1)		# fsck -y by mount -c failed - sicko file system
	    echo Unable to repair file system on partition: $normalroot
	    echo This is your system disk: without it we have nothing
	    echo on which to install software.
	    echo
	    mrmkfsrc $normalroot
	    ;;
	2)		# fsstat says file system is mounted
	    break	# oh well.  Break out of mount loop
	    ;;
	3)		# fsstat didnt find any file system
	    echo No valid file system found on: $normalroot
	    echo This is your system disk: without it we have nothing
	    echo on which to install software.
	    echo
	    mrmkfsrc $normalroot
	    ;;
	esac

	echo "\nTrying again to mount $normalroot on /root.\n"
    done

    # Mount graph to /root/hw since autoconfig is looking /root/hw/root
    # key on /root/hw/.invent ... if console doesn't exist ... mount hwgraph
    if [ ! -d /root/hw ]; then
        /bin/rm -rf /root/hw  > /dev/null 2>&1
        mkdir /root/hw
        /etc/mnthwgfs /root/hw
    else
        if [ ! -d /root/hw/.invent ]; then
                /etc/mnthwgfs /root/hw
        fi
    fi

    # Copy over the ioconfig.conf file and then run ioconfig 
    # to create the disk devices before we try and mount anything
    # This is also a good time to create tape devices.
    if [ -f /root/etc/ioconfig.conf ] ; then
      		cp /root/etc/ioconfig.conf /etc
    fi
    /sbin/ioconfig -f /hw
    cd /dev; ./MAKEDEV MAXPTY=10 MAXGRO=4 MAXGRI=4 tape scsi > /dev/null

    # Avoid repeated reboots on same miniroot appending
    # repeated copies of /root/etc/fstab to /etc/fstab,
    # and pick up results of mounting /root, above:

    test -f /etc/fstab.Sav || mount -p > /etc/fstab.Sav
    cp /etc/fstab.Sav /etc/fstab
    
    if test -f /root/etc/lvtab
    then
	echo "\n	LV volumes are no longer supported in this release.";
	echo "	Please use lv_to_xlv(1) to convert to XLV volumes.\n";
    fi

    test -x /root/sbin/xlv_assemble && mrinitxlvrc

    # In custom mode, adjust some configuration files
    # with information provided by the server.
    mrcustomrc mounthook

    if test -f /root/etc/fstab
    then
	# Goal: Append to /etc/fstab path adjusted contents of /root/etc/fstab
	#	Then mount local efs,xfs file systems under /root.
	# Use 3 new mount options:
	#	-M to specify alternate mtab, and
	#	-P to specify prefix to add to mnt path.
	#	-T to specify efs and xfs types.
        #
        # Do 2 XLV conversions here:
        # 1) Convert from pre 6.4 xlv names (/dev/dsk/xlv -> /dev/xlv)
        # 2) Prefix '/root' in front of the devicenames 
        #  (step 2 isn't necessary for hwg devices since /root/hw exists)

	mount -M /root/etc/fstab -P /root -p |
	    sed -e 's;/dev/dsk/xlv/;/dev/xlv/;' \
		-e 's;/dev/rdsk/xlv/;/dev/rxlv/;' \
	        -e 's;/dev/xlv/;/root/dev/xlv/;' \
	        -e 's;/dev/rxlv/;/root/dev/rxlv/;' \
	    | awk '$2 != "/root"' >> /etc/fstab

	mkdir /root/etc/fscklogs > /dev/null 2>&1

	while : attempting to mount
	do
		if mount -c -T efs,xfs -b /root > /root/etc/fscklogs/miniroot 2>&1 ; then
			cat /root/etc/fscklogs/miniroot
			break
		elif mrcustomrc iscustom && mkmountdirs ; then
			# created a directory that did not exist,
			# so try again
			continue
		else
			cat /root/etc/fscklogs/miniroot
			echo
			echo "Unable to mount all local efs,xfs file systems under /root"
			echo "Copy of above errors left in /root/etc/fscklogs/miniroot"
			echo
			sleep 5
			break
		fi
	done

    else	# no /root/etc/fstab

	if prtvtoc -s -h $normalroot |
		    awk '$1 == 6 && ( $2 == "efs" || $2 == "xfs" )' |
		    grep . > /dev/null
	then
	    # Goal: Since there is no fstab, this is probably a clean install.
	    #	If the main drive has a partition 6, get a file system
	    #	on it (if there isn't already one there, and if the user
	    #	agrees) and get it mounted at /root/usr.  Get var, usr/var
	    #	/etc/fstab and /root/etc/fstab set up correctly.
    
	    test -d /root/usr || mkdir /root/usr > /dev/null 2>&1
	    
	    normalusr=`devnm / | sed 's/. .*/6/'`   # Jam '6' on end of swap device
	    
	    while { mount -o mrquota -c $normalusr /root/usr; test $? -ne 0; }
	    do					# while unable to mount /usr
		fsstat $normalusr >&- 2>&-
	
		case $? in
		0)		# fsstat says file system clean but not mounted?
		    echo '\n'Unable to mount partition: $normalusr on /root/usr.
		    echo
		    echo This is your usr file system: you will need it mounted
		    echo to install software.
		    echo
		    echo Please manually correct your configuration and try again.
		    echo '\n\tPress Enter to invoke C Shell csh: \c'
		    read line
		    ( echo set prompt="'\@nAfter repairs, type "'"exit"'" to try mounting /usr again.\@nminiusr> '" > /.prompt ) > /dev/null
		    csh
		    echo
		    ( echo set prompt="'miniusr> '" > /.prompt ) > /dev/null
		    ;;
		1)		# fsck -y by mount -c failed - sicko file system
		    echo '\n'Unable to repair usr file system on partition: $normalusr
		    echo
		    echo This is your usr file system: you will need it mounted
		    echo to install software.  You can either obtain a shell "sh"
		    echo and attempt manual repairs, or you can say "yes" and
		    echo agree to make a new file system on your usr partition.
		    echo
		    mrmkfsrc $normalusr && continue
		    ;;
		2)		# fsstat says file system is mounted
		    break	# oh well.  Break out of mount loop
		    ;;
		3)		# fsstat didnt find any file system
		    echo '\n'No valid file system found on: $normalusr.
		    echo
		    echo This is your usr file system: you will need it mounted
		    echo to install software.  You can either obtain a shell "sh"
		    echo and attempt manual repairs, or you can say "yes"
		    echo and agree to make a new file system on your usr partition.
		    echo
		    mrmkfsrc $normalusr && continue
		    ;;
		esac

		while : asking if should try mount again
		do
		    echo
		    echo Try again to mount $normalusr on /root/usr? '[yes/no/sh/help]: \c'
		    read line
		    
		    case "$line" in
		    yes|YES)
			continue 2
			;;
		    no|NO)
			echo Giving up on trying to mount usr partition.
			break 2
			;;
		    sh|SH)
			( echo set prompt="'miniroot> '" > /.prompt ) 2>/dev/null
			csh
			echo
			continue 1
			;;
		    [hH]*)
			cat <<-!!
			
			yes - Will try again to mount the following device
			
			                $normalusr
			
			      below /root/usr.
			
			no  - Will give up on mounting your usr partition,
			      and go on to invoke the Software Installation
			      Utility.  If you have a small root partition
			      currently mounted under /root, you probably
			      won't have enough disk space to install
			      software.
			
			sh  - Invoke a subshell, permitting use of available
			      IRIX utilities to repair this partition.
			
			help - show this message
			
			!!
			continue 1
			;;
		    *)
			cat <<-\!!
		    
		        Type one of the words: "yes", "no", "sh" or "help".
		        Then press the Enter key.
		    
			!!
			continue 1
			;;
		    esac
		done	# asking if should try mount again
	    done	# while trying to mount usr file system ...
	fi		# if there's a partition 6 ...

	# There was no fstab - we have root and maybe usr mounted,
	# so create an fstab reflecting this new state of affairs,
	# and fix up var and maybe usr/var correctly.  We are
	# probably here on an install to clean disks, but don't
	# be so sure about that - maybe user just nuked fstab.

	(
	    if test ! -d /root/etc
	    then
		rm /root/etc			# rm etc if non-directory
		mkdir /root/etc			# make etc dir
	    fi
	    rm -f /root/etc/fstab		# maybe it was a fifo or ... ??
	    touch /root/etc/fstab		# safer create than '>'

	    rtype=`/etc/fstyp $normalroot`
	    # put root entry in fstab
	    test -f /root/etc/fstab &&
		echo '/dev/root / ' $rtype ' rw,raw=/dev/rroot 0 0' > /root/etc/fstab

	    fsstat $normalusr
	    if test $? -eq 2
	    then
		# if usr mounted, put it in fstab and fix var, usr/var

		utype=`/etc/fstyp $normalusr`
		test -f /root/etc/fstab &&
		    echo '/dev/usr /usr ' $utype ' rw,raw=/dev/rusr 0 0' >> /root/etc/fstab
		echo '/dev/usr /root/usr ' $utype ' rw,raw=/dev/rusr 0 0' >> /etc/fstab
    
		if test ! -d /root/usr/var
		then
		    rm /root/usr/var		# rm usr/var if non-directory
		    mkdir /root/usr/var		# make usr/var dir
		fi

		if test -d /root/usr/var
		then
		    rmdir /root/var		# rm var if empty dir
		    rm /root/var		# rm var if non-dir
		    mv /root/var /root/var.sav	# mv var if nonempty dir
		    ln -s usr/var /root/var	# make var a symlink to usr/var
		else
		    if test ! -d /root/var	# no usr/var, just use var
		    then
			rm /root/var		# rm var if non-directory
			mkdir /root/var		# make var dir
		    fi
		fi
	    else
		# if usr not mounted, just fix var as real directory

		if test ! -d /root/var
		then
		    rm /root/var		# rm var if non-directory
		    mkdir /root/var		# make var dir
		fi
	    fi
	) 1>&- 2>&-					    # quiet ..

    fi			# if fstab, mount that, else try usr part 6 .. fi

    # show user what's mounted

    mount -p | awk '
	    $3 == "efs" || $3 == "xfs" { printf "    %-23s  on  %s\n", $1, $2; }
    '
    echo
    echo

    ;;

rootonly)

    # Attempt to mount /root and return status
    test -d /root || mkdir /root > /dev/null 2>&1
    normrootroot=`devnm / | sed 's/. .*/0/'`	# Jam '0' on end of swap device
    mount -o mrquota -c $normrootroot /root
    exit $?
    ;;

unmntlocal)

    /sbin/umount -k /root/hw
    /sbin/umount -ak -b /proc,/,/hw
    sync
    ;;

mntany)

    shift
    /sbin/mount $*
    if [ ! -d /root/hw ]; then
        /bin/rm -rf /root/hw  > /dev/null 2>&1
        mkdir /root/hw
    fi

    # Only mount /root/hw if /root is mounted and /root/hw is not

    rootdev=`stat -qd /root`

    if [ -d /root/hw ] &&
       [ `stat -qd /` -ne "$rootdev" ] &&
       [ `stat -qd /root/hw` -eq "$rootdev" ] ; then
        /etc/mnthwgfs /root/hw
    fi
    ;;

unmntany)

    # Since it is very difficult to determine if we actually need
    # to unmount /root/hw (since /root may be specified in a
    # variety of ways), the easiest solution is to always unmount
    # /root/hw and then see if the root filesystem is still mounted.
    # If it is, remount /root/hw

    shift
    /sbin/umount -k /root/hw
    /sbin/umount $*
    if [ `stat -qd /` -ne `stat -qd /root` ] ; then
        /etc/mnthwgfs /root/hw
    fi
    ;;


*)
    echo Usage: mrmountrc '{mntlocal|unmntlocal|mntany dirs|unmntany dirs}'
    ;;

esac
