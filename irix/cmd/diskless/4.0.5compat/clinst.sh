#! /bin/sh
#
#
HOSTNAME="`hostname`"
#
# Following variables should be read from file /var/boot/4.X/$CLASS.dat
#
#DISKLESS="/usr/tmp"
#CLROOT="$DISKLESS/$HOST"
#SHAREHOST="$HOSTNAME"
#SHARE="$DISKLESS/$CLASS"
#SWAP="$DISKLESS/swap/$HOST"
#SWAPSIZE="10m"
#GFXBOARD="ECLIPSE"
#CPUBOARD="IP6"
#CPUARCH="R3000"
#MACH="IP6"
#BOOTP_DIR="/usr/local/boot"
#NIS="yes"
#DLMAJOR=18
#
SHELL="/bin/sh"
MKFILE="/etc/mkfile"
DISKLESS_DIR="/var/boot/4.X"
CLIENT_NIS_DIR="/usr/etc/yp"
INST="./inst4.0.5"
SGI_CC=-cckr
export SGI_CC

#
#

CMDNAME=$0
TARGET=""

#
# CMDMODE is used to talk with cl_init
#
CMDMODE="no"
MINUSR=""

SENDCLASS="\${CLASS}"
SENDHOST="\${HOST}"
NOWECHO="\${ECHO}"

#
# Check the SHARE-directory is server's root.
#
#	Arguments: 1 -- SHARE directory
#
chk_sroot()
{
	if [ "$1" = "/" ]; then
		return 1;
	fi;

	set `ls -l "$1"`;

	if [ "$1" != "l---------" ]; then
		return 0;
	fi;

	if [ $# -ne 11 ]; then
		echo "INTERNAL ERROR"
		exit 99;
	fi;

	shift; shift;
	if [ $9 != "/" ]; then
		return 0;
	fi;

	return 1;
}

#
# Update exports on server
#
#	arguments: 1 -- export directory.
#		   2 -- r/w option.
#		   3 -- class/client KEY.
#		   4 -- access right(client only).
#
export_dir()
{
	if [ ! -f /etc/exports ]; then
		> /etc/exports
		chmod 644 /etc/exports
	fi
	grep $1 /etc/exports >/dev/null 2>&1
	case $? in
	1)
		if [ "$4" = "" ]; then
			echo "$1 -$2,anon=root	#class=$3" \
				>> /etc/exports
		else
			echo "$1 -$2,anon=root,access=$4	#host=$3" \
				>> /etc/exports
		fi;;
	esac

	if [ -x /usr/etc/exportfs ]; then
		/usr/etc/exportfs $1
		case $? in
		0)
			;;
		*)
			echo "ATTENTION: fix /etc/exports and rerun exportfs !" >> /dev/console
			exit 1
			;;
		esac
	else
		echo "ATTENTION: /usr/etc/exportfs does not exist !" >> /dev/console
		exit 1
	fi
}


#
#	Unexports directories on server.
#
#
#		Arguments: 1 -- root directory
#			   2 -- key
#			   3 -- swap dir(client only)
#
unexport_dir()
{
	if [ ! -f /etc/exports ]; then
		echo "ATTENTION: /usr/etc/exportfs does not exist !" >> /dev/console
		exit 1
	fi;

	/usr/etc/exportfs -u "$1"
	if [ "$3" = "" ]; then
		sed -e /\#class=$2/d /etc/exports > /etc/export.$$
	else
		/usr/etc/exportfs -u "$3"
		sed -e /\#host=$2/d /etc/exports > /etc/export.$$
		rm -r $3
	fi;

	rm -r $1
	mv /etc/export.$$ /etc/exports
}


#
#	poke_host
#
#		Argument: 1 -- hostname.
#			  2 -- NIS option.
#			  3 -- TARGET.
#
#		return:   0 -- host is neither CLASS not CLIENT.
#			  1 -- host is not running.
#			  2 -- host in running.
#
poke_host()
{
	POKEHOST="$1";
	POKENIS="$2";
	POKETARGET="$3";

	RETVAL=0;
	if [ "$POKENIS" = "yes" ]; then
		ypmatch "$POKEHOST" bootparams > /dev/null 2>&1;
		if [ $? -eq 0 ]; then
			RETVAL=1;
		fi;
	fi;
	if [ "$POKENIS" != "yes" -o "$RETVAL" -eq 0 ]; then
		if [ ! -f /etc/bootparams ]; then
			return 0;
		fi
		grep "$POKEHOST" /etc/bootparams | while read a_line; do
			set $a_line;
			if [ "$1" = "$POKEHOST" ]; then
				return 1;
			fi;
		done

		RETVAL=$?;
	fi;

	if [ "$POKETARGET" = "share" -o "$RETVAL" -eq 0 ]; then
		return "$RETVAL";
	fi

	if [ `ping -s 56 -c 5 $POKEHOST | wc -l` -gt 6 ]; then
		RETVAL=2;
	fi

	return "$RETVAL";
}

