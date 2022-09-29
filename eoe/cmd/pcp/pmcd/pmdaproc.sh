# Common sh(1) procedures to be used in the Performance Co-Pilot
# PMDA Install and Remove scripts
#
# $Id: pmdaproc.sh,v 2.28 1999/11/11 06:08:01 nathans Exp $
#
# Copyright 1995, Silicon Graphics, Inc.
# ALL RIGHTS RESERVED
# 
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
# 
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
# 
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.

# Some degree of paranoia here ...
PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/pcp/bin
export PATH

# some useful common variables for Install/Remove scripts
#
# writeable root of PMNS
NAMESPACE=${PMNS_DEFAULT-/var/pcp/pmns/root}
PMNSROOT=`basename $NAMESPACE`

# put you PMNS files here
PMNSDIR=`dirname $NAMESPACE`

# install your DSO PMDAs here
DSOPMDADIR=/var/pcp/lib

# install your daemon PMDAs here
PMDADIR=/var/pcp/pmdas

# Install control variables
#	Can install as DSO?
dso_opt=false
#	Can install as daemon?
daemon_opt=true
#	If daemon, pipe?
pipe_opt=true
#	If daemon, socket?  and default for Internet sockets?
socket_opt=false
socket_inet_def=''
#	IPC Protocol for daemon (binary or text)
ipc_prot=binary
#	Delay after install before checking (sec)
check_delay=3
#	Additional command line args to go in /etc/pmcd.conf
args=""
#	Source for the pmns
pmns_source=pmns
#	Source for the helptext
help_source=help
#	Assume libpcp_pmda.so.1
pmda_interface=1


# Other variables and constants
#
prog=`basename $0`
tmp=/var/tmp/$$
do_pmda=true
do_check=true
__here=`pwd`
__pmcd_is_dead=false

trap "rm -f $tmp $tmp.*; exit" 0 1 2 3 15

# Parse command line args
#
while [ $# -gt 0 ]
do
    case $1
    in
	-N)	# name space only
	    do_pmda=false
	    ;;

	-n)	# alternate name space
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -n requires a name space file option"
		exit 1
	    fi
	    NAMESPACE=$2
	    PMNSROOT=`basename $NAMESPACE`
	    PMNSDIR=`dirname $NAMESPACE`
	    shift
	    ;;

	-Q)	# skip check for metrics going away
	    do_check=false
	    ;;

	-R)	# $ROOT
	    if [ "$prog" = "Remove" ]
	    then
		echo "Usage: $prog [-N] [-n namespace] [-Q]"
		exit 1
	    fi
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -R requires a directory option"
		exit 1
	    fi
	    root=$2
	    shift
	    ;;

	*)
	    if [ "$prog" = "Install" ]
	    then
		echo "Usage: $prog [-N] [-n namespace] [-Q] [-R rootdir]"
	    else
		echo "Usage: $prog [-N] [-n namespace] [-Q]"
	    fi
	    exit 1
	    ;;
    esac
    shift
done

