#!/bin/sh
#Tag 0x00010D13
#
# Copyright 1998, Silicon Graphics, Inc.
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
#
# $Id: pmie_check.sh,v 1.5 1999/05/28 02:21:17 kenmcd Exp $
#
# Administrative script to check pmie processes are alive, and restart
# them as required.
#

# Some degree of paranoia here ...
PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/pcp/bin
export PATH

PMIE=pmie
PMPOST=pmpost
PMIECONF=pmieconf

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
tmp=/tmp/$$
status=0
echo >$tmp.lock
trap "rm -f \`[ -f $tmp.lock ] && cat $tmp.lock\` $tmp.*; exit \$status" 0 1 2 3 15
prog=`basename $0`

# control file for pmie administration ... edit the entries in this
# file to reflect your local configuration
#
CONTROL=/var/pcp/config/pmie/control

# determine real name for localhost
LOCALHOSTNAME="localhost"
[ -x /usr/bsd/hostname ] && LOCALHOSTNAME=`/usr/bsd/hostname`

# determine whether SGI Embedded Support Partner events need to be used
CONFARGS="-F"
[ -x /usr/sbin/esplogger ] && CONFARGS='m global syslog_prefix $esp_prefix$'

# option parsing
#
SHOWME=false
MV=mv
RM=rm
CP=cp
KILL=kill
VERBOSE=false
VERY_VERBOSE=false
START_PMIE=true
usage="Usage: $prog [-NsV] [-c control]"
while getopts c:NsV? c
do
    case $c
    in
	c)	CONTROL="$OPTARG"
		;;
	N)	SHOWME=true
		MV="echo + mv"
		RM="echo + rm"
		CP="echo + cp"
		KILL="echo + kill"
		;;
	s)	START_PMIE=false
		;;
	V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	?)	echo "$usage"
		status=1
		exit
		;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 0 ]
then
    echo "$usage"
    status=1
    exit
fi

_error()
{
    echo "$prog: [$CONTROL:$line]"
    echo "Error: $1"
    echo "... automated performance reasoning for host \"$host\" unchanged"
    touch $tmp.err
}

_warning()
{
    echo "$prog [$CONTROL:$line]"
    echo "Warning: $1"
}

_message()
{
    case $1 in
      'restart')
	 echo "Restarting pmie for host \"$host\" ...\c"
      ;;
    esac
}

_lock()
{
    # demand mutual exclusion
    #
    fail=true
    rm -f $tmp.stamp
    for try in 1 2 3 4
    do
	if pmlock -v $logfile.lock >$tmp.out
	then
	    echo $logfile.lock >$tmp.lock
	    fail=false
	    break
	else
	    if [ ! -f $tmp.stamp ]
	    then
		touch -t `pmdate -30M %Y%m%d%H%M` $tmp.stamp
	    fi
	    if [ -n "`find $logfile.lock ! -newer $tmp.stamp -print 2>/dev/null`" ]
	    then
		_warning "removing lock file older than 30 minutes"
		ls -l $logfile.lock
		rm -f $logfile.lock
	    fi
	fi
	sleep 5
    done

    if $fail
    then
	# failed to gain mutex lock
	#
	if [ -f $logfile.lock ]
	then
	    _warning "is another PCP cron job running concurrently?"
	    ls -l $logfile.lock
	else
	    echo "$prog: `cat $tmp.out`"
	fi
	_warning "failed to acquire exclusive lock ($logfile.lock) ..."
	continue
    fi
}

_unlock()
{
    rm -f $logfile.lock
    echo >$tmp.lock
}

_check_logfile()
{
    if [ ! -f $logfile ]
    then
	echo "$prog: Error: cannot find pmie output file at \"$logfile\""
	logdir=`dirname $logfile`
	echo "Directory (`cd $logdir; /usr/bin/pwd`) contents:"
	ls -la $logdir
    else
	echo "Contents of pmie output file \"$logfile\" ..."
	cat $logfile
    fi
}

_check_pmie_version()
{
    # the -C option was introduced at the same time as the /var/tmp/pmie
    # stats file support (required for pmie_check), so if this produces a
    # non-zero exit status, bail out
    # 
    if $PMIE -C /dev/null >/dev/null 2>&1
    then
	:
    else
	binary=`which $PMIE`
	echo "$prog: Error: wrong version of $binary installed"
	cat - <<EOF
You seem to have mixed versions in your Performance Co-Pilot (PCP)
installation, such that pmie(1) is incompatible with pmie_check(1).
EOF
	echo
	showfiles pcp\* \
	| nawk '/bin\/pmie$/ {print $4}' >$tmp.subsys

	if [ -s $tmp.subsys ]
	then
	    echo "Currently $binary is installed from these subsystem(s):"
	    echo
	    versions `cat $tmp.subsys` </dev/null
	    echo
	else
	    echo "The current installation history does not identify the subsystem(s)"
	    echo "where $binary was installed from."
	    echo
	fi

	case `uname -r`
	in
	    6.5*)
		echo "Please upgrade pcp_eoe from the IRIX 6.5.5 (or later) distribution."
		;;
	    *)
		echo "Please upgrade pcp_eoe from the PCP 2.1 (or later) distribution."
		;;
	esac

	status=1
	exit
    fi
}

