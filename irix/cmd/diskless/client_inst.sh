#! /bin/sh
#ident  "$Revision: 1.48 $"
#
#	client_inst
#
umask 022

. ./.dl_inst

usage()
{
	echo "$CMDNAME: -h host -r release -c class [-d]"
	echo "	-h host:	 create client tree for 'host', not needed for 'share'"
	echo "	-r release:	 use release 'release'"
	echo "	-c class:	 use class 'class', not needed for 'share'"
	echo "	-d:		 delete the host or release"
	exit 0
}

ACTION="create"
TARGET="client";
HOST="";

while [ "$1" != "" ]; do
	case "$1" in
	"-c")
		shift
		CLASS=$1
		;;
	"-r")
		shift
		RELEASE=$1
		;;
	"-h")
		shift
		HOST=$1;
		;;
	"-d")
		ACTION="delete"
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
if [ "$TARGET" = "" ]; then
	echo "$CMDNAME: ERROR: Missing target !\n"
	usage;
	exit 1;
fi;
if [ "$HOST" = "" ]; then
	echo "$CMDNAME: ERROR: Missing host name\n"
	usage;
	exit 1;
fi;
if [ "$HOST" = "$RELEASE" ]; then
	echo "$CMDNAME: ERROR: HOSTname can not be same as RELEASEname\n"
	usage;
	exit 1;
fi
if [ "$HOST" = "$CLASS" ]; then
	echo "$CMDNAME: ERROR: HOSTname can not be same as CLASSname\n"
	usage;
	exit 1;
fi

# Simple error checking for RELEASE
if [ ! -f $RELEASE.dat ]; then
	echo "$CMDNAME: ERROR: Invalid release argument."
        if [ `echo $RELEASE | grep "\.dat"` ]; then
		echo "$CMDNAME: Release argument does not need .dat suffix"
	else
		echo "$CMDNAME: $RELEASE.dat file does not exist"
	fi
	exit 1;
fi

# Simple error checking for CLASS
if [ ! -f $CLASS.dat ]; then
	echo "$CMDNAME: ERROR: Invalid class argument."
	if [ `echo $CLASS | grep "\.dat"` ]; then
		echo "$CMDNAME: Class argument does not need .dat suffix"
	else
		echo "$CMDNAME: $CLASS.dat file does not exist"
	fi
	exit 1;
fi

. $DIR/$RELEASE.dat
. $DIR/$CLASS.dat

# For backwards compatibility... just in case
MACH=${CPUBOARD}

if [ "$NFS_INSTALL" = "yes" ]; then
	echo "Performing client tree install over NFS."
fi

# SHARE set in RELEASE.dat, so we need to set DL_ROOT here
DL_ROOT=$CLROOT; export DL_ROOT

# leave for compatibility sakes.
if [ "$DLMAJOR" = "" ]; then
	DLMAJOR=18
fi

#
# Validate client name.
#	host_line: Trying to remember the line from /etc/hosts
#
# line should look like:
#
#	192.80.2.1	fully_qualified_name name alias1 alias2
#
# Algorithm for /etc/hosts: Look for #s.  These should correspond to
#	the beginning of a line in /etc/hosts.  From here, assume
#	the second argument after the IP address is the fully qualified
#	name.  Try and find an exact match before finding another
#	IP address.
#
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
			exit 1;
		fi;
	else
		# The following line can obtain an exact match!  The secret
		# is to remove comments before doing a grep with white space.
		match=`grep -v "^\#" /etc/hosts | sed "s/#.*//" | egrep '[ 	]'"$HOST"'([ 	]|$)' | head -1`
		if [ "$match" != "" ]; then
			host_line=${match};
			set $host_line;
			HOST=$2;
		else
			echo Host name $HOST not found in /etc/hosts
			exit 1;
		fi;
	fi;

