#! /sbin/sh
#Tag 0x00000f00
#ident	"$Revision: 1.34 $"

#	Sequence performed to change the init stat of a machine.

#	This procedure checks to see if you are permitted and allows an
#	interactive shutdown.  The actual change of state, killing of
#	processes and such are performed by the new init state, say 0,
#	and its /etc/rc0.

#	Usage:  shutdown [ -y ] [ -g<grace-period> ] [ -i<init-state> ] [ -p ]
#	-p requests that the machine powerdown, if supported (Indigo^2).
#	Ignored  if init-state is not 0.

# same path as default superuser path in /etc/default/login
# guards against getting wrong versions of programs/scripts
# for commands invoked by relative name
PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/etc:/usr/etc:/usr/bin/X11

# I18N
# ----------------------------------------------------------------------------
ask() {
#! ask msg default
#!  default should be 0 for "no", "yes" otherwise
#!  returns 0 for "no", otherwise "yes"

yesstr=`gettxt ${msgfile}:171 "yes"`
nostr=`gettxt ${msgfile}:172 "no"`
choicefmt=`gettxt ${msgfile}:169 "(%s/%s)"`
defaultfmt=`gettxt ${msgfile}:170 "[%s]"`
shortyes=`echo $yesstr | cut -c1`
shortno=`echo $nostr | cut -c1`

fmt=${choicefmt}${defaultfmt}
if [ $2 -eq 0 ]
then
	str=`printf "${choicefmt}${defaultfmt} : " "$yesstr" "$nostr" "$nostr"`
else
	str=`printf "${choicefmt}${defaultfmt} : " "$yesstr" "$nostr" "$yesstr"`
fi
printf "%s%s" "$1" "$str"

read ans

# convert answer to lowercase
ans=`echo $ans | tr '[A-Z]' '[a-z]'`

case "$ans" in
	${yesstr}*)	ret=1
			;;
	${nostr}*)	ret=0
			;;
	${shortyes}*)	ret=1
			;;
	${shortno}*)	ret=0
			;;
	*)		ret=$2
			;;
esac

return ${ret}
} # End of ask

msgfile=uxsgicore
# ----------------------------------------------------------------------------

#	Check the user id.
if [ -x /usr/bin/id ]
then
	eval `id  |  sed 's/[^a-z0-9=].*//'`
	if [ "${uid:=0}" -ne 0 ]
	then
	        printf "`gettxt ${msgfile}:710 '%s:  Only root can run /etc/shutdown.'`\n" $0
		exit 2
	fi
fi

grace=60
askconfirmation=yes
initstate=0
while [ $# -gt 0 ]
do
	case $1 in
	-g[0-9]* )
		grace=`expr "$1" : '-g\(.*\)'`
		;;
	-i[Ss016] )
		initstate=`expr "$1" : '-i\(.*\)'`
		;;
	-i[2345] )
		initstate=`expr "$1" : '-i\(.*\)'`
		printf "`gettxt ${msgfile}:711 '%s:  Initstate $initstate is not for system shutdown'`\n" $0
		exit 1
		;;
	-y )
		askconfirmation=
		;;
	-p )
		poweroff=yes
		;;
	-* )
		printf "`gettxt ${msgfile}:712 'Illegal flag argument \'%s\''`\n" $1
		exit 1
		;;
	* )
		printf "`gettxt ${msgfile}:713 'Usage:  %s [ -y ] [ -g<grace> ] [ -i<initstate> ]'`\n" $0
		exit 1
	esac
	shift
done

#
# ask shutdown reason as soon as possible
#
/usr/etc/amsdreasons -i ${initstate}

#
# IRIX does not provide /etc/bupscd. However, the following lines could
# be re-enabled by VARs who do (e.g. Sinix)
#
# if [ -n "${askconfirmation}" -a -x /etc/ckbupscd ]
# then
# 	#	Check to see if backups are scheduled at this time
# 	BUPS=`/etc/ckbupscd`
# 	if [ "$BUPS" != "" ]
# 	then
# 		echo "$BUPS"
# 		ask "`gettxt ${msgfile}:714 'Do you wish to abort this shutdown and return to \ncommand level to do these backups ? '`" 0
# 		if [ $? -ne 0 ]		# check for yes
# 		then
# 			exit 1
# 		fi
# 	fi
# fi
# 

if [ -z "${TZ}"  -a  -r /etc/TIMEZONE ]
then
	. /etc/TIMEZONE
fi

printf "%s" "`gettxt ${msgfile}:715 'Shutdown started.     '`"
date

sync
cd /

trap "exit 1"  1 2 15

a="`who  |  wc -l`"
if [ ${a} -gt 1  -a  ${grace} -gt 0 ]
then
	if [ -x /sbin/wall ]
	then
		printf "`gettxt ${msgfile}:716 'The system will be shut down in %s seconds.\nPlease log off now.\n'`" "${grace}"| \
			/sbin/wall &
	fi
	sleep ${grace}
fi

if [ -x /sbin/wall ]
then
	echo `gettxt ${msgfile}:717 "THE SYSTEM IS BEING SHUT DOWN! Log off now."` | /sbin/wall &
fi

if [ ${grace} -gt 60 ]
then
	sleep 60
else
	sleep ${grace}
fi

if [ ${askconfirmation} ]
then
	ask "`gettxt ${msgfile}:718 'Do you want to continue with the shutdown ? '`" 0
	b=$?
else
	b=1
fi

if [ $b -eq 0 ]
then
	if [ -x /sbin/wall ]
	then
		echo `gettxt ${msgfile}:719 "False Alarm:  The system will not be brought down."` | /sbin/wall
	fi
	echo `gettxt ${msgfile}:720 'Shut down aborted.'`
	exit 1
fi
if [ "$poweroff" = yes -a "$initstate" = "0" ]
then
:
	/sbin/uadmin 256 9 # set poweroff flag
fi

/sbin/init ${initstate}
