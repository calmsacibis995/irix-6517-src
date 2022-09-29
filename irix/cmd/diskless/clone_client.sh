#!/bin/sh 
#ident	$Revision: 1.5 $
#
#	clone_client
#
umask 022
. ./.dl_inst
AUTOMATIC="false"

usage()
{
	echo "$CMDNAME: -f filename -r release -c class -a -clone src_root"
	echo "	-f filename:		use filename to determine the hosts to install"
	echo "	-r release:		use release 'release'"
	echo "	-c class:		use class 'class', not needed for 'share'"
	echo "  -a:			install without user interaction (best if "
	echo "				doing installs over the network)"
	echo "	-clone src_root:	clone client/share tree from this full-path"
	exit 0
}

while [ "$1" != "" ]; do
	case "$1" in 
	"-f")
		shift
		HOSTFILE=$1
		;;
	"-r")
		shift
		RELEASE=$1
		;;
	"-c")
		shift
		CLASS=$1
		;;
	"-clone")
		shift
		CLONE_DIR=$1
		;;
	"-a")
		AUTOMATIC="true"
		;;
	*)
		usage;
		;;
	esac
	shift;
done

if [ "$RELEASE" = "" ]; then
	echo "$CMDNAME: ERROR: Release not supplied !\n"
	usage;
	exit 1;
fi;
if [ "$CLASS" = "" ]; then
	echo "$CMDNAME: ERROR: Class not supplied !\n"
	usage;
	exit 1;
fi;
if [ "$HOSTFILE" = "" ]; then
	echo "$CMDNAME: ERROR: Hostfile not supplied !\n"
	usage;
	exit 1;
fi;

# Simple error checking for RELEASE

if [ ! -f $RELEASE.dat ]; then
	echo "$CMDNAME: ERROR: Invalid release argument."
	if [ `echo $RELEASE | grep "\.dat"` ]; then
		echo "$CMDNAME: Release argument does not need .dat suffix"
	else
		echo "$CMDNAME: $CLASS.dat file does not exist"
	fi
	exit 1;
fi

# For backwards compatibility... just in case
MACH=${CPUBOARD}

if [ "$CLONE_DIR" != "" ]; then
	if [ ! -d "$CLONE_DIR" ]; then
		echo "$CMDNAME: ERROR: Clone argument is not a directory\n"
		usage;
		exit 1;
	fi
fi

if [ "$NFS_INSTALL" = "yes" ]; then
	echo "Performing client tree cloning over NFS."
fi

if [ ! -f $HOSTFILE ]; then
	echo "$CMDNAME: ERROR: $HOSTFILE does not exist\n"
	usage;
	exit 1;
fi

INST="/usr/sbin/inst"

#Iterate through the list and do an automatic install for each one!