#
#	poke_class
#
#		Argument: 1 -- share tree.
#			  2 -- NIS option.
#			  3 -- ACTION.
#			  4 -- CLASS.
#
#		return:   0 -- continue
#			  1 -- abort.
#
poke_class()
{
	POKESHARE="$1";
	POKENIS="$2";
	POKEACT="$3";
	POKECLASS="$4";

	if [ "$POKENIS" = "yes" ]; then
	    ypcat bootparams | grep "share=${POKESHARE}" 2>&1 > /dev/null
	    if [ $? -eq 0 ]; then
		echo "\nWARNING: Class $POKECLASS is still serving clients."
		if [ "$POKEACT" = "delete" ]; then
			echo "continue to delete $POKECLASS(Y/N)\07?\c"
		else
			echo "continue to update $POKECLASS(Y/N)\07?\c"
		fi;
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			return 1;
		fi;
		echo "\nChecking clients status:\n"
		ypcat -k bootparams | grep "share=${POKESHARE}" | while read a_line; do
			set $a_line;
			echo "        $1 ... \c"
			if [ `ping -s 56 -c 5 $1 | wc -l` -gt 6 ]; then
				echo "UP";
			else
				echo "DOWN";
			fi
		done

		echo "\nWARNING: clients will be out of sync. Clients which are UP may crash.\n"
		return 0;
	    fi;
	fi;

	if [ ! -f /etc/bootparams ]; then
		if [ "$POKEACT" = "delete" ]; then
			return 1;
		else
			return 0;
		fi;
	fi

	grep "share=${POKESHARE}" /etc/bootparams 2>&1 > /dev/null
	if [ $? -ne 0 ]; then
		return 0;
	fi

	echo "\nWARNING: Class $POKECLASS is still serving clients."
	if [ "$POKEACT" = "delete" ]; then
		echo "continue to delete $POKECLASS(Y/N)\07?\c"
	else
		echo "continue to update $POKECLASS(Y/N)\07?\c"
	fi;
	read CONF
	if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
		return 1;
	fi;
	echo "\nChecking clients status:\n"
	grep "share=${POKESHARE}" /etc/bootparams | while read a_line; do
		set $a_line;
		echo "    client $1 ... \c"
		if [ `ping -s 56 -c 5 $1 | wc -l` -gt 6 ]; then
			echo "UP";
		else
			echo "DOWN";
		fi
	done

	echo "\nWARNING: clients will be out of sync. Clients which are UP may crash.\n"
	return 0;

}
ACTION="create"

while [ "$1" != "" ]; do
	case "$1" in
	"-c")
		shift
		CLASS=$1
		;;
	"-h")
		shift
		HOST=$1;
		;;
	"-r")
		CMDMODE="yes";
		MINUSR="-R";
		break;
		;;
	"share")
		TARGET="share";
		;;
	"client")
		TARGET="client";
		;;
	"-d")
		ACTION="delete"
		;;
	*)
		echo "$CMDNAME: [-h host] -c class [-d] share|client"
		echo "	-h host:	create client tree for 'host', not needed for 'share'"
		echo "	-c class:	use class 'class'"
		echo "	-d:		delete the host or class"
		echo "	share:		install prototype/share tree"
		echo "	client:		install client tree"
		exit 0
		;;
	esac
	shift;
done

#
# If class not supplied, it is assumed to be auto-installation.
# The bizarre echo strings should not be modified, it is used in cl_init
#
if [ "$CMDMODE" = "yes" ]; then
	echo "$SENDCLASS"
	read CLASS

	if [ ! -f $DISKLESS_DIR/$CLASS.dat ]; then
		echo "missing class file"
		exit 0;
	fi;

	echo "$SENDHOST"
	read HOST

	TARGET="client"

	if [ "$HOST" = "" ]; then
		echo "Null host name\n"
		exit 0;
	fi;

	echo "$NOWECHO"
