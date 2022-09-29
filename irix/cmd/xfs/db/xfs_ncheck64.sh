#!/bin/sh -f
#ident "$Revision: 1.1 $"

OPTS=" "
ISFILE=" "
USAGE="usage: xfs_ncheck64 [-sf] [-i ino]... special"


while getopts "b:fi:sv" c
do
	case $c in
	s)	OPTS=$OPTS"-s ";;
	i)	OPTS=$OPTS"-i "$OPTARG" ";;
	f)	ISFILE=" -f";;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done
shift `expr $OPTIND - 1`
case $# in
	1)	xfs_db64$ISFILE -r -p xfs_ncheck64 -c "blockget -ns" -c "ncheck$OPTS" $1
		status=$?
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit $status