#
# Use old directory even if client name is qualified differently.
#	Don't go past 2nd number entry in $host_line.
#
	cldir=`dirname "$CLROOT"`
	set $host_line
	while [ $# -gt 0 ] ; do
		if [ -d "$cldir/$1" ]; then
			CLROOT=$cldir/$1
			break;
		fi;
		shift;
		if [ $# -gt 0 ]; then
			if [ "`expr $1 : '[1-9][0-9]\{0,2\}\.[0-9]\{1,3\}.*'`" != "0" ]; then
				break;
			fi
		fi
	done;
fi;

BPKEY="$CLROOT/bpkey"

#
#

if [ "$ACTION" = "delete" ]; then
	poke_host "$HOST" "$SERVER_NIS" "$TARGET";
	HOSTSTAT=$?;
	case $HOSTSTAT in
	0)
		echo "$HOST is not diskless client for RELEASE $RELEASE"
		echo "Action aborted\n";
		exit 1;
		;;
	2)
	    	echo "\nWARNING: client $HOST is UP and RUNNING and may CRASH.\n"
		;;
	*)
		# do nothing
		;;
	esac

	echo "Remove client tree at $CLROOT (shared tree = $SHARE)"
	echo "Enter confirmation (y/Y)\07 :\c"
	read CONF
	if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
		echo "Action aborted\n";
		exit 1;
	fi;

	if [ ! -d $CLROOT ]; then
		echo "Client tree directory ($CLROOT) does not exist"
		echo "Do you want to remove remainder of client installation?"
		echo "Enter confirmation (y/Y)\07 :\c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "Action aborted\n";
			exit 1;
		fi;
		rm -rf $CLROOT
		mkdir $CLROOT
	fi;

	if [ "$NFS_INSTALL" != "yes" ]; then
		if [ -f $BPKEY -a "$SERVER_NIS" = "yes" ]; then
			$NIS_EXEC_DIR/yp_bootparam -d -h $HOST -k $BPKEY;
			rm $BPKEY
		elif [ "$SERVER_NIS" != "yes" -a -f /etc/bootparams ]; then
			sed -e /\^$HOST/d /etc/bootparams > /etc/bootparams.tmp
			mv /etc/bootparams.tmp /etc/bootparams
		fi
	else
		echo "Please remove the entry keyed on: $HOST"
		echo "  from the NFS server's bootparams mechanism."
		echo "  This is either the server's bootparams file"
		echo "  or the NIS bootparams map."
	fi
	# unexport_dir remove the client's root & swap
	unexport_dir host $CLROOT $HOST
	unexport_dir host $SWAP $HOST
	if [ "$SERVER_NIS" = "yes" ]; then
		echo "Do you want host $HOST removed from NIS(y/n)?\07 \c"
		read CONF
		if [ "$CONF" = "y" ]; then
			$NIS_EXEC_DIR/yp_host -d -h $HOST;
		fi;
	fi
	exit 0;
fi


#
# Install.
#

if [ "$HOST" = "" ]; then
	echo "Host name unspecified\n"
	exit 1;
fi
poke_host "$HOST" "$SERVER_NIS" "$TARGET";
HOSTSTAT=$?;
NEWINST="false"
case $HOSTSTAT in
2)
    	echo "\nWARNING: client $HOST is UP and RUNNING and may CRASH.\n"
	;;
*)
	# do nothing
	;;
esac
	echo "\nClient tree = $CLROOT, shared tree = $SHARE"
if [ `expr "${SHAREHOST}:${CLROOT}" : '.*'` -gt 37 ]; then
	RP="${SHAREHOST}:${CLROOT}"
	echo "WARNING: Client tree($RP) is longer than 37 chars.  Some older machines"
	echo "         cannot store more than 37 characters in one NVRAM variable."
fi;
echo "Enter confirmation (y/Y)\07 :\c"
read CONF
if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
	echo "Action aborted\n";
	exit 1;
fi
if [ ! -d $CLROOT ]; then
	mkdir -p $CLROOT;
	NEWINST="true"
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

pick_inst


$INST -r$CLROOT $INST_OPT -S $SHARE -mGFXBOARD=$GFXBOARD \
	-mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD -mVIDEO=$VIDEO \
	-mMODE=$MODE

# /sbin is mounted at boot time.  Need a mount point.
if [ ! -d $CLROOT/sbin ]; then
	mkdir -p $CLROOT/sbin;
fi

# /usr is mounted at boot time.  Need a mount point.
if [ ! -d $CLROOT/usr ]; then
	mkdir -p $CLROOT/usr;
fi

# /var/share is mounted at boot time.  Need a mount point.
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
	# If sed allow other symbols, we could write this as:
	#	-e '\@[ \t]/[ \t]+nfs[ \t]@d'  with \t = tab
	#
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

# If the file isn't present or this is a new install, then create /etc/hosts
if [ ! -f $CLROOT/etc/hosts -o "$NEWINST" = "true" ]; then
	if [ "$SERVER_NIS" = "yes" ]; then
		ypcat hosts > $CLROOT/etc/hosts;
	else
		cp /etc/hosts $CLROOT/etc
	fi
fi

#
# Do I need to rebuild the kernel?
# If unix does not exist, then this is probably an initial
# client setup.  We'll create a file w/an old time stamp
# to ensure we generate one via the REBUILD checking below.
#
if [ ! -f $CLROOT/unix ]; then
        touch -t 197001010000 $CLROOT/unix;
fi

REBUILD=`find $SHARE/usr/cpu/sysgen/${CPUBOARD}boot/ -newer $CLROOT/unix -print | wc | awk '{print $1}'`
if [ $REBUILD != 0 ]; then

	# Need to make a complete tree, so make symlink to $SHARE/usr
	echo "\nBuilding kernel ..."
	mv $CLROOT/usr $CLROOT/usr.tmp
	ln -s $SHARE/usr $CLROOT/usr > /dev/null 2>&1

	# $CLROOT/var/sysgen/boot is a symlink.  Need to find it out before
	# an lboot can be performed.  The following code strips preceding
	# ../ from the beginning of a symlink.  This assumes, of course, that
	# a relative symlink will .. up to root.  Either we do this, or we
	# assume that the two valid forms of the symlink are absolute or
	# relative with ../../ preceding the relative symlink.  Remember
	# that diskless systems don't have the symlink from /var -> usr/var.
	# Therefore, the relative symlink only needs 2 ..'s.  IRIX 5.1 used
	# the absolute symlink.
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

# Make /usr/lib/libc.so.1, and and the same for lib32 as well as lib64
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
echo "Done with client install"