else
	if [ "$CLASS" = "" ]; then
		echo "Class not supplied !"
		exit 1;
	fi;
	if [ "$TARGET" = "" ]; then
		echo "Missing target !\n"
		exit 1;
	fi;
	if [ "$TARGET" != "share" -a "$HOST" = "" ]; then
		echo "Missing host name\n"
		exit 1;
	fi;
fi

. $DISKLESS_DIR/$CLASS.dat

if [ "$HOST" = "$CLASS" ]; then
	echo HOSTname can not be same as CLASSname
	exit 1;
fi

if [ "$DLMAJOR" = "" ]; then
	DLMAJOR=18
fi

#
# Validate client name.
#	possible_host_match: fully qualified name for potential match
#	host_line: Trying to remember the line from /etc/hosts
#
if [ "$HOST" != "" ]; then
	SYSID="$HOST"
	if [ "$NIS" = "yes" ]; then
		ypmatch $HOST hosts > /dev/null 2>&1
		if [ $? = "0" ]; then
			match="`ypmatch $HOST hosts`";
		fi
	else
		match=`grep $HOST /etc/hosts | grep -v \#`
		if [ "$match" != "" ]; then
			set $match
			while [ $# -gt 0 ]; do
				if [ "`expr $1 : '[1-9][0-9]\{0,2\}\.[0-9]\{1,3\}*'`" != "0" ]; then
					host_line=$*;
					possible_host_match=$2;
				elif [ "$1" = "$HOST" ]; then
					HOST=$possible_host_match;
					break;
				fi;
				shift;
			done;
		else
			echo Host name $HOST not found
			exit 1;
		fi;
	fi;

	if [ "$match" = "" ]; then
		echo "Invalid host name - $HOST"
		exit 1
	fi

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
			if [ "`expr $1 : '[1-9][0-9]\{0,2\}\.[0-9]\{1,3\}*'`" != "0" ]; then
				break;
			fi
		fi
	done;
fi;

SHKEY="$DISKLESS_DIR/$CLASS.bpkey"
BPKEY="$CLROOT/bpkey"

#
#

if [ "$ACTION" = "delete" ]; then
	case "$TARGET" in
	"share")
		poke_host "$CLASS" "$NIS" "$TARGET";
		HOSTSTAT=$?;
		case $HOSTSTAT in
		0)
			echo "Diskless class $CLASS does not exists"
			echo "Action aborted\n";
			exit 1;
			;;
		1)
			poke_class "${SHAREHOST}:${SHARE}" "$NIS" "$ACTION" "$CLASS";
			HOSTSTAT=$?;
			if [ "$HOSTSTAT" -eq 1 ]; then
				echo "Action aborted\n";
				exit 1;
			fi;
			;;
		*)
			echo "clinst: INTERNAL ERROR $HOSTSTAT"
			exit 1
			;;
		esac

		if [ ! -d $SHARE ]; then
			echo "Share tree not exist"
			exit 1;
		fi;
		echo "About to remove shared tree at $SHARE......"
		echo "Enter confirmation (y/Y)\07 :\c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "Action aborted\n";
			exit 1;
		fi;
		if [ -f $SHKEY -a "$NIS" = "yes" ]; then
			$CLIENT_NIS_DIR/yp_bootparam -b -d -h $CLASS -k $SHKEY;
			rm $SHKEY;
		elif [ "$NIS" != "yes" -a -f /etc/bootparams ]; then
			sed -e /\^$CLASS\ /d /etc/bootparams > /etc/bootparam
			mv /etc/bootparam /etc/bootparams
		fi;
		unexport_dir $SHARE $CLASS
		rm $BOOTP_DIR/$CLASS
		;;
	"client")
		poke_host "$HOST" "$NIS" "$TARGET";
		HOSTSTAT=$?;
		case $HOSTSTAT in
		0)
			echo "$HOST is not diskless client for CLASS $CLASS"
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

		if [ ! -d $CLROOT ]; then
			echo "Client tree not exist"
			exit 1;
		fi;
		echo "Remove client tree at $CLROOT (shared tree = $SHARE)"
		echo "Enter confirmation (y/Y)\07 :\c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "Action aborted\n";
			exit 1;
		fi;
		if [ -f $BPKEY -a "$NIS" = "yes" ]; then
			$CLIENT_NIS_DIR/yp_bootparam -d -h $HOST -k $BPKEY;
			rm $BPKEY
		elif [ "$NIS" != "yes" -a -f /etc/bootparams ]; then
			sed -e /\^$HOST/d /etc/bootparams > /etc/bootparam
			mv /etc/bootparam /etc/bootparams
		fi;
		unexport_dir $CLROOT $HOST $SWAP
		if [ "$NIS" = "yes" ]; then
			echo "Do you want host $HOST removed from NIS(y/n)?\07 \c"
			read CONF
			if [ "$CONF" = "y" ]; then
				$CLIENT_NIS_DIR/yp_host -d -h $HOST;
			fi;
		fi
		;;
	*)
		echo "Unknown  target specified !!"
		;;
	esac
	exit 0;