_check_pmie()
{
    $VERBOSE && echo " [process $1] \c"

    # wait until pmie process starts, or exits
    #
    delay=5
    [ ! -z "$PMCD_CONNECT_TIMEOUT" ] && delay=$PMCD_CONNECT_TIMEOUT
    x=5
    [ ! -z "$PMCD_REQUEST_TIMEOUT" ] && x=$PMCD_REQUEST_TIMEOUT

    # wait for maximum time of a connection and 20 requests
    #
    delay=`expr $delay + 20 \* $x`
    i=0
    while [ $i -lt $delay ]
    do
	$VERBOSE && echo ".\c"
	if [ -f $logfile ]
	then
	    # $logfile was previously removed, if it has appeared again then
	    # we know pmie has started ... if not just sleep and try again
	    #
	    if ls /var/tmp/pmie/$1 >$tmp.out 2>&1
	    then
		if grep "No such file or directory" $tmp.out >/dev/null
		then
		    :
		else
		    sleep 5
		    $VERBOSE && echo " done"
		    return 0
		fi
	    fi
	    if ps -e | grep "^ *$1 " >/dev/null
	    then
		:
	    else
		$VERBOSE || _message restart
		echo " process exited!"
		echo "$prog: Error: failed to restart pmie"
		echo "Current pmie processes:"
		ps -ef | sed -n -e 1p -e "/$PMIE/p"
		echo
		_check_logfile
		return 1
	    fi
	fi
	sleep 5
	i=`expr $i + 5`
    done
    $VERBOSE || _message restart
    echo " timed out waiting!"
    sed -e 's/^/	/' $tmp.out
    _check_logfile
    return 1
}

if $START_PMIE
then
    # ensure we have a pmie binary which supports the features we need
    # 
    _check_pmie_version
else
    # if pmie has never been started, there's no work to do to stop it
    # 
    [ ! -d /var/tmp/pmie ] && exit
    $PMPOST "stop pmie from $prog"
fi