# wait for pmcd to be alive again
# 	Usage: __wait_for_pmcd [can_wait]
#
__wait_for_pmcd()
{
    # 60 seconds default seems like a reasonble max time to get going
    [ -z "$_can_wait" ] && __can_wait=${1-60}
    __i=2
    __dead=true
    sleep 2
    while [ $__i -lt $__can_wait ]
    do
	__clients=`pmprobe -n /var/pcp/pmns/root_pmcd pmcd.numclients 2>/dev/null | sed -e 's/.* //'`
	if [ ! -z "$__clients" -a "$__clients" -gt 0 ]
	then
	    __dead=false
	    break
	fi
	sleep 2
	__i=`expr $__i + 2`
    done
    if $__dead
    then
	echo "Arrgghhh ... PMCD failed to start after $__can_wait seconds"
	if [ -f ${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log ]
	then
	    echo "Here is the PMCD logfile (${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log):"
	    ls -l ${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log
	    cat ${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log
	else
	    echo "No trace of the PMCD logfile (${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log)!"
	fi
	__pmcd_is_dead=true
    fi
}

# try and put pmcd back the way it was
#
__restore_pmcd()
{
    if [ -f $tmp.pmcd.conf.save ]
    then
	__pmcd_is_dead=false
	echo
	echo "Save current PMCD control file in /etc/pmcd.conf.prev ..."
	rm -f /etc/pmcd.conf.prev
	mv /etc/pmcd.conf /etc/pmcd.conf.prev
	echo "Restoring previous PMCD control file, and trying to restart PMCD ..."
	cp $tmp.pmcd.conf.save /etc/pmcd.conf
	chown root /etc/pmcd.conf
	chmod 444 /etc/pmcd.conf
	rm -f $tmp.pmcd.conf.save
	/etc/init.d/pcp start
	__wait_for_pmcd
    fi
    if $__pmcd_is_dead
    then
	echo
	echo "Sorry, failed to restart PMCD."
    fi
}

# __pmda_cull name domain
#
__pmda_cull()
{
    # context and integrity checks
    #
    [ ! -f /etc/pmcd.conf ] && return
    if [ ! -w /etc/pmcd.conf ]
    then
	echo "pmdaproc.sh: \"/etc/pmcd.conf\" is not writeable"
	exit 1
    fi
    if [ $# -ne 2 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_cull() called with $# (instead of 2) arguments"
	exit 1
    fi

    # remove matching entry from /etc/pmcd.conf if present
    #
    nawk </etc/pmcd.conf >/tmp/$$.pmcd.conf '
BEGIN					{ status = 0 }
$1 == "'"$1"'" && $2 == "'"$2"'" 	{ status = 1; next }
					{ print }
END					{ exit status }'
    if [ $? -eq 0 ]
    then
	# no match
	:
    else
	
	# log change to /var/adm/pcplog/NOTICES
	#
	pmpost "PMDA cull: from /etc/pmcd.conf: $1 $2"

	# save pmcd.conf in case we encounter a problem, and then
	# install updated /etc/pmcd.conf
	#
	cp /etc/pmcd.conf $tmp.pmcd.conf.save
	cp /tmp/$$.pmcd.conf /etc/pmcd.conf
	chown root /etc/pmcd.conf
	chmod 444 /etc/pmcd.conf

	# signal pmcd if it is running
	#
	if pminfo -v pmcd.version >/dev/null 2>&1
	then
	    killall -HUP pmcd
	    __wait_for_pmcd
	    $__pmcd_is_dead && __restore_pmcd
	    # give PMDA a chance to cleanup, especially if a new one about
	    # to be installed
	    sleep 3
	fi
    fi
    rm -f /tmp/$$.pmcd.conf

    # kill off any matching PMDA that is still running
    #
    for __sig in INT TERM KILL
    do
	__pids=`ps -ef \
		| sed -n \
		    -e 's/$/ /' \
		    -e '/pmda'$1'.*-d '$2' /{
s/  */ /g
s/^ //
p
}' \
		| cut -d" " -f2`

	if [ ! -z "$__pids" ]
	then
	    /bin/kill -$__sig $__pids >/dev/null 2>&1
	    sleep 3
	else
	    break
	fi
    done

}

# __pmda_add "entry for /etc/pmcd.conf"
#
__pmda_add()
{
    # context and integrity checks
    #
    if [ ! -w /etc/pmcd.conf ]
    then
	echo "pmdaproc.sh: \"/etc/pmcd.conf\" is not writeable"
	exit 1
    fi
    if [ $# -ne 1 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_add() called with $# (instead of 1) arguments"
	exit 1
    fi

    # save pmcd.conf in case we encounter a problem
    #
    cp /etc/pmcd.conf $tmp.pmcd.conf.save

    myname=`echo $1 | nawk '{print $1}'`
    mydomain=`echo $1 | nawk '{print $2}'`
    # add entry to /etc/pmcd.conf
    #
    echo >/tmp/$$.pmcd.access
    nawk </etc/pmcd.conf '
NF==0					{ next }
/^[      ]*\[[   ]*access[       ]*\]/	{ state = 2 }
state == 2				{ print >"'/tmp/$$.pmcd.access'"; next }
$1=="'$myname'" && $2=="'$mydomain'"	{ next }
					{ print >"'/tmp/$$.pmcd.body'"; next }'
    ( cat /tmp/$$.pmcd.body \
      ; echo "$1" \
      ; cat /tmp/$$.pmcd.access \
    ) >/etc/pmcd.conf
    rm -f /tmp/$$.pmcd.access /tmp/$$.pmcd.body
    chown root /etc/pmcd.conf
    chmod 444 /etc/pmcd.conf

    # log change to /var/adm/pcplog/NOTICES
    #
    pmpost "PMDA add: to /etc/pmcd.conf: $1"

    # signal pmcd if it is running, else start it
    #
    if pminfo -v pmcd.version >/dev/null 2>&1
    then
	killall -HUP pmcd
	__wait_for_pmcd
	$__pmcd_is_dead && __restore_pmcd
    else
	log=${PCP_LOGDIR-/var/adm/pcplog}/pmcd.log
	rm -f $log
	/etc/init.d/pcp start
	__wait_for_pmcd
	$__pmcd_is_dead && __restore_pmcd
    fi
}

# expect -R root or $ROOT not set in environment
#
__check_root()
{
    if [ "X$root" != X ]
    then
	ROOT="$root"
	export ROOT
    else
	if [ "X$ROOT" != X -a "X$ROOT" != X/ ]
	then
	    echo "Install: \$ROOT was set to \"$ROOT\""
	    echo "          Use -R rootdir to install somewhere other than /"
	    exit 1
	fi
    fi
}

# should be able to extract default domain from domain.h
#
__check_domain()
{
    if [ -f domain.h ]
    then
	# $domain is for backwards compatibility, modern PMDAs
	# have something like
	#	#define FOO 123
	#
	domain=''
	eval `nawk <domain.h '
/^#define/ && $3 ~ /^[0-9][0-9]*$/	{ print $2 "=" $3
					  if (seen == 0) {
					    print "domain=" $3
					    print "SYMDOM=" $2
					    seen = 1
					  }
					}'`
	if [ "X$domain" = X ]
	then
	    echo "Install: cannot determine the Performance Metrics Domain from ./domain.h"
	    exit 1
	fi
    else
	echo "Install: cannot find ./domain.h to determine the Performance Metrics Domain"
	exit 1
    fi
}

# handle optional configuration files that maybe already given in an
# /etc/pmcd.conf line or user-supplied or some default or sample
# file
#
# before calling _choose_configfile, optionally define the following
# variables
#
# Name		Default			Use
#
# configdir	/var/pcp/config/$iam	directory for config ... assumed
#					name is $iam.conf in this directory
#
# configfile	""			set if have a preferred choice,
#					e.g. from /etc/pmcd.conf
#					this will be set on return if we've
#					found an acceptable config file
#
# default_configfile
#		""			if set, this is the default which
#					will be offered
#
# Note: 
#  If the choice is aborted then $configfile will be set to empty.
#  Therefore, there should be a test for an empty $configfile after
#  the call to this function.
#
_choose_configfile()
{
    configdir=${configdir-/var/pcp/config/$iam}

    if [ ! -d $configdir ]
    then
    	mkdir -p $configdir
    fi

    while true
    do
	echo "Possible configuration files to choose from:"
	# List viable alternatives
	__i=0          # menu item number
	__filelist=""  # list of configuration files
	__choice=""    # the choice of configuration file
	__choice1=""   # the menu item for the 1st possible choice
	__choice2=""   # the menu item for the 2nd possible choice
	__choice3=""   # the menu item for the 3rd possible choice

        if [ ! -z "$configfile" ]
	then
	    if [ -f $configfile ]
	    then
		__i=`expr $__i + 1`
		__choice1=$__i
		__filelist="$__filelist $configfile"
		echo "[$__i] $configfile"
	    fi
	fi

        if [ -f $configdir/$iam.conf ]
	then
	    if echo $__filelist | grep "$configdir/$iam.conf" > /dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice2=$__i
		__filelist="$__filelist $configdir/$iam.conf"
		echo "[$__i] $configdir/$iam.conf"
	    fi
	fi

	if [ -f $default_configfile ]
	then
	    if echo $__filelist | grep "$default_configfile" > /dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice3=$__i
		__filelist="$__filelist $default_configfile"
		echo "[$__i] $default_configfile"
	    fi
	fi

	__i=`expr $__i + 1`
	__own_choice=$__i
	echo "[$__i] Specify your own configuration file."

	__i=`expr $__i + 1`
	__abort_choice=$__i
	echo "[$__i] None of the above (abandon configuration file selection)."

	echo "Which configuration file do you want to use ? [1] \c"
	read __reply

	# default
	if [ -z "$__reply" ] 
	then
	    __reply=1
	fi

	# Process the reply from the user
	if [ $__reply = $__own_choice ]
	then
	    echo "Enter the name of the existing configuration file: \c"
	    read __choice
	    if [ ! -f "$__choice" ]
	    then
		echo "Cannot open \"$__choice\"."
		echo ""
		echo "Please choose another configuration file."
		__choice=""
	    fi
	elif [ $__reply = $__abort_choice ]
	then
	    echo "Abandoning configuration file selection."
	    configfile=""
	    return 0
	elif [ "X$__reply" = "X$__choice1" -o "X$__reply" = "X$__choice2" -o "X$__reply" = "X$__choice3" ]
	then
	    # extract nth field as the file
	    __choice=`echo $__filelist | nawk -v n=$__reply '{ print $n }'` 
	else
	    echo "Illegal choice: $__reply"
	    echo ""
	    echo "Please choose number between: 1 and $__i"
	fi

	if [ ! -z "$__choice" ]
	then
	    echo
	    echo "Contents of the selected configuration file:"
	    echo "--------------- start $__choice ---------------"
	    cat $__choice
	    echo "--------------- end $__choice ---------------"
	    echo

	    echo "Use this configuration file? [y] \c"
	    read ans
	    if [ ! -z "$ans" -a "X$ans" != Xy -a "X$ans" != XY ]
	    then
		echo ""
		echo "Please choose another configuration file."
	    else
		break
	    fi
	fi
    done


    __dest=$configdir/$iam.conf
    if [ "$__choice" != "$__dest" ]
    then
	if [ -f $__dest ]
	then
	    echo "Removing old configuration file \"$__dest\""
	    rm -f $__dest
	    if [ -f $__dest ]
	    then
		echo "Error: cannot remove old configuration file \"$__dest\""
		exit 1
	    fi
	fi
	if cp $__choice $__dest
	then
	    :
	else
	    echo "Error: cannot install new configuration file \"$__dest\""
	    exit 1
	fi
	__choice=$__dest
    fi

    configfile=$__choice
}

# choose correct PMDA installation mode
#
# make sure we are installing in the correct style of configuration
#
__choose_mode()
{
    __def=m
    $do_pmda && __def=b
    echo \
'You will need to choose an appropriate configuration for installation of
the "'$iam'" Performance Metrics Domain Agent (PMDA).

  collector	collect performance statistics on this system
  monitor	allow this system to monitor local and/or remote systems
  both		collector and monitor configuration for this system
'
    while true
    do
	echo 'Please enter c(ollector) or m(onitor) or b(oth) ['$__def'] \c'
	read ans
	case "$ans"
	in
	    "")	break
		    ;;
	    c|collector|b|both)
		    do_pmda=true
		    break
		    ;;
	    m|monitor)
		    do_pmda=false
		    break
		    ;;
	    *)	echo "Sorry, that is not acceptable response ..."
		    ;;
	esac
    done
    echo
}

# choose an IPC method
#
__choose_ipc()
{
    _dir=${1-/usr/pcp/lib}
    ipc_type=''
    $pipe_opt && ipc_type=pipe
    $socket_opt && ipc_type=socket
    $pipe_opt && $socket_opt && ipc_type=''
    if [ -z "$ipc_type" ]
    then
	while true
	do
	    echo "PMCD should communicate with the $iam daemon via a pipe or a socket? [pipe] \c"
	    read ipc_type
	    if  [ "X$ipc_type" = Xpipe -o "X$ipc_type" = X ]
	    then
		ipc_type=pipe
		break
	    elif [ "X$ipc_type" = Xsocket ]
	    then
		break
	    else
		echo "Must choose one of \"pipe\" or \"socket\", please try again"
	    fi
	done
    fi

    if [ $ipc_type = pipe ]
    then
	type="pipe	$ipc_prot 		$_dir/$pmda_name"
    else
	while true
	do
	    echo "Use Internet or Unix domain sockets? [Internet] \c"
	    read ans
	    if [ "X$ans" = XInternet -o "X$ans" = X ]
	    then
		echo "Internet port number or service name? [$socket_inet_def] \c"
		read port
		[ "X$port" = X ] && port=$socket_inet_def
		case $port
		in
		    [0-9]*)
			    ;;
		    *)
			    if grep "^$port[ 	]*[0-9]*/tcp" /etc/services >/dev/null 2>&1
			    then
				:
			    else
				echo "Warning: there is no tcp service for \"$port\" in /etc/services!"
			    fi
			    ;;
		esac
		type="socket	inet $port	$_dir/$pmda_name"
		args="-i $port $args"
		break
	    elif [ "X$ans" = XUnix ]
	    then
		echo "Unix FIFO name? \c"
		read fifo
		if [ "X$fifo" = X ]
		then
		    echo "Must provide a name, please try again"
		else
		    type="socket	unix $fifo	$_dir/$pmda_name"
		    args="-u $fifo $args"
		    break
		fi
	    else
		echo "Must choose one of \"Unix\" or \"Internet\", please try again"
	    fi
	done
    fi
}