fi

#
# Install.
#
case "$TARGET" in
"share")
	poke_host "$CLASS" "$NIS" "$TARGET";
	HOSTSTAT=$?;
	case $HOSTSTAT in
	1)
		poke_class "${SHAREHOST}:${SHARE}" "$NIS" "$ACTION" "$CLASS";
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

	if [ "$USESRVROOT" = "yes" ]; then
		echo "Using $SHAREHOST:/ as share-tree $SHARE ($SHARE is dir or symlink to /)"
		if [ ! -d $SHARE ]; then
			ln -s / $SHARE;
			if [ ! -l $SHARE ]
			then echo $SHARE not a directory or symlink to /; exit 1;
			fi
		else
			chk_sroot $SHARE;
			if [ $? -ne 1 ]; then
				echo "Remove share tree $CLASS and restart.\n"
				echo "Something is wrong with $SHARE used internally during setup"
				exit 1;
			fi
		fi
		echo "Is server installed as proto-tree(y/n)? \c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "When inst invoked, please do following:\n"
			echo "    1. goto manual mode\n"
			echo "    2. set instmode proto\n"
			echo "    3. execute from cmd.\n"
			echo "    4. execute list cmd.\n"
			echo "Ready? \c"
			read CONF
			if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
				echo "Action aborted\n";
				exit 1;
			fi
			$INST -mGFXBOARD=$GFXBOARD -mCPUARCH=$CPUARCH \
				-mCPUBOARD=$CPUBOARD -m$MACH
		fi
	else
		if [ ! -d $SHARE ]; then
			mkdir -p $SHARE;
		fi
		$INST -r$SHARE -mGFXBOARD=$GFXBOARD \
			-mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD -m$MACH
		rm -f $SHARE/dev/root
		rm -f $SHARE/dev/swap
		/etc/mknod $SHARE/dev/root b $DLMAJOR 0
		/etc/mknod $SHARE/dev/swap b $DLMAJOR 0
		rm -f $SHARE/usr/sysgen/boot/efs.orig
		DL_ROOT=$SHARE;export DL_ROOT
		sed -e /TOOLROOT/d $SHARE/usr/sysgen/system.dl > $SHARE/usr/sysgen/system.tmp
		mv -f $SHARE/usr/sysgen/system.tmp $SHARE/usr/sysgen/system.dl
		echo "CC: TOOLROOT=$DL_ROOT/usr/sysgen/root $DL_ROOT/usr/sysgen/root/usr/bin/cc" >> $SHARE/usr/sysgen/system.dl
		echo "LD: TOOLROOT=$DL_ROOT/usr/sysgen/root $DL_ROOT/usr/sysgen/root/usr/bin/ld" >> $SHARE/usr/sysgen/system.dl
		if [ -l $SHARE/usr/sysgen/root/usr/lib/cpp ]; then
			rm -f $SHARE/usr/sysgen/root/usr/lib/cpp
			ln -s ../../lib/cpp $SHARE/usr/sysgen/root/usr/lib/cpp
		fi
		$SHARE/usr/sbin/lboot -u $SHARE/unix -r $SHARE \
					-s $SHARE/usr/sysgen/system.dl
		touch -m 0101000070 $SHARE/unix
	fi

	if [ ! -d $BOOTP_DIR ]; then
		mkdir -p $BOOTP_DIR;
	fi
	if [ -l $BOOTP_DIR/$CLASS ]; then
		rm -f $BOOTP_DIR/$CLASS
	fi
	ln -sf $SHARE $BOOTP_DIR/$CLASS
	if [ "$NIS" = "yes" ]; then
		ypcat hosts > $SHARE/etc/hosts;
	else
		if [ "$USESRVROOT" != "yes" ]; then
			cp /etc/hosts $SHARE/etc;
		fi;
	fi;
	export_dir $SHARE ro $CLASS
	if [ "$NIS" = "yes" ]; then
		if [ -f $SHKEY ]; then
			$CLIENT_NIS_DIR/yp_bootparam -d -h $CLASS -b -k $SHKEY;
			rm $SHKEY
		fi
		$CLIENT_NIS_DIR/yp_bootparam -a -h $CLASS -k $SHKEY -b \
			root=$SHAREHOST:$SHARE \
			share=$SHAREHOST: swap=$SHAREHOST:
	else
		sed -e /\^$CLASS\ /d /etc/bootparams > /etc/bootparam
		mv /etc/bootparam /etc/bootparams
		echo "$CLASS root=$SHAREHOST:$SHARE share=$SHAREHOST: swap=$SHAREHOST:" >> /etc/bootparams
	fi;

	if [ "$USESRVROOT" != "yes" ]; then
		if [ ! -l $SHARE/etc/init ]; then
			cp $SHARE/etc/init $SHARE/etc/init.bk;
			rm -rf $SHARE/etc/init
			ln -s cl_init $SHARE/etc/init;
		fi
		rm $SHARE/etc/telinit
		ln -s init.bk $SHARE/etc/telinit
	fi
	;;

