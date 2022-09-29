#!/bin/sh
#
#ident "$Revision: 1.22 $"
#
# script run at the time of installation.
#

AMRVERSIONNUM=2.1
AVAILDIR=$rbase/var/adm/avail
SAVEDIR=$AVAILDIR/.save
CONFIGDIR=$AVAILDIR/config
USRETCDIR=/usr/etc

OPTIONS=`cat /etc/config/savecore.options 2> /dev/null`
CRASHDIR=""
if [ "$OPTIONS" != "" ] ; then
    for DIR in $OPTIONS ; do
	if [ -d "$DIR" ] ; then
	    CRASHDIR=$DIR
	fi
    done
    if [ "$CRASHDIR" = "" ] ; then
	CRASHDIR=/var/adm/crash
    fi
else
    CRASHDIR=/var/adm/crash
fi

AMCONFIG=$USRETCDIR/amconfig
AMSYSINFO=$USRETCDIR/amsysinfo
AMNOTIFY=$USRETCDIR/amnotify
AMTIME1970=$USRETCDIR/amtime1970
AMCONVERT=$USRETCDIR/amconvert
AMSYSLOG=$USRETCDIR/amsyslog

LOGFILE=$AVAILDIR/availlog
OLOGFILE=$AVAILDIR/oavaillog
AUTOEMAILLIST=$CONFIGDIR/autoemail.list

SINCE1970=`$AMTIME1970`
SINCE1970DATE=`$AMTIME1970 -d $SINCE1970`

AVAILREPFILE=$CRASHDIR/availreport
PLATFORM=`uname -m`
SERIALNUM=""

if [ -f $LOGFILE ] ; then
    if [ ! -d $SAVEDIR ] ; then
	mv $LOGFILE $LOGFILE.O
    fi
fi

rm -f /etc/init.d/avail /etc/rc0.d/K05avail /etc/rc2.d/S95avail /etc/config/avail /etc/config/availmon

#
# move saved sysid to serialnum
#

if [ -f $SAVEDIR/sysid ] ; then
    mv $SAVEDIR/sysid $SAVEDIR/serialnum
fi

#
# convert STOP lines to EVENT lines
# and check support.sw.fru (or eoe.sw.base) and fix EVENT code
#

FRUDATE=""
case $PLATFORM in
IP19 | IP21 | IP25)
    FRUVERSION=`versions support.sw.fru | tail -1`
    if echo "$FRUVERSION" | grep fru > /dev/null ; then
	FRUDATE=`echo $FRUVERSION | awk '{print $3}'`
    fi
    ;;
IP27)
    FRUVERSION=`versions -n eoe.sw.base | tail -1 | awk '{print $3}'`
    if [ $FRUVERSION -ge 1263561140 ] ; then
	FRUDATE=`versions eoe.sw.base | tail -1 | awk '{print $3}'`
    fi
    ;;
esac
if [ "$FRUDATE" != "" ] ; then
    MM=`echo $FRUDATE | cut -d'/' -f1`
    DD=`echo $FRUDATE | cut -d'/' -f2`
    YY=`echo $FRUDATE | cut -d'/' -f3`
    case $MM in
    01)
	MMM="Jan"
	;;
    02)
	MMM="Feb"
	;;
    03)
	MMM="Mar"
	;;
    04)
	MMM="Apr"
	;;
    05)
	MMM="May"
	;;
    06)
	MMM="Jun"
	;;
    07)
	MMM="Jul"
	;;
    08)
	MMM="Aug"
	;;
    09)
	MMM="Sep"
	;;
    10)
	MMM="Oct"
	;;
    11)
	MMM="Nov"
	;;
    12)
	MMM="Dec"
	;;
    esac
    if [ $YY -ge 69 ] ; then
	YYYY="19$YY"
    else
	YYYY="20$YY"
    fi
    FRUTIME=`$AMTIME1970 -t xxx $MMM $DD 00:00:00 $YYYY`
    AMCONVERTARG="-t $FRUTIME"
else
    AMCONVERTARG=""
fi

if [ -f $OLOGFILE ] ; then
    mv $OLOGFILE $OLOGFILE.orig
    $AMCONVERT -o $OLOGFILE.orig -n $OLOGFILE $AMCONVERTARG
fi
if [ -f $LOGFILE ] ; then
    mv $LOGFILE $LOGFILE.orig
    $AMCONVERT -o $LOGFILE.orig -n $LOGFILE $AMCONVERTARG
fi

#
# add pager report entry if it is not in autoemail.list1
#

grep "pager(concise text):" $AUTOEMAILLIST > /dev/null
if [ $? -ne 0 ] ; then
    echo "#\npager(concise text):" >> $AUTOEMAILLIST
fi

#
# add hinv.sort in saved directory for the hinv bug that
# cannot generate output in a special order
#

if [ ! -f $SAVEDIR/hinv.sort ] ; then
    sort $SAVEDIR/hinv > $SAVEDIR/hinv.sort
fi

#
# Remove .backup directory if exists
#

OSVERSION=`uname -r | cut -d"-" -f1`
OSPHASE=`uname -r | cut -d"-" -f2`
BACKUPDIR=$AVAILDIR/.backup
ORIGSAVEDIR=$BACKUPDIR/.save.orig
ORIGCONFDIR=$BACKUPDIR/config.orig

if [ "$OSPHASE" = "$OSVERSION" ] ; then
    :
elif [ -d "$BACKUPDIR" ] ; then
    if [ -d "$ORIGCONFDIR" ] ; then
	cp -r $ORIGCONFDIR/* $CONFIGDIR
	rm -rf $ORIGCONFDIR
	if [ -d $ORIGSAVEDIR ] ; then
	    rm -rf $ORIGSAVEDIR
        fi
    elif [ -d "$ORIGSAVEDIR" ] ; then
	cp $ORIGSAVEDIR/autoemail $CONFIGDIR
	cp $ORIGSAVEDIR/autoemail.list $CONFIGDIR
    else
	:
    fi
    rm -rf $BACKUPDIR
fi


