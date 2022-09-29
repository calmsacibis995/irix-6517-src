#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/lastlogin.sh	1.6.3.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/lastlogin.sh,v 1.4 1999/12/30 19:46:11 liang Exp $"
#	"lastlogin - keep record of date each person last logged in"
#	"bug - the date shown is usually 1 more than it should be "
#	"       because lastlogin is run at 4am and checks the last"
#	"       24 hrs worth of process accounting info (in pacct)"
PATH=/usr/lib/acct:/bin:/usr/bin:/etc
cd /var/adm/acct
if test ! -r sum/loginlog; then
	nulladm sum/loginlog
fi
#	"cleanup loginlog - delete entries of those no longer in"
#	"/etc/passwd and add an entry for those recently added"
#	"line 0 - remove yellow pages markers"
#	"line 1 - get file of current logins in same form as loginlog"
#	"line 2 - merge the 2 files; use uniq to delete common"
#	"lines resulting in those lines which need to be"
#	"deleted or added from loginlog"
#	"line 3 - result of sort will be a file with 2 copies"
#	"of lines to delete and 1 copy of lines that are "
#	"valid; use uniq to remove duplicate lines"
cat /etc/passwd | sed -e '/^+$/d' -e '/^+:/d' -e 's/^+//' |\
sed "s/\([^:]*\).*/00-00-00  \1/" |\
sort +1 - sum/loginlog | uniq -u +10 |\
sort +1 - sum/loginlog |uniq -u > sum/tmploginlog
cp sum/tmploginlog sum/loginlog
#	"update loginlog"
_d="`date +%y-%m-%d`"
_day=`date +%m%d`
#	"lines 1 and 2 - remove everything from the total acctng records"
#	"with connect info except login name and add the date"
#	"lines 3 and 4 - convert the year to 4 digits, sort in reverse order"
#	"by login name, get 1st occurrence of each login name, resort by"
#	"date, and convert the year back to 2 digits"
acctmerg -a < nite/ctacct.$_day | \
 sed -e "s/^[^ 	][^ 	]*[ 	][ 	]*\([^ 	][^ 	]*\)[ 	].*/$_d  \1/" | \
 cat - sum/loginlog | sed -e '/^00-00-00/!s/^0/200/' -e 's/^00/0000/' \
 -e 's/^[789]/19&/' | sort -r +1 | uniq +12 | sort | sed "s/^..//" \
 > sum/tmploginlog
cp sum/tmploginlog sum/loginlog
rm -f sum/tmploginlog
