#! /bin/sh
#
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/ckpacct.sh,v 1.6 1993/11/05 04:24:00 jwag Exp $"

#       "periodically check the size of /var/adm/pacct"
#       "if over $1 blocks (500) default, execute turnacct switch"
#       "should be run as adm"

PATH=/usr/lib/acct:/bin:/usr/bin:/etc
trap "rm -f /var/adm/cklock*; exit 0" 0 1 2 3 9 15
export PATH

_max=${1-250}
_MIN_BLKS=500
cd /var/adm

#	set up lock files to prevent simultaneous checking

cp /dev/null cklock
chmod 400 cklock
ln cklock cklock1 > /dev/null 2>&1
if test $? -ne 0 ; then exit 1; fi

#	check to see if accounting enabled.  If "accton pacct"
#	exits 1, then accounting was enabled.

if [ ! -r pacct ]
then
	echo "" > pacct		#	pacct file is created
fi

accton pacct 2> /dev/null	#	don't print potential error msg
			 	#	to stderr
ACCTSTAT=$?
if test ${ACCTSTAT} -eq 0
then	     
	 accton  #	accting was disabled. Disable it again
fi

#	If there are less than $_MIN_BLKS free blocks left on the /var
#	file system, turn off the accounting (unless things improve
#	the accounting wouldn't run anyway).  If something has
#	returned the file system space, restart accounting.  This
#	feature relies on the fact that ckpacct is kicked off by the
#	cron at least once per hour.
sync;sync;

# version for changed df: default is back to blocks not KB!
# Also check /var/adm explicitly, could be a different filesystem from /usr

_blocks=`df /var/adm |awk '/avail/ {next} /.*/ {print $5}'`

if [ "$_blocks" -lt $_MIN_BLKS   -a  -f /tmp/acctoff ];then
	echo "ckpacct: /var/adm still low on space ($_blocks blks); \c"
	echo "acctg still off"
	( echo "ckpacct: /var/adm still low on space ($_blocks blks); \c"
	echo "acctg still off" ) | mail root adm
	exit 1
elif [ "$_blocks" -lt $_MIN_BLKS ];then
	if test ${ACCTSTAT} -ne 0; then
		echo "ckpacct: /var/adm too low on space ($_blocks blks); \c"
		echo "turning acctg off"
		( echo "ckpacct: /var/adm too low on space ($_blocks blks); \c"
		echo "turning acctg off" ) | mail root adm
		nulladm /tmp/acctoff
		turnacct off
		exit 1
	fi
elif [ -f /tmp/acctoff ];then
	echo "ckpacct: /var/adm free space restored; turning acctg on"
	echo "ckpacct: /var/adm free space restored; turning acctg on" | \
		mail root adm
	rm /tmp/acctoff
	turnacct on
fi

_cursize="`du -s pacct | sed 's/	.*//'`"
if [ "${_max}" -lt "${_cursize}" ]; then
	turnacct switch
fi