"client")
	if [ "$HOST" = "" ]; then
		echo "Host name unspecified\n"
		exit 1;
	fi
	poke_host "$HOST" "$NIS" "$TARGET";
	HOSTSTAT=$?;
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
		echo "WARNING: Client tree($RP) is longer than 37 chars."
	fi;
	if [ "$CMDMODE" = "no" ]; then
		echo "Enter confirmation (y/Y)\07 :\c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "Action aborted\n";
			exit 1;
		fi
	fi
	if [ ! -d $CLROOT ]; then
		mkdir -p $CLROOT;
	fi

	if [ -l /share ]; then
		echo "Warning: Possible multiple diskless client installaton!"
		echo "If there is no other client installation, just type y on prompt."
		echo "Continue?(y/Y)\07 :\c"
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			echo "Action aborted\n";
			exit 1;
		fi
	fi

	export_dir $CLROOT rw $HOST $HOST
	echo "$HOSTNAME:$CLROOT / nfs rw 0 0" > $DISKLESS_DIR/fstab.$HOST
	echo "$SHAREHOST:$SHARE /share nfs ro 0 0" >> $DISKLESS_DIR/fstab.$HOST
	echo "root=$HOSTNAME:$CLROOT" > $DISKLESS_DIR/bootparam.$HOST
	echo "share=$SHAREHOST:$SHARE" >> $DISKLESS_DIR/bootparam.$HOST

	if [ "$SWAP" = "" -o "$SWAPSIZE" = "0" -o "$SWAPSIZE" = "0k" -o \
		"$SWAPSIZE" = "0b" -o "$SWAPSIZE" = "0m" ]; then
		rm -rf $CLROOT/swap;
		rm -f $SWAP/_swap;
		echo "swap=$HOSTNAME:" >> $DISKLESS_DIR/bootparam.$HOST;
	else
		if [ ! -d $CLROOT/swap ]; then
			mkdir -p $CLROOT/swap;
		fi
		echo "$HOSTNAME:$SWAP /swap nfs rw 0 0" >> $DISKLESS_DIR/fstab.$HOST
		echo "swap=$HOSTNAME:$SWAP" >> $DISKLESS_DIR/bootparam.$HOST;
	
		if [ ! -d $SWAP ]; then
			mkdir -p $SWAP;
		fi;
		if [ ! -f $MKFILE ]; then
			echo "$MKFILE does not exit\n" > /dev/console;
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
		export_dir $SWAP rw $HOST $HOST;
	fi;

	if [ "$NIS" = "yes" ]; then
		if [ -f $BPKEY ]; then
			$CLIENT_NIS_DIR/yp_bootparam -d -h $HOST -k $BPKEY;
		fi
		$CLIENT_NIS_DIR/yp_bootparam -a -h $HOST -k $BPKEY \
			-f $DISKLESS_DIR/bootparam.$HOST
		rm $DISKLESS_DIR/bootparam.$HOST
	else
		sed -e /\^$HOST/d /etc/bootparams > /etc/bootparam
		mv /etc/bootparam /etc/bootparams
		set `cat $DISKLESS_DIR/bootparam.$HOST`
		echo "$HOST $*" >> /etc/bootparams
	fi;

	$INST $MINUSR -r$CLROOT -f$SHARE/usr/lib/inst \
		-mGFXBOARD=$GFXBOARD -mCPUARCH=$CPUARCH -mCPUBOARD=$CPUBOARD -m$MACH
	if [ ! -d $CLROOT/share ]; then
		mkdir -p $CLROOT/share;
	fi
	if [ -f $CLROOT/usr/sbin/inst -a ! -l $CLROOT/usr/sbin/inst ]; then
		mv $CLROOT/usr/sbin/inst $CLROOT/usr/sbin/inst.O
		cp ./inst4.0.5 $CLROOT/usr/sbin/inst4.0.5H
		ln -s inst $CLROOT/usr/sbin/inst4.0.5H
	fi

	cp $DISKLESS_DIR/fstab.$HOST $CLROOT/etc/mtab;
	if [ ! -f $CLROOT/etc/fstab ]; then
		mv $DISKLESS_DIR/fstab.$HOST $CLROOT/etc/fstab;
	else
		sed -e /\\/\ nfs/d -e /\\/share/d -e /\\/swap/d \
			$CLROOT/etc/fstab > $CLROOT/etc/fstab.tmp;
		mv $DISKLESS_DIR/fstab.$HOST $CLROOT/etc/fstab;
		cat $CLROOT/etc/fstab.tmp >> $CLROOT/etc/fstab;
		rm $CLROOT/etc/fstab.tmp
	fi
	if [ ! -d $CLROOT/tmp ]; then
		mkdir -m 777 $CLROOT/tmp;
	fi
	echo on > $CLROOT/etc/config/nfs
	echo on > $CLROOT/etc/config/network
	echo on > $CLROOT/etc/config/verbose
	echo on > $CLROOT/etc/config/timed
	if [ "$NIS" = "yes" ]; then
		ypcat hosts > $CLROOT/etc/hosts;
	else
		cp /etc/hosts $CLROOT/etc
	fi
	rm -f $CLROOT/dev/root
	rm -f $CLROOT/dev/swap
	/etc/mknod $CLROOT/dev/root b $DLMAJOR 0
	/etc/mknod $CLROOT/dev/swap b $DLMAJOR 0

	if [ "$USESRVROOT" != "yes" ]; then
		rm -f $CLROOT/etc/init
		rm -f $CLROOT/etc/cl_init
		ln -s /share/etc/init.bk $CLROOT/etc/init
		rm -f $CLROOT/etc/telinit
		ln -s /etc/init $CLROOT/etc/telinit
		rm -f $CLROOT/unix
		ln $SHARE/unix $CLROOT/unix > /dev/null 2>&1
		if [ "$?" != "0" -o "$SHAREHOST" != "$HOSTNAME" ]; then
			rm -f $CLROOT/unix
			if [ "$SHAREHOST" != "$HOSTNAME" ]; then
				mkdir  $DISKLESS_DIR/mount.$HOST
				mount $SHAREHOST:$SHARE $DISKLESS_DIR/mount.$HOST
				cp $DISKLESS_DIR/mount.$HOST/unix $CLROOT/unix
				umount $DISKLESS_DIR/mount.$HOST
				rm -r $DISKLESS_DIR/mount.$HOST
			else
				cp $SHARE/unix $CLROOT/unix
			fi
		fi
	else
		$SHARE/usr/sbin/lboot -u $CLROOT/unix -r $SHARE \
					-s $SHARE/usr/sysgen/system.dl
	fi
	touch -m 0101000070 $CLROOT/unix

	if [ "$NISDOMAIN" != "" ]; then
		echo $NISDOMAIN > ${CLROOT}${CLIENT_NIS_DIR}/ypdomain 
	elif [ -x /usr/bin/domainname ]; then
		if [ ! -f ${CLROOT}${CLIENT_NIS_DIR}/ypdomain ]; then
			/usr/bin/domainname > ${CLROOT}${CLIENT_NIS_DIR}/ypdomain
		fi
	fi;

	rm -f $CLROOT/etc/rc2.d/S48savecore
	if [ "$LOCALDISK" != "yes" ]; then
		rm -f $CLROOT/usr/etc/nfsd
	fi
	echo "$SYSID" > $CLROOT/etc/sys_id

	BDNAME="$DISKLESS_DIR"/`basename "$CLROOT"`
	if [ -l "$BDNAME" ]; then
		rm -f "$BDNAME"
	else
		rm -rf "$BDNAME"
	fi
	mkdir "$BDNAME"
	ln -sf "$CLROOT"/unix "$BDNAME"

	rm /share >/dev/null 2>&1
	;;
*)
	echo "No target specified !!"
	exit 0
	;;
esac