# Function to filter pminfo output
#
__filter()
{
    awk '
NF==0		{ next }
/ value /	{ value++; next }
/^'$1'/		{ metric++
		  if ($0 !~ /:/) next
		}
		{ warn++ }
END		{ if (warn) printf "%d warnings, ",warn
		  printf "%d metrics and %d values\n",metric,value
		}'
}

_setup()
{
    # check the user is root 
    #
    _check_userroot

    # Set $domain and $SYMDOM from domain.h
    #
    __check_domain

    case $prog
    in
	*Install*)
	    # Check that $ROOT is not set, we have a default domain value and
	    # choose the installation mode (collector, monitor or both)
	    #
	    __check_root
	    __choose_mode
	    ;;
    esac

    # some more configuration controls
    pmns_name=${pmns_name-$iam}
    pmda_name=pmda${iam}
    dso_name=pmda_${iam}.so
    dso_entry=${iam}_init
    pmda_dir=${PMDADIR}/${iam}
}

# Configurable PMDA installation
#
# before calling _install,
# 1. set $iam
# 2. set one or both of $dso_opt or $daemon_opt true (optional, $daemon_opt
#    is true by default)
# 3. if $dso_opt set one or both of $pipe_opt or $socket_opt true (optional,
#    $pipe_opt is true by default)
# 4. if $socket_opt and there is an default Internet socket, set
#    $socket_inet_def

