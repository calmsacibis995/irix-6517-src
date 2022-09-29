#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)dirname:dirname.sh	1.6.1.2"

for i in $*
do
        case $i in
        --)     shift; break;;
        esac
done

if [ $# -gt 1 ]
then
	catalog=uxcore
	label=UX:dirname
	pfmt -l $label -g $catalog:1 "Incorrect usage\\n"
	pfmt -l $label -g $catalog:3 -s action "Usage: dirname [ path ]\\n"
	exit 1
fi
#	First check for pathnames of form //*non-slash*/* in which case the 
#	dirname is /.
#	Otherwise, remove the last component in the pathname and slashes 
#	that come before it.
#	If nothing is left, dirname is "."
ans=`/usr/bin/expr \
	"${1:-.}/" : '\(/\)/*[^/]*//*$' `
if [ -n "$ans" ];then
	echo $ans
else
	ans=`/usr/bin/expr \
		"${1:-.}/" : '\(.*[^/]\)//*[^/][^/]*//*$' `
	if [ -n "$ans" ];then
		echo $ans
	else
		echo "."
	fi
fi
exit 0
