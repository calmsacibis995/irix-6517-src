#!/bin/sh
#
#	Generate identification strings
#
# $Revision: 3.21 $

file=./bwhen.h

if [ "$1" != "" -a "$ALPHANUM" != "" ]
then
	if [ "$ALPHANUM" = "MR" ] ; then
		rel="$1"
	elif [ $PRODUCT != "SN0XXL" ]
	then
		rel="$1-ALPHA-$ALPHANUM"
	else
		ALPHANUM=`expr "$ALPHANUM" + 1`
		rel="$1-ALPHA-$ALPHANUM"	
	fi
else
	# help identify development kernels
	who=`echo $USER`
	# set $RELEASE, if file present and normal form
	eval `grep RELEASE= $ROOT/usr/include/make/releasedefs 2>/dev/null`
	if [ -n "$BWHEN_REL" ]
		then rel="$BWHEN_REL"
	elif [ -s $WORKAREA/.version_number ]
		then rel="`cat $WORKAREA/.version_number`"
		rel="$RELEASE-${who}-${rel}"
	else
		rel=`basename $ROOT`
		rel="$RELEASE-${who}-${rel}"
	fi
	if [ -n "$PRODUCT" ]
		then rel="$rel-$PRODUCT"
	fi
	if [ -n "$KDEBUG" ]
		then rel="$rel-DEBUG"
	fi
fi

eval `grep RELEASE_NAME= $ROOT/usr/include/make/releasedefs 2>/dev/null`

echo "#define BWHEN_REL		\"$rel\"" > $file
echo "#define BWHEN_VER		\"`date +%m%d%H%M`\"" >> $file
echo "#define BWHEN_RELNAME	\"$RELEASE_NAME\"" >> $file