_install()
{
    if [ -z "$iam" ]
    then
	echo 'Botch: must define $iam before calling _install_preamble()'
	exit 1
    fi

    if $do_pmda
    then
	if $dso_opt || $daemon_opt
	then
	    :
	else
	    echo 'Botch: must set at least one of $dso_opt or $daemon_opt to "true"'
	    exit 1
	fi
	if $daemon_opt
	then
	    if $pipe_opt || $socket_opt
	    then
		:
	    else
		echo 'Botch: must set at least one of $pipe_opt or $socket_opt to "true"'
		exit 1
	    fi
	fi
    fi

    echo "Updating the Performance Metrics Name Space (PMNS) ..."

    # Install the namespace
    #

    for __n in $pmns_name
    do
	if pminfo -n $NAMESPACE $__n >/dev/null 2>&1
	then
            cd $PMNSDIR
	    if pmnsdel -n $PMNSROOT $__n >$tmp 2>&1
	    then
		killall -HUP pmcd
	    else
		if grep 'Non-terminal "'"$__n"'" not found' $tmp >/dev/null
		then
		    :
		elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp >/dev/null
		then
		    :
		else
		    echo "$prog: failed to delete \"$__n\" from the PMNS"
		    cat $tmp
		    exit 1
		fi
	    fi
            cd $__here
	fi

	# Put the default domain number into the namespace file
	#
	# If there is only one namespace, then the pmns file will
	# be named "pmns".  If there are multiple metric trees,
	# subsequent pmns files will be named "pmns.<metricname>"
	#
	# the string "pmns" can be overridden by the Install/Remove
	# scripts by altering $pmns_source
	#
	if [ "$__n" = "$iam" -o "$__n" = "$pmns_name" ]
	then
	    __s=$pmns_source
	else
	    __s=$pmns_source.$__n
	fi
	sed -e "s/$SYMDOM:/$domain:/" <$__s >$PMNSDIR/$__n

        cd $PMNSDIR
	if pmnsadd -n $PMNSROOT $__n
	then
	    killall -HUP pmcd
	else
	    echo "$prog: failed to add the PMNS entries for \"$__n\" ..."
	    echo
	    ls -l
	    exit 1
	fi
        cd $__here
    done

    if [ -d /var/pcp/config/pmchart ]
    then
	echo "Installing pmchart view(s) ..."
	for __i in *.pmchart
	do
	    if [ "$__i" != "*.pmchart" ]
	    then
		__dest=/var/pcp/config/pmchart/`basename $__i .pmchart`
		rm -f $__dest
		cp $__i $__dest
	    fi
	done
    else
	echo "Skip installing pmchart view(s) ... no \"/var/pcp/config/pmchart\" directory"
    fi

    # Prompt for command line arguments to the PMDA
    #
    if $do_pmda
    then
	# Select an IPC method for communication between PMCD and PMDA
	#
	pmda_type=''
	$dso_opt && pmda_type=dso
	$daemon_opt && pmda_type=daemon
	$dso_opt && $daemon_opt && pmda_type=''
	if [ -z "$pmda_type" ]
        then
	    while true
	    do
		echo "Install $iam as a daemon or dso agent? [daemon] \c"
		read pmda_type
		if [ "X$pmda_type" = Xdaemon -o "X$pmda_type" = X ]
		then
		    pmda_type=daemon
		    break
		elif [ "X$pmda_type" = Xdso ]
		then
		    break
		else
		    echo "Must choose one of \"daemon\" or \"dso\", please try again"
		fi
	    done
	fi
	if [ "$pmda_type" = daemon ]
	then
	    __choose_ipc $pmda_dir
	    args="-d $domain $args"
	else
	    type="dso	$dso_entry	$dso_name"
	    args=""
	fi

	# Fix domain in help for instance domains (if any)
	#
	if [ -f $help_source ]
	then
	    help_version=1
	    case $pmda_interface
	    in
		2|3)	# PMDA_INTERFACE_2 or PMDA_INTERFACE_3
			help_version=2
			;;
	    esac
	    sed -e "/^@ $SYMDOM\./s/$SYMDOM\./$domain./" <$help_source \
	    | newhelp -v $help_version -o $help_source
	fi

	# Terminate old PMDA
	#
	echo "Terminate PMDA if already installed ..."
	__pmda_cull $iam $domain

	# Install binaries
	#
	echo "Installing files ..."
	if [ "$pmda_type" = dso ]
	then
	    STYLE=`/usr/pcp/bin/pmobjstyle`
	else
	    STYLE=""
	fi

	if make STYLE="$STYLE" install
	then
	    :
	else
	    echo "$prog: Arrgh, \"make install\" failed!"
	    exit 1
	fi

	# Add PMDA to pmcd's configuration file
	#
	echo "Updating the PMCD control file, and notifying PMCD ..."
	__pmda_add "$iam	$domain	$type $args"

	# Check that the agent is running OK
	#
	if $do_check
	then
	    [ "$check_delay" -gt 5 ] && echo "Wait $check_delay seconds for the $iam agent to initialize ..."
	    sleep $check_delay
	    for __n in $pmns_name
	    do
		echo "Check $__n metrics have appeared ... \c"
		pminfo -n $NAMESPACE -f $__n | __filter $__n
	    done
	fi
    else
	echo "Skipping PMDA install and PMCD re-configuration"
    fi
}