for HOST in `cat $HOSTFILE`
do
	. $DIR/$RELEASE.dat
	. $DIR/$CLASS.dat
	NEWINST="true"
	DL_ROOT=$CLROOT; export DL_ROOT
	if [ "$DLMAJOR" = "" ]; then
		DLMAJOR=18
	fi
	if [ "$HOST" = "" ]; then
		continue
	fi
	if [ `echo $HOST | grep "^\#"` ]; then
		continue
	fi 
	if [ "$HOST" != "" ]; then
		SYSID="$HOST"
		if [ "$SERVER_NIS" = "yes" ]; then
			ypmatch $HOST hosts > /dev/null 2>&1
			if [ $? = "0" ]; then
				match="`ypmatch $HOST hosts`";
				host_line=${match};
				set $host_line;
				HOST=$2;
			else 
				echo "Invalid host name - $HOST"
				continue
			fi;
		else
			match=`grep -v "^\#" /etc/hosts | sed " s/#.*//" | egrep '[	 ]'"$HOST"'([ 	]|$)' | head -1`
			if [ "$match" != "" ]; then
				host_line=${match};
				set $host_line;
				HOST=$2;
			else
				echo "Host name $HOST not found in /etc/hosts"
				continue
			fi;
		fi;
	else
		cldir=`dirname "$CLROOT"`
		set $host_line
		while [ $# -gt 0 ]; do
			if [ -d "$cldir/$1" ]; then
				CLROOT=$cldir/$1
				break;
			fi;
			shift;
			if [ $# -gt 0 ]; then
				if [ "`expr $1 : '[1-9][0-9]\{0,2\}\.[0-9]\{1,3\}.*'`" != "0" ]; then
					break;
				fi;
			fi;
		done;
	fi;
	BPKEY="$CLROOT/bpkey"
	poke_host "$HOST" "$SERVER_NIS" "$TARGET";
	HOSTSTAT=$?;
	case $HOSTSTAT in 
		2)
			echo "\nWARNING: client $HOST is UP and RUNNING and may CRASH.\n"
			;;
		*)
			# do nothing 
			;;
	esac
	echo "\nClient tree = $CLROOT, share tree = $SHARE"
	if [ `expr "${SHAREHOST}:${CLROOT}" : '.*'` -gt 37 ]; then
		RP="${SHAREHOST}:${CLROOT}"
		echo "WARNING: Client tree \($RP\) is longer than 37 chars.  Some older machines"
		echo "	cannot store more than 37 characters in one NVRAM variable."
	fi;
	if [ ! -d $CLROOT ]; then 
		mkdir -p $CLROOT;
	fi
	export_client_dir $CLROOT $HOST noswap
	echo "$HOSTNAME:$CLROOT / nfs rw 0 0"		> $DIR/fstab.$HOST
	echo "$SHAREHOST:$SHARE/usr /usr nfs ro 0 0"	>> $DIR/fstab.$HOST
	echo "$SHAREHOST:$SHARE/sbin /sbin nfs ro 0 0"	>> $DIR/fstab.$HOST
	echo "$SHAREHOST:$SHARE/var/share /var/share nfs rw 0 0" >> $DIR/fstab.$HOST
	echo "root=$HOSTNAME:$CLROOT"			> $DIR/bootparam.$HOST
	echo "sbin=$SHAREHOST:$SHARE/sbin"		>> $DIR/bootparam.$HOST
	echo "usr=$SHAREHOST:$SHARE/usr"		>> $DIR/bootparam.$HOST
	echo "var_share=$SHAREHOST:$SHARE/var/share"	>> $DIR/bootparam.$HOST
	if [ "$SWAP" = "" -o "$SWAPSIZE" = "0" -o "$SWAPSIZE" = "0k" -o \
		"$SWAPSIZE" = "0b" -o "$SWAPSIZE" = "0m" ]; then
		rm -rf $CLROOT/swap;
		rm -f $SWAP/_swap;
		echo "swap=$HOSTNAME:" >> $DIR/bootparam.$HOST;
	else
		if [ ! -d $CLROOT/swap ]; then
			mkdir -p $CLROOT/swap;
		fi
		echo "$HOSTNAME:$SWAP /swap nfs rw 0 0" >> $DIR/fstab.$HOST
		echo "swap=$HOSTNAME:$SWAP" >> $DIR/bootparam.$HOST;

		if [ ! -d $SWAP ]; then
			mkdir -p $SWAP;
		fi;
		if [ ! -f $MKFILE ]; then
			echo "$MKFILE does not exit\n" > /dev/console;
			echo "$MKFILE does not exit\n"
			exit 1;
		fi;
		SWAP_EXIST="no"
		if [ -f $SWAP/_swap ]; then
			set `ls -s $SWAP/_swap`
			SWAP_SZ="` expr $1 / "2048" `m"
			if [ "$SWAP_SZ" = "$SWAPSIZE" ]; then
				SWAP_EXIST="yes"
			fi;
		fi;
		if [ "$SWAP_EXIST" = "no" ]; then
			echo "Create $SWAPSIZE swap file ........"
			$MKFILE $SWAPSIZE $SWAP/_swap;
		fi;
		export_client_dir $SWAP $HOST swap;
	fi;
	if [ "$NFS_INSTALL" != "yes" ]; then
		if [ "$SERVER_NIS" = "yes" ]; then
			if [ -f $BPKEY ]; then
				$NIS_EXEC_DIR/yp_bootparam -d -h $HOST -k $BPKEY;
			fi
			$NIS_EXEC_DIR/yp_bootparam -a -h $HOST -k $BPKEY \
				-f $DIR/bootparam.$HOST
			rm $DIR/bootparam.$HOST
		else
			sed -e /\^$HOST/d /etc/bootparams > /etc/bootparams.tmp
			mv /etc/bootparams.tmp /etc/bootparams
			set `cat $DIR/bootparam.$HOST`
			echo "$HOST $*" >> /etc/bootparams
			rm $DIR/bootparam.$HOST
		fi
	else
		echo "\nPlease put the following line in the NFS server's bootparams file\n"
		set `cat $DIR/bootparam.$HOST`
		echo "	$HOST $*"
		echo "  or put it into the NIS bootparams map."
	fi
	
	#Now get all of the software that was installed into the client to clone
	#Now lets check some things:
	#   Is this a new install?  If so then check to see if the directory to
	#   be cloned is a client_tree.  If so, then we can just copy the tree and 
	#   skip the inst stuff.  Else use inst to install all of the necessary stuff  	
	
	if [ ! -d $CLROOT/var/inst -a -f $CLONE_DIR/var/inst/.client ]; then
		
		#Just copy the directories
		
		cd $CLONE_DIR
		find . -depth -print | cpio -pdm $CLROOT
		cd $DIR
	else
		showprods -3Fn -r $CLONE_DIR | sort -u | nawk '{ if (/^I/) print $0 }' > .selections1

		if [ ! -f .selections1 ]; then
			echo "Problem executing showprods...Exiting"
			exit 1
		fi

		if [ -d $CLROOT/var/inst ]; then
			NEWINST="false"
			showprods -3Fn -r $CLROOT | sort -u > .selections2
			if [ -f .selections2 ]; then
				echo "Problem executing showprods...Exiting"
				exit 1
			fi
			diff .selections1 .selections2 | nawk '{ if (NF == 5) print $0 }' > .selectionstemp
			rm -f .selections2
		else
			nawk '{ if (NF == 4) printf "< %s\n",$0 }' .selections1 > .selectionstemp
		fi

		rm -f .selections1
		nawk '{ if (/^\</) printf "%s\n",$5 }' .selectionstemp | sort -u | nawk '{ printf "-f %s ",$1 }' > .location

		if [ `wc .location | awk '{print $1}'` -ne 0 ]; then
			echo "-f none" > .location
		fi 
	
		echo "k conflicting" > .selections
		nawk '{ if (/^\>/) printf "r %s %s\n",$3,$4 }' .selectionstemp | sort -u >> .selections
		nawk '{ if (/^\</) printf "i %s %s\n",$3,$4 }' .selectionstemp | sort -u >> .selections 
		if [ "$AUTOMATIC" = "true" ]; then
			$INST -r $CLROOT $INST_OPT -a -f `cat .location` -S $SHARE -mGFXBOARD=$GFXBOARD \
				-mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD -mVIDEO=$VIDEO \
				-mMODE=$MODE -F .selections 
		else
			$INST -r $CLROOT $INST_OPT -f `cat .location` -S $SHARE -mGFXBOARD=$GFXBOARD \
				-mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD -mVIDEO=$VIDEO \
				-mMODE=$MODE -F .selections 
		fi

		# Cleanup

		rm -f .selections .location .selectionstemp
	fi

	# /sbin is mounted at boot time.  Need a mount point.

	if [ ! -d $CLROOT/sbin ]; then
		mkdir -p $CLROOT/sbin;
	fi

	# /usr is mounted during boot time.  Need a mount point.
	if [ ! -d $CLROOT/usr ]; then
		mkdir -p $CLROOT/usr;
	fi

	# /var/share is mounted during boot time.  Need a mount point.
	if [ ! -d $CLROOT/var/share ]; then
		mkdir -p $CLROOT/var/share;
	fi

	# If the client is up and running, don't munge the mtab file.
	if [ ${HOSTSTAT} != 2 ]; then
		cp $DIR/fstab.$HOST $CLROOT/etc/mtab;
	fi

	if [ ! -f $CLROOT/etc/fstab ]; then
		mv $DIR/fstab.$HOST $CLROOT/etc/fstab;
	else
		sed	-e '\@[ 	]/[	 ][	 ]*nfs[ 	]@d' \
			-e '\@[ 	]/usr[	 ][	 ]*nfs[ 	]@d' \
			-e '\@[ 	]/swap[	 ][	 ]*nfs[ 	]@d' \
			-e '\@[ 	]/sbin[	 ][	 ]*nfs[ 	]@d' \
			-e '\@[ 	]/var/share[	 ][	 ]*nfs[ 	]@d' \
		$CLROOT/etc/fstab > $CLROOT/etc/fstab.tmp
		mv $DIR/fstab.$HOST $CLROOT/etc/fstab;
		cat $CLROOT/etc/fstab.tmp >> $CLROOT/etc/fstab;
		rm $CLROOT/etc/fstab.tmp
	fi
	if [ ! -d $CLROOT/tmp ]; then
		mkdir -m 777 $CLROOT/tmp;
	fi
	echo on > $CLROOT/etc/config/nfs
	echo on > $CLROOT/etc/config/network
	echo on > $CLROOT/etc/config/verbose
	echo on > $CLROOT/etc/config/lockd

	# If the file isn't present or it is a new install, then create /etc/hosts
	if [ ! -f $CLROOT/etc/hosts -o "$NEWINST" = "true" ]; then
		if [ "$SERVER_NIS" = "yes" ]; then
			ypcat hosts > $CLROOT/etc/hosts;
		else
			cp /etc/hosts $CLROOT/etc
		fi
	fi
	
	# Do I need to rebuild the kernel???  If so, do it only once and copy 
	# it to .unix so we build one generic kernel and copy it for each 
	# machine!
	
	if [ ! -f .unix ]; then
		if [ ! -f $CLROOT/unix ]; then
			touch -t 197001010000 $CLROOT/unix;
		fi

		REBUILD=`find $SHARE/usr/cpu/sysgen/${CPUBOARD}boot/ -newer $CLROOT/unix -print | wc | awk '{print $1}'`
		if [ $REBUILD != 0 ]; then
			echo "\nBuilding kernel for $HOST..."
			mv $CLROOT/usr $CLROOT/usr.tmp
			ln -s $SHARE/usr $CLROOT/usr > /dev/null 2>&1
			if [ -l $CLROOT/var/sysgen/boot ]; then
				symlink=`/bin/ls -l $CLROOT/var/sysgen/boot|awk '{print $11}'|sed "s@\.\.\/@@g"`;
				BOOT=$SHARE/$symlink
			fi
			$SHARE/usr/sbin/lboot -u $CLROOT/unix -r $CLROOT -b $BOOT \
				-s $CLROOT/var/sysgen/system.dl -c $CLROOT/var/sysgen/stune \
				-n $CLROOT/var/sysgen/mtune
			if [ $? -ne 0 ]; then
				echo "Failed to build kernel..."
				echo "Check all error messages then run client_inst again."
				exit 1
			fi
			rm -f $CLROOT/usr
			mv $CLROOT/usr.tmp $CLROOT/usr
			touch -m 0101000070 $CLROOT/unix
			cp $CLROOT/unix .unix
		fi
	else
		echo "Copying generated kernel for $HOST"
		cp .unix $CLROOT/unix
		touch -m 0101000070 $CLROOT/unix
	fi
	if [ "$NISDOMAIN" != "" ]; then
		mkdir -p ${CLROOT}${NIS_DIR} > /dev/null 2>&1
		echo $NISDOMAIN > ${CLROOT}${NIS_DIR}/ypdomain 
	elif [ -x /usr/bin/domainname ]; then
		if [ ! -f ${CLROOT}${NIS_DIR}/ypdomain ]; then
			mkdir -p ${CLROOT}${NIS_DIR} > /dev/null 2>&1
			/usr/bin/domainname > ${CLROOT}${NIS_DIR}/ypdomain
		fi
	fi;

	# $CLROOT/lib is in $SHARE/sbin/lib so make symlinks - same for lib32
	rm -rf $CLROOT/lib $CLROOT/lib32
	ln -s sbin/lib $CLROOT/lib
	ln -s sbin/lib32 $CLROOT/lib32

	# Make /usr/lib/libc.so.1, and and the lsame for lib32
	mkdir -p $CLROOT/usr/lib > /dev/null 2>&1
	if [ ! -l $CLROOT/usr/lib/libc.so.1 -a ! -f $CLROOT/usr/lib/libc.so.1 ]; then
		ln -s ../../lib/libc.so.1 $CLROOT/usr/lib/libc.so.1
	fi
	mkdir -p $CLROOT/usr/lib32 > /dev/null 2>&1
	if [ ! -l $CLROOT/usr/lib32/libc.so.1 -a ! -f $CLROOT/usr/lib32/libc.so.1 ]; then
		ln -s ../../lib32/libc.so.1 $CLROOT/usr/lib32/libc.so.1
	fi
	if [ "$MODE" = "64bit" ]; then
		mkdir -p $CLROOT/usr/lib64 > /dev/null 2>&1
		if [ ! -l $CLROOT/usr/lib64/libc.so.1 -a ! -f $CLROOT/usr/lib64/libc.so.1 ]; then
			ln -s ../../lib64/libc.so.1 $CLROOT/usr/lib64/libc.so.1
		fi
	fi

	echo "$SYSID" > $CLROOT/etc/sys_id