if [ ! -f $CONTROL ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

#  1.0 is the first release, and the version is set in the control file
#  with a $version=x.y line
#
version=1.0
eval `grep '^version=' $CONTROL | sort -rn`
if [ $version != "1.0" ]
then
    _error "unsupported version (got $version, expected 1.0)"
    status=1
    exit
fi

echo >$tmp.dir
rm -f $tmp.err $tmp.pmies

line=0
cat $CONTROL \
    | sed -e "s/LOCALHOSTNAME/$LOCALHOSTNAME/g" \
    | while read host socks logfile args
do
    line=`expr $line + 1`
    case "$host"
    in
	\#*|'')	# comment or empty
		continue
		;;
	\$*)	# in-line variable assignment
		$SHOWME && echo "# $host $socks $logfile $args"
		cmd=`echo "$host $socks $logfile $args" \
		     | sed -n \
			 -e "/='/s/\(='[^']*'\).*/\1/" \
			 -e '/="/s/\(="[^"]*"\).*/\1/' \
			 -e '/=[^"'"'"']/s/[;&<>|].*$//' \
			 -e '/^\\$[A-Za-z][A-Za-z0-9_]*=/{
s/^\\$//
s/^\([A-Za-z][A-Za-z0-9_]*\)=/export \1; \1=/p
}'`
		if [ -z "$cmd" ]
		then
		    # in-line command, not a variable assignment
		    _warning "in-line command is not a variable assignment, line ignored"
		else
		    case "$cmd"
		    in
			'export PATH;'*)
			    _warning "cannot change \$PATH, line ignored"
			    ;;
			'export IFS;'*)
			    _warning "cannot change \$IFS, line ignored"
			    ;;
			*)
			    $SHOWME && echo "+ $cmd"
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
    esac

    if [ -z "$socks" -o -z "$logfile" -o -z "$args" ]
    then
	_error "insufficient fields in control file record"
	continue
    fi

    [ $VERY_VERBOSE = "true" ] && echo "Check pmie -h $host -l $logfile ..."

    # make sure output directory exists
    #
    dir=`dirname $logfile`
    if [ ! -d $dir ]
    then
	mkdir -p $dir >$tmp.err 2>&1
	if [ ! -d $dir ]
	then
	    cat $tmp.err
	    _error "cannot create directory ($dir) for pmie log file"
	fi
    fi
    [ ! -d $dir ] && continue

    cd $dir
    dir=`/usr/bin/pwd`
    $SHOWME && echo "+ cd $dir"

    if [ ! -w $dir ]
    then
	_warning "no write access in $dir, skip lock file processing"
    else
	_lock
    fi

    # match $logfile and $fqdn from control file to running pmies
    pid=""
    fqdn=`pmhostname $host`
    for file in `ls /var/tmp/pmie`
    do
	p_id=$file
	file="/var/tmp/pmie/$file"
	p_logfile=""
	p_pmcd_host=""
	if [ -f /proc/pinfo/$p_id ]
	then
	    eval `tr '\0' '\012' < $file | sed -e '/^$/d' | sed -e 3q | nawk '
NR == 2	{ printf "p_logfile=\"%s\"\n", $0; next }
NR == 3	{ printf "p_pmcd_host=\"%s\"\n", $0; next }
	{ next }'`
	    if [ "$p_logfile" = $logfile -a "$p_pmcd_host" = $fqdn ]
	    then
		pid=$p_id
		break
	    fi
	else
	    # ignore, its not a running process
	    eval $RM -f $file
	fi
    done

    if $VERY_VERBOSE
    then
	if [ -z "$pid" ]
	then
	    echo "No current pmie process exists for:"
	else
	    echo "Found pmie process $pid monitoring:"
	fi
	echo "    host = $fqdn\n    log file = $logfile"
    fi

    if [ -z "$pid" -a $START_PMIE = true ]
    then
	configfile=`echo $args | sed -n -e 's/^/ /' -e 's/[ 	][ 	]*/ /g' -e 's/-c /-c/' -e 's/.* -c\([^ ]*\).*/\1/p'`
	if [ ! -z "$configfile" ]
	then
	    # if this is a relative path and not relative to cwd,
	    # substitute in the default pmie search location.
	    #
	    if [ ! -f "$configfile" -a \
			"`basename $configfile`" = "$configfile" ]
	    then
		configfile="/var/pcp/config/pmie/$configfile"
	    fi
	    case "`file $configfile`"
	    in
		*'No such file or directory')
			# generate it, if possible
			# 
			if $PMIECONF -f $configfile $CONFARGS >$tmp.diag 2>&1
			then
			    :
			else
			    _warning "pmieconf failed to generate \"$configfile\""
			fi
			;;
		*'PCP pmie config'*)
			# pmieconf file, see if re-generation is needed
			#
			cp $configfile $tmp.pmie
			if $PMIECONF -f $tmp.pmie $CONFARGS >$tmp.diag 2>&1
			then
			    grep -v "generated by pmieconf" $configfile >$tmp.old
			    grep -v "generated by pmieconf" $tmp.pmie >$tmp.new
			    if diff $tmp.old $tmp.new >/dev/null
			    then
				:
			    else
				if [ -w $configfile ]
				then
				    $VERBOSE && echo "Reconfigured: \"$configfile\" (pmieconf)"
				    eval $CP $tmp.pmie $configfile
				else
				    _warning "no write access to pmieconf file \"$configfile\", skip reconfiguration"
				fi
			    fi
			else
			    _warning "pmieconf failed to reconfigure \"$configfile\""
			    sed -e "s;$tmp.pmie;$configfile;g" $tmp.diag
			fi
			;;
		*)	;;
	    esac
	fi

	args="-h $host -l $logfile $args"

	$VERBOSE && _message restart
	sock_me=''
	[ "$socks" = y ] && sock_me='pmsocks '
	[ -f $logfile ] && eval $MV -f $logfile $logfile.prior

	if $SHOWME
	then
	    $VERBOSE && echo
	    echo "+ ${sock_me}$PMIE -b $args"
	    _unlock
	    continue
	else
	    # since this is launched as a sort of daemon, any output should
	    # go on pmie's stderr, i.e. $logfile ... use -b for this
	    #
	    $VERY_VERBOSE && ( echo; echo "+ ${sock_me}$PMIE -b $args"; echo "...\c" )
	    $PMPOST "start pmie from $prog for host $host"
	    ${sock_me}$PMIE -b $args &
	    pid=$!
	fi

	# wait for pmie to get started, and check on its health
	_check_pmie $pid

    elif [ ! -z "$pid" -a $START_PMIE = false ]
    then
	# Send pmie a SIGTERM, which is noted as a pending shutdown.
	# Add pid to list of pmies sent SIGTERM - may need SIGKILL later.
	#
	$VERY_VERBOSE && echo "+ $KILL -TERM $pid"
	eval $KILL -TERM $pid
	echo "$pid \c" >> $tmp.pmies	
    fi

    _unlock
done

# check all the SIGTERM'd pmies really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
elif [ $START_PMIE = false -a -s $tmp.pmies ]
then
    pmielist=`cat $tmp.pmies`
    if ps -p "$pmielist" >/dev/null 2>&1
    then
	$VERY_VERBOSE && ( echo; echo "+ $KILL -KILL `cat $tmp.pmies` ...\c" )
	eval $KILL -KILL $pmielist >/dev/null 2>&1
	sleep 3		# give them a chance to go
	if ps -f -p "$pmielist" >$tmp.alive 2>&1
	then
	    echo "$prog: Error: pmie process(es) will not die"
	    cat $tmp.alive
	    status=1
	fi
    fi
fi

[ -f $tmp.err ] && status=1
exit
