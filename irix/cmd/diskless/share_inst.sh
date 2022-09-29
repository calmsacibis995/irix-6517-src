#! /bin/sh
#ident  "$Revision: 1.37 $"
#
#	share_inst
#
umask 022

. ./.dl_inst

usage()
{
	echo "$CMDNAME: -r release [-d]"
	echo "	-r release:	use release 'release'"
	echo "	-d:		delete the release"
	exit 0
}

ACTION="create"
TARGET="share";

while [ "$1" != "" ]; do
	case "$1" in
	"-r")
		shift
		RELEASE=$1
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
if [ "$TARGET" = "" ]; then
	echo "$CMDNAME: ERROR: Missing target !\n"
	usage;
	exit 1;
fi;

# Simple error checking for RELEASE
if [ ! -f $RELEASE.dat -a ! -l $RELEASE.dat -a ! -d $RELEASE.dat ]; then
	echo "$CMDNAME: ERROR: Invalid release argument."
	if [ `echo $RELEASE | grep "\.dat"` ]; then
		echo "$CMDNAME: Release argument does not need .dat suffix"
	else
		echo "$CMDNAME: $RELEASE.dat file does not exist"
	fi
	exit 1;
fi

. $DIR/$RELEASE.dat

# For backwards compatibility... just in case
MACH=${CPUBOARD}

if [ "$DLMAJOR" = "" ]; then
	DLMAJOR=18
fi


SHKEY="$DIR/$RELEASE.bpkey"

if [ "$NFS_INSTALL" = "yes" ]; then
	echo "Performing share tree install over NFS."
fi

#
#

if [ "$ACTION" = "delete" ]; then
	poke_host "$RELEASE" "$SERVER_NIS" "$TARGET";
	HOSTSTAT=$?;
	case $HOSTSTAT in
	0)
		echo "Diskless release $RELEASE does not exist"
		echo "Action aborted\n";
		exit 1;
		;;
	1)
		poke_class "${SHAREHOST}:${SHARE}" "$SERVER_NIS" "delete" "$RELEASE";
		HOSTSTAT=$?;
		if [ "$HOSTSTAT" -eq 1 ]; then
			echo "Action aborted\n";
			exit 1;
		fi;
		;;
	*)
		echo "share_inst: INTERNAL ERROR $HOSTSTAT"
		exit 1
		;;
	esac
		if [ ! -d $SHARE ]; then
		echo "$CMDNAME: ERROR: Share tree not exist"
		exit 1;
	fi;
	echo "About to remove shared tree at $SHARE......"
	echo "Enter confirmation (y/Y)\07 :\c"
	read CONF
	if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
		echo "Action aborted\n";
		exit 1;
	fi;
	if [ "$NFS_INSTALL" != "yes" ]; then
		if [ -f $SHKEY -a "$SERVER_NIS" = "yes" ]; then
			$NIS_EXEC_DIR/yp_bootparam -b -d -h $RELEASE -k $SHKEY;
			rm $SHKEY;
		elif [ "$SERVER_NIS" != "yes" -a -f /etc/bootparams ]; then
			sed -e /\^$RELEASE\ /d /etc/bootparams > /etc/bootparams.tmp
			mv /etc/bootparams.tmp /etc/bootparams
		fi;
	else
		echo "Please remove the entry keyed on: $RELEASE"
		echo "  from the NFS server's bootparams mechanism."
		echo "  This is either the server's bootparams file"
		echo "  or the NIS bootparams map."
	fi

	# remove the share tree's /usr and /sbin directories
	unexport_dir class $SHARE/usr $RELEASE
	unexport_dir class $SHARE/sbin $RELEASE
	rm -r $SHARE

	exit 0;
fi


#
# Install.
#
poke_host "$RELEASE" "$SERVER_NIS" "$TARGET";
HOSTSTAT=$?;
case $HOSTSTAT in
1)
	poke_class "${SHAREHOST}:${SHARE}" "$SERVER_NIS" "$ACTION" "$RELEASE";
	HOSTSTAT=$?;
	if [ "$HOSTSTAT" -eq 1 ]; then
		echo "Action aborted\n";
		exit 1;
	fi;
	;;
*)
	# do nothing
esac
echo "About to install shared tree at $SHARE......"
echo "Enter confirmation (y/Y)\07:\c"
read CONF
if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
	echo "Action aborted\n";
	exit 1;
fi
if [ "$HOSTNAME" != "$SHAREHOST" ]; then
	echo "Share tree must be built on host $SHAREHOST"
	exit 1;
fi

# Create $SHARE/lib, so /lib/* files can stored as /sbin/lib/* files
if [ ! -d $SHARE/sbin/lib ]; then
	rm -f $SHARE/sbin/lib;
	mkdir -p $SHARE/sbin/lib;
fi
ln -s sbin/lib $SHARE > /dev/null 2>&1

# Create $SHARE/lib32, so /lib32/* files can stored as /sbin/lib32/* files
if [ ! -d $SHARE/sbin/lib32 ]; then
	rm -f $SHARE/sbin/lib32;
	mkdir -p $SHARE/sbin/lib32;
fi
ln -s sbin/lib32 $SHARE > /dev/null 2>&1

