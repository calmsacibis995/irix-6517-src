#!/bin/sh -f
#ident "$Revision: 1.2 $"

OPTS=" "
ISFILE=" "
USAGE="usage: xfs_ncheck [-sf] [-i ino]... special"


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
	1)	xfs_db$ISFILE -r -p xfs_ncheck -c "blockget -ns" -c "ncheck$OPTS" $1
		status=$?
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit $status