_remove()
{
    # Update the namespace
    #

    echo "Culling the Performance Metrics Name Space ..."
    cd $PMNSDIR

    for __n in $pmns_name
    do
	echo "$__n ... \c"
	if pmnsdel -n $PMNSROOT $__n >$tmp 2>&1
	then
	    rm -f $PMNSDIR/$__n
	    killall -HUP pmcd
	    echo "done"
	else
	    if grep 'Non-terminal "'"$__n"'" not found' $tmp >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    else
		echo "error"
		cat $tmp
		exit
	    fi
	fi
    done

    # Remove the PMDA and help files
    #
    cd $__here

    if $do_pmda
    then
	echo "Updating the PMCD control file, and notifying PMCD ..."
	__pmda_cull $iam $domain

	echo "Removing files ..."
	make clobber >/dev/null
	for __i in *.pmchart
	do
	    if [ "$__i" != "*.pmchart" ]
	    then
		__dest=/var/pcp/config/pmchart/`basename $__i .pmchart`
		rm -f $__dest
	    fi
	done

	if $do_check
	then
	    for __n in $pmns_name
	    do
		echo "Check $__n metrics have gone away ... \c"
		if pminfo -n $NAMESPACE -f $__n >$tmp 2>&1
		then
		    echo "Arrgh, something has gone wrong!"
		    cat $tmp
		else
		    echo "OK"
		fi
	    done
	fi
    else
	echo "Skipping PMDA removal and PMCD re-configuration"
    fi
}

_check_userroot()
{
    eval `id | sed -e 's/(.*//'`
    if [ "$uid" -ne 0 ]
    then
	echo "Error: You must be root (uid 0) to update the PCP collector configuration."
	exit 1
    fi
}

# preferred public interfaces
#
pmdaSetup()
{
    _setup
}

pmdaChooseConfigFile()
{
    _choose_configfile
}

pmdaInstall()
{
    _install
}

pmdaRemove()
{
    _remove
}
