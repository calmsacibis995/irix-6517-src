#! /sbin/sh -e
#Tag 0x00000800

# Reboot the system--stop the system and leave it 'in the PROM'
# halt [-p]
#   -p requests that the machine powerdown, if supported (Indigo2 & Indy).
#
# $Revision: 1.11 $"

# I18N
# ----------------------------------------------------------------------------
ask() {
#! ask msg default
#!  default should be 0 for "no", "yes" otherwise
#!  returns 0 for "no", otherwise "yes"
#!  because this script is using the -e shell option
#!    ask returns it's status via the 'ret' variable.

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

return 0
} # End of ask

ret=0
msgfile=uxsgicore
# ----------------------------------------------------------------------------

poweroff=
case $1 in
	-p )
		poweroff=-p
		;;
esac

if test -n "$REMOTEHOST"
then
	hostn=`hostname -s`
	fmt=`gettxt ${msgfile}:721 "Halt %s ? "`
	str=`printf "${fmt}" "${hostn}"`
	ask "${str}" 0
	if [ ${ret} -eq 0 ]
	then
		echo `gettxt ${msgfile}:722 "'Halt' cancelled."`
		exit 0
	fi
fi

/etc/shutdown -y -g0 -i0 $poweroff

sleep 10 2> /dev/null
