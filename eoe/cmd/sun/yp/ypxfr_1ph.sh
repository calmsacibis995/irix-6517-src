#! /bin/sh
#
# @(#)ypxfr_1perhour.sh 1.1 86/02/05 Copyr 1985 Sun Microsystems, Inc.  
# @(#)ypxfr_1perhour.sh	2.1 86/04/16 NFSSRC
#
# ypxfr_1perhour.sh - Do hourly NIS map check/updates
#

USAGE="$0: [-vf] [-h host] [c]"

while getopts "vfh:c" c; do
    case $c in
    v)
	if test "$verbose" = "-v"; then
	    set -v
	fi
	set -x
	verbose="-v"
	;;
    f) ARGS="$ARGS -f";;
    h) ARGS="$ARGS -h $OPTARG";;
    c) ARGS="$ARGS -c";;
    \?) echo $USAGE; exit 1;;
    esac
done
shift `expr $OPTIND - 1`
if test "$#" != 0; then
	echo $USAGE
	exit 1
fi

if /sbin/chkconfig ypmaster; then
	exit
fi
if /sbin/chkconfig yp && /sbin/chkconfig ypserv; then :
else
	exit
fi

# The following sleep command is used to stagger NIS servers' ypxfr requests. 
# TXTM must be greater than the time in seconds required to transfer all
# of the databases to a single machine.
# Note that 60*60=0xe10=seconds/hour.

NSVR=`ypcat -k ypservers |wc -l`
TXTM='5*2'			# 2 databases * 5 seconds/database

sleep `(echo ibase=16; hostid | sed -e 's/0x//' -e 'y/abcdef/ABCDEF/' \\
       -e "s/.*/((& % ($NSVR+1))*$TXTM % E10)/") | bc 2>/dev/null`

/usr/sbin/ypxfr $ARGS passwd.byname
/usr/sbin/ypxfr $ARGS passwd.byuid 
