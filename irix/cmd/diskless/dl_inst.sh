#! /bin/sh
#ident  "$Revision: 1.72 $"
#
#
# Following variables should be read from file /var/boot/$RELEASE.dat
#
#DISKLESS="/usr/tmp"
#CLROOT="$DISKLESS/$HOST"
#SHAREHOST="$HOSTNAME"
#SHARE="$DISKLESS/$CLASS"
#SWAP="$DISKLESS/swap/$HOST"
#SWAPSIZE="10m"
#NIS="yes"
#DLMAJOR=18	A long time ago NFS used its own major number
#
# Following variables should be read from file /var/boot/$CLASS.dat
#
#GFXBOARD="ECLIPSE"
#CPUBOARD="IP6"
#CPUARCH="R3000"
#VIDEO=""
#MODE=""
#
# The following variables are "server-relative"
#
DIR="/var/boot"
MKFILE="/usr/sbin/mkfile"
#
# The following variables are "client-relative"
#
BOOT=$CLROOT/var/sysgen/boot
SHELL="/sbin/sh"
NIS_DIR="/var/yp"
NIS_EXEC_DIR="/usr/sbin"
SGI_CC=-cckr; export SGI_CC
NFS_INSTALL="no"
PATH="/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/etc:/usr/etc:/usr/bin/X11"
export PATH

#
# Pick installation tool
#
# No arguements
#
pick_inst()
{
	echo "Which installation tool would you like to use:"
	echo "	1. inst"
	echo "	2. Software Manager"
	echo ""
	echo -n "Your choice (1 or 2): "
	read choice;
	case $choice in
	2 )
		INST="/usr/sbin/swmgr"
		INSTALL_TOOL=${INST}
		INST_OPT="-Vbackground:off"
		;;
	1 )
		INST="/usr/sbin/inst"
		INSTALL_TOOL=${INST}
		INST_OPT=""
		;;
	*)
		echo "Unknown install mechanism"
		exit 1
		;;
	esac
}

#
#

CMDNAME=$0
TARGET=""

#
# Run exportfs on server
#
#	No arguments
#
run_exportfs()
{
	if [ -x /usr/etc/exportfs ]; then
		/usr/etc/exportfs $1
		case $? in
		0)
			;;
		*)
			echo "There is probably a mistake in your /etc/exports file"
			echo "There is probably a mistake in your /etc/exports file" >> /dev/console
			echo "ATTENTION: fix /etc/exports and rerun exportfs !"
			echo "ATTENTION: fix /etc/exports and rerun exportfs !" >> /dev/console
			exit 1
			;;
		esac
	else
		echo "ATTENTION: /usr/etc/exportfs does not exist !"
		echo "ATTENTION: /usr/etc/exportfs does not exist !" >> /dev/console
		exit 1
	fi
}

#
# Create /etc/exports if it does not exist.
#
#	No arguments
#
create_exports()
{
	if [ ! -f /etc/exports ]; then
		> /etc/exports
		chmod 644 /etc/exports
	fi
}

#
# Update client exports on server
#
# Any swap partitions must be exported with the wsync flag.  This will force
# accesses to NFS swap to be synchronous.  Without this, we have a good chance
# on a heavily loaded client system to have random programs core dump when a
# server recovers after a crash.
#
#	arguments: 1 -- export directory.
#		   2 -- class/client KEY.
#		   3 -- swap/noswap argument
#
export_client_dir()
{
	if [ "$NFS_INSTALL" = "yes" ]; then
		echo "\nPlease export this entry on the NFS server: "
		if [ "$3" = "swap" ]; then
			echo "	$1 -rw=$2,wsync,access=$2,root=$2    #host=$2"
		else
			echo "	$1 -rw=$2,access=$2,root=$2    #host=$2"
		fi
		return;
	fi

	create_exports

	# Add export line to /etc/exports
	grep $1 /etc/exports >/dev/null 2>&1
	if [ $? -eq 1 ]; then
		if [ "$3" = "swap" ]; then
			echo "$1 -rw=$2,wsync,access=$2,root=$2	#host=$2" \
				>> /etc/exports
		else
			echo "$1 -rw=$2,access=$2,root=$2	#host=$2" \
				>> /etc/exports
		fi
	fi

	run_exportfs $1
}


#
# Update shareable exports on server
#
#	arguments: 1 -- export directory.
#		   2 -- export privileges
#		   3 -- class/client KEY.
#
export_share_dir()
{
	if [ $2 != "rw" -a $2 != "ro" ]; then
		echo "\nIllegal flag to export_share_dir"
		exit
	fi

	if [ "$NFS_INSTALL" = "yes" ]; then
		echo "\nPlease export this entry on the NFS server: "
		echo "	$1 -$2 ,anon=root	#class=$3"
		return;
	fi

	create_exports

	# Add export line to /etc/exports
	grep $1 /etc/exports >/dev/null 2>&1
	if [ $? -eq 1 ]; then
		echo "$1 -$2 $4	#class=$3" >> /etc/exports
	fi

	run_exportfs $1
}


#
#	Unexports directories on server.
#
#		Arguments: 1 -- type
#			   2 -- directory
#			   3 -- key
#
unexport_dir()
{
	if [ "$1" != "class" -a "$1" != "host" ]; then
		echo "Illegal type to unexport_dir"
		exit 1;
	fi
	if [ "X$2" = "X" ]; then
		echo "No directory argument to unexport_dir";
		exit 1;
	fi
	if [ ! -d "$2" ]; then
		echo "Illegal directory argument to unexport_dir";
		exit 1;
	fi
	if [ "X$3" = "X" ]; then
		echo "No key argument to unexport_dir";
		exit 1;
	fi
	if [ "X$NFS_INSTALL" = "Xyes" ]; then
		echo "Please unexport the NFS server entry for: \c"
		echo $3
		rm -r $2
		return
	fi

	if [ ! -f /etc/exports ]; then
		echo "ATTENTION: /etc/exports does not exist !" >> /dev/console
		echo "ATTENTION: /etc/exports does not exist !"
		exit 1
	fi

	if [ -x /usr/etc/exportfs ]; then
		/usr/etc/exportfs -u "$2" > /dev/null 2>&1
	else
		echo "ATTENTION: /usr/etc/exportfs does not exist !"
		echo "ATTENTION: /usr/etc/exportfs does not exist !" >> /dev/console
		exit 1
	fi

	if [ "$1" = "class" ]; then
		grep -v "$2.*\#class=$3" /etc/exports > /etc/export.$$
	else
		grep -v "$2.*\#host=$3" /etc/exports > /etc/export.$$
	fi;

	rm -r $2
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
	if [ "$NFS_INSTALL" = "yes" ]; then
		return 1;
	fi
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
	if [ "$NFS_INSTALL" = "yes" ]; then
		echo "Warning: $POKECLASS may be active.  Not checking server"
		if [ "$POKEACT" = "delete" ]; then
			echo "continue to delete $POKECLASS(Y/N)\07?\c"
		else
			echo "continue to update $POKECLASS(Y/N)\07?\c"
		fi;
		read CONF
		if [ "$CONF" != "y" -a "$CONF" != "Y" ]; then
			return 1;
		fi
		return 0;
	fi

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
	echo "\nChecking client's status:\n"
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