# Make /bin a symlink to usr/bin, so /bin/sh will point to something
if [ ! -l $SHARE/bin ]; then
	rm -f $SHARE/bin
	ln -s usr/bin $SHARE/bin
fi

# Make /etc a symlink to /sbin, so commands like /etc/mknod will exist
if [ ! -l $SHARE/etc ]; then
	rm -f $SHARE/etc
	ln -s sbin $SHARE/etc
fi

# Make /dev/ devices which are required when chroot is done by exitops in inst.
mkdir -p $SHARE/dev > /dev/null 2>&1
mknod $SHARE/dev/null c 1 2 > /dev/null 2>&1
chmod 666 $SHARE/dev/null > /dev/null 2>&1
mknod $SHARE/dev/zero c 37 0 > /dev/null 2>&1
chmod 666 $SHARE/dev/zero > /dev/null 2>&1
mknod $SHARE/dev/usema c 47 1 > /dev/null 2>&1
chmod 777 $SHARE/dev/usema > /dev/null 2>&1
mknod $SHARE/dev/usemaclone c 47 0 > /dev/null 2>&1
chmod 777 $SHARE/dev/usemaclone > /dev/null 2>&1

# Create a few directories, so we don't have dangling symlinks.  Delete
# this stuff once we no longer have maint images.
mkdir -p $SHARE/var/spool > /dev/null 2>&1
mkdir -p $SHARE/var/X11/xdm > /dev/null 2>&1

# Let the user pick an installation tool *new*

pick_inst

# Now perform installation

SHARED_DIRS="-d usr -d sbin -d lib -d lib32 -d var/share"
if [ "${GFXBOARD}_" = "_" -a "${CPUARCH}_" = "_" -a \
     "${CPUBOARD}_" = "_" -a "${MACH}_" = "_" ]; then
	# Install every architecture
	$INST -r $SHARE -s $INST_OPT -C $SHARED_DIRS -mMODE=$MODE
elif [ "${GFXBOARD}_" != "_" -a "${CPUARCH}_" != "_" -a \
       "${CPUBOARD}_" != "_" -a "${MACH}_" != "_" ]; then
	# Install selected architecture
	$INST -r $SHARE -s $INST_OPT $SHARED_DIRS \
		-mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD \
		-mGFXBOARD=$GFXBOARD -mVIDEO=$VIDEO -mMODE=$MODE
else
	echo "ERROR: $RELEASE.dat is inconsistent."
	echo "	GFXBOARD, CPUARCH, CPUBOARD must all be set or unset"
	exit 1
fi

# Create /var/share if it doesn't exist.  It may be a user created symlink, so
# don't delete.
if [ ! -d $SHARE/var/share ]; then
	mkdir -p $SHARE/var/share
fi

# Export /usr and /sbin
echo "Exporting directories ..."
export_share_dir $SHARE/usr ro $RELEASE 
export_share_dir $SHARE/sbin ro $RELEASE 
export_share_dir $SHARE/var/share rw $RELEASE

if [ "$NFS_INSTALL" = "yes" ]; then
 	echo "\nPlease put the following line in the NFS server's bootparams file\n"
	echo "	$RELEASE root=$SHAREHOST:$SHARE sbin=$SHAREHOST: swap=$SHAREHOST:\nn"
	echo "	or put it into the NIS bootparams map."
	exit
fi

echo "Creating bootparams entries ..."
if [ "$SERVER_NIS" = "yes" ]; then
	if [ -f $SHKEY ]; then
		$NIS_EXEC_DIR/yp_bootparam -d -h $RELEASE -b -k $SHKEY;
		rm $SHKEY
	fi
	$NIS_EXEC_DIR/yp_bootparam -a -h $RELEASE -k $SHKEY -b \
		root=$SHAREHOST:$SHARE \
		sbin=$SHAREHOST: swap=$SHAREHOST:
else
	sed -e /\^$RELEASE\ /d /etc/bootparams > /etc/bootparams.tmp
	mv /etc/bootparams.tmp /etc/bootparams
	echo "$RELEASE root=$SHAREHOST:$SHARE sbin=$SHAREHOST: swap=$SHAREHOST:" >> /etc/bootparams
fi;
#
# Creating whatis database
#
MAN=$SHARE/usr/share/catman:$SHARE/usr/share/man:$SHARE/usr/catman:$SHARE/usr/man:$SHARE/usr/local/man
set `/sbin/stat -mq $SHARE/var/inst/hist $SHARE/usr/share/catman/whatis 2>/dev/null` > /dev/null
if [ ! -s /usr/share/catman/makewhatis -o \( "$2" -a "$1" -gt "$2" \) ]
then
        logger -t Diskless client makewhatis man page database out of date with install history
        ( /sbin/nice -20 /usr/lib/makewhatis -M $MAN $SHARE/usr/share/catman/whatis 2>&1 | logger -t makewhatis 
	 logger -t Diskless client makewhatis man page database build finished ) &
fi

echo "Done with share tree install.\n"
echo "NOTE: Please read the NFS release notes for additional information"
echo "	which was not included in the administration guide.  As an"
echo "	example, diskless systems require extra configuration with home"
echo "	directories."
