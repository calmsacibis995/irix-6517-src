#!/bin/sh

AVAILDIR=/var/adm/avail
USRETC=/usr/etc
SAVEDIR=$AVAILDIR/.save
CONFIGDIR=$AVAILDIR/config
PREVSTARTFILE=$SAVEDIR/prevstart
AMTIME1970=$USRETC/amtime1970
AMCONVERT=$USRETC/amconvert

#
# Convert PREVSTART first.  All availmon tools need it
#

if [ -f "$PREVSTARTFILE" ] ; then
    PREVSTART=`cat $PREVSTARTFILE | cut -d'|' -f2`
else
    PREVSTART=`cat $AVAILDIR/availlog | tail -1 | cut -d'|' -f2`
fi

echo "$PREVSTART" > $PREVSTARTFILE

if [ -f $SAVEDIR/autoemail -o -f $SAVEDIR/autoemail.list ] ; then
    $AMCONVERT -C 1
else
    $AMCONVERT -C 0
fi

rm -rf $CONFIGDIR/*