#
# Create symlinks under /var/boot.
# This gets a little trickier for 6.2 because of the 32-bit ELF
# kernel support.  The IP{20,22} platforms do not have
# ELF aware PROMs and so sash must be used as an intermediate
# kernel loader.  For these platforms, we create a link from unix
# to sash, which in turn will look for unix.auto (which is a link
# to the "real" kernel file).
#
	if [ "$NFS_INSTALL" != "yes" ]; then
		BDNAME=${DIR}/`basename "$CLROOT"`
		if [ -l "$BDNAME" ]; then
			rm -f "$BDNAME"
		else
			rm -rf "$BDNAME"
		fi
		mkdir "$BDNAME"

		match="`versions -r $CLROOT eoe1.sw.unix|grep eoe1.sw.unix 2>/dev/null`"
		if [ -z "$match" ]; then
		#
		# Client is running 6.2 so we need to boot unix through sash
		# Be careful here...the target of the unix symlink has to
		# point to the right version of sash (see nfs.idb).
		#
			if [ "$CPUBOARD" = "IP20" -o "$CPUBOARD" = "IP22" ]
			then
				ln -sf "$SHARE"/sbin/stand/sash.ARCS "$BDNAME"/unix
				ln -sf "$CLROOT"/unix "$BDNAME"/unix.auto
			else
				ln -sf "$CLROOT"/unix "$BDNAME"
			fi
		else
			# Client is running pre 6.2
			ln -sf "$CLROOT"/unix "$BDNAME"
		fi

		# lets see if they installed eoe.sw.kdebug.  If so, let's be
		# nice and make a link to symmon so it won't have to be copied

		match="`versions -r $CLROOT eoe.sw.kdebug | grep kdebug 2>/dev/null`"
		if [ -n "$match" ]; then
			ln -sf "$SHARE"/sbin/stand/symmon."$CPUBOARD" "$BDNAME"/symmon."$CPUBOARD"
		fi
		# add a short README 
		echo "DO NOT ALTER THE SYMLINKS CONTAINED IN THIS DIRECTORY!" >$BDNAME/README
		echo "They are required for proper booting of ELF kernels" >>$BDNAME/README
	fi;
	echo "Built client tree for $HOST"
done
rm -rf .unix
echo "Finished creating client trees in $HOSTFILE"
